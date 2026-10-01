#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <termios.h>
#include <math.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <X11/X.h>
#include <X11/xproto.h>
#include <X11/cursorfont.h>
#include "font8x16.c"

#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 768
#define MAX_WINDOWS   64
#define MAX_GCS       64
#define MAX_CLIENTS   16

#define ROOT_WINDOW_ID 1
#define COLOR_DESKTOP  0x00336699 /* Classic X11 Slate Blue */
#define COLOR_BLACK    0x00000000
#define COLOR_WHITE    0x00FFFFFF

struct x_window {
    uint32_t wid;
    uint32_t parent;
    int client_fd;
    int x, y;
    int width, height;
    int border_width;
    uint32_t border_pixel;
    uint32_t bg_pixel;
    bool mapped;
    char title[64];
    uint32_t event_mask;
    uint32_t *pixmap; /* Backing store pixmap */
};

struct x_gc {
    uint32_t gid;
    uint32_t fg;
    uint32_t bg;
    uint32_t line_width;
};

struct x_client {
    int fd;
    bool active;
    uint32_t resource_base;
};

static int fb_fd = -1;
static int mouse_fd = -1;
static uint32_t *backbuffer = NULL;

static struct x_window windows[MAX_WINDOWS];
static int window_count = 0;
static uint32_t window_stack[MAX_WINDOWS]; /* Z-order from bottom to top */
static int stack_count = 0;

static struct x_gc gcs[MAX_GCS];
static int gc_count = 0;

static struct x_client clients[MAX_CLIENTS];

static int mouse_x = 512;
static int mouse_y = 384;
static uint8_t mouse_buttons = 0;
static uint32_t focused_window = ROOT_WINDOW_ID;

static bool screen_dirty = true;

/* Standard X11 Cursor Bitmap (16x16) */
static const uint16_t cursor_mask[16] = {
    0x8000, 0xC000, 0xE000, 0xF000,
    0xF800, 0xFC00, 0xFE00, 0xFF00,
    0xFF80, 0xFFC0, 0xFC00, 0xDC00,
    0x8E00, 0x0E00, 0x0700, 0x0300
};

static const uint16_t cursor_shape[16] = {
    0x0000, 0x4000, 0x6000, 0x7000,
    0x7800, 0x7C00, 0x7E00, 0x7800,
    0x4C00, 0x0C00, 0x0600, 0x0600,
    0x0000, 0x0000, 0x0000, 0x0000
};

static struct x_window *find_window(uint32_t wid) {
    for (int i = 0; i < window_count; i++) {
        if (windows[i].wid == wid) return &windows[i];
    }
    return NULL;
}

static struct x_gc *find_gc(uint32_t gid) {
    for (int i = 0; i < gc_count; i++) {
        if (gcs[i].gid == gid) return &gcs[i];
    }
    return NULL;
}

static void stack_raise(uint32_t wid) {
    int idx = -1;
    for (int i = 0; i < stack_count; i++) {
        if (window_stack[i] == wid) {
            idx = i;
            break;
        }
    }
    if (idx >= 0 && idx < stack_count - 1) {
        for (int i = idx; i < stack_count - 1; i++) {
            window_stack[i] = window_stack[i + 1];
        }
        window_stack[stack_count - 1] = wid;
    }
}

static void init_root_window(void) {
    memset(windows, 0, sizeof(windows));
    window_count = 1;
    stack_count = 1;

    struct x_window *root = &windows[0];
    root->wid = ROOT_WINDOW_ID;
    root->parent = 0;
    root->client_fd = -1;
    root->x = 0;
    root->y = 0;
    root->width = SCREEN_WIDTH;
    root->height = SCREEN_HEIGHT;
    root->border_width = 0;
    root->border_pixel = 0;
    root->bg_pixel = COLOR_DESKTOP;
    root->mapped = true;
    root->pixmap = NULL;
    strcpy(root->title, "DUnix Root Window");
    root->event_mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

    window_stack[0] = ROOT_WINDOW_ID;
}

static inline int iabs(int v) {
    return v < 0 ? -v : v;
}

static void draw_pixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        backbuffer[y * SCREEN_WIDTH + x] = color;
    }
}

static void draw_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
    if (y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    for (int r = y; r < y + h; r++) {
        uint32_t *row = &backbuffer[r * SCREEN_WIDTH + x];
        for (int c = 0; c < w; c++) {
            row[c] = color;
        }
    }
}

static void draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = iabs(x2 - x1);
    int dy = iabs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        draw_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

static void draw_rect(int x, int y, int w, int h, uint32_t color) {
    draw_line(x, y, x + w - 1, y, color);
    draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
    draw_line(x, y, x, y + h - 1, color);
    draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
}

/* Window Backing-Store Drawing Functions */
static void win_draw_pixel(struct x_window *win, int x, int y, uint32_t color) {
    if (!win) return;
    if (win->wid == ROOT_WINDOW_ID) {
        draw_pixel(x, y, color);
        return;
    }
    if (win->pixmap && x >= 0 && x < win->width && y >= 0 && y < win->height) {
        win->pixmap[y * win->width + x] = color;
    }
}

static void win_draw_fill_rect(struct x_window *win, int x, int y, int w, int h, uint32_t color) {
    if (!win) return;
    if (win->wid == ROOT_WINDOW_ID) {
        draw_fill_rect(x, y, w, h, color);
        return;
    }
    if (!win->pixmap) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > win->width) w = win->width - x;
    if (y + h > win->height) h = win->height - y;
    if (w <= 0 || h <= 0) return;

    for (int r = y; r < y + h; r++) {
        uint32_t *row = &win->pixmap[r * win->width + x];
        for (int c = 0; c < w; c++) {
            row[c] = color;
        }
    }
}

static void win_draw_line(struct x_window *win, int x1, int y1, int x2, int y2, uint32_t color) {
    if (!win) return;
    int dx = iabs(x2 - x1);
    int dy = iabs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        win_draw_pixel(win, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

static void win_draw_rect(struct x_window *win, int x, int y, int w, int h, uint32_t color) {
    win_draw_line(win, x, y, x + w - 1, y, color);
    win_draw_line(win, x, y + h - 1, x + w - 1, y + h - 1, color);
    win_draw_line(win, x, y, x, y + h - 1, color);
    win_draw_line(win, x + w - 1, y, x + w - 1, y + h - 1, color);
}

static void win_draw_char(struct x_window *win, int x, int y, char c, uint32_t color) {
    const uint8_t *glyph = font8x16[(unsigned char)c];
    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                win_draw_pixel(win, x + col, y + row, color);
            }
        }
    }
}

static void win_draw_string(struct x_window *win, int x, int y, const char *str, uint32_t color) {
    if (!str) return;
    int cur_x = x;
    while (*str) {
        win_draw_char(win, cur_x, y, *str, color);
        cur_x += 8;
        str++;
    }
}

static void win_draw_arc(struct x_window *win, int x, int y, int w, int h, uint32_t color) {
    int rx = w / 2;
    int ry = h / 2;
    int cx = x + rx;
    int cy = y + ry;
    if (rx <= 0 || ry <= 0) return;

    for (int dy = -ry; dy <= ry; dy++) {
        double d = 1.0 - ((double)(dy * dy) / (double)(ry * ry));
        if (d < 0) continue;
        int dx = (int)(rx * sqrt(d));
        win_draw_pixel(win, cx - dx, cy + dy, color);
        win_draw_pixel(win, cx + dx, cy + dy, color);
    }
}

static void win_fill_arc(struct x_window *win, int x, int y, int w, int h, uint32_t color) {
    int rx = w / 2;
    int ry = h / 2;
    int cx = x + rx;
    int cy = y + ry;
    if (rx <= 0 || ry <= 0) return;

    for (int dy = -ry; dy <= ry; dy++) {
        double d = 1.0 - ((double)(dy * dy) / (double)(ry * ry));
        if (d < 0) continue;
        int dx = (int)(rx * sqrt(d));
        for (int cur_x = cx - dx; cur_x <= cx + dx; cur_x++) {
            win_draw_pixel(win, cur_x, cy + dy, color);
        }
    }
}

static void redraw_desktop(void) {
    /* 1. Clear desktop root */
    draw_fill_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_DESKTOP);

    /* Draw subtle grid pattern */
    for (int y = 0; y < SCREEN_HEIGHT; y += 32) {
        for (int x = 0; x < SCREEN_WIDTH; x += 32) {
            draw_pixel(x, y, 0x002B547E);
        }
    }

    /* 2. Redraw all mapped windows in Z-order */
    for (int i = 0; i < stack_count; i++) {
        uint32_t wid = window_stack[i];
        if (wid == ROOT_WINDOW_ID) continue;

        struct x_window *win = find_window(wid);
        if (!win || !win->mapped) continue;

        /* Draw window border */
        if (win->border_width > 0) {
            for (int b = 0; b < win->border_width; b++) {
                draw_rect(win->x - b - 1, win->y - b - 1,
                          win->width + (b + 1) * 2, win->height + (b + 1) * 2,
                          win->border_pixel);
            }
        }

        /* Composite window backing pixmap onto backbuffer */
        if (win->pixmap) {
            for (int r = 0; r < win->height; r++) {
                int screen_y = win->y + r;
                if (screen_y < 0 || screen_y >= SCREEN_HEIGHT) continue;

                for (int c = 0; c < win->width; c++) {
                    int screen_x = win->x + c;
                    if (screen_x < 0 || screen_x >= SCREEN_WIDTH) continue;

                    backbuffer[screen_y * SCREEN_WIDTH + screen_x] = win->pixmap[r * win->width + c];
                }
            }
        }
    }

    /* 3. Draw software mouse cursor */
    for (int cy = 0; cy < 16; cy++) {
        for (int cx = 0; cx < 16; cx++) {
            int px = mouse_x + cx;
            int py = mouse_y + cy;
            if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                if (cursor_mask[cy] & (0x8000 >> cx)) {
                    if (cursor_shape[cy] & (0x8000 >> cx)) {
                        draw_pixel(px, py, COLOR_BLACK);
                    } else {
                        draw_pixel(px, py, COLOR_WHITE);
                    }
                }
            }
        }
    }

    /* 4. Blit backbuffer to physical framebuffer */
    if (fb_fd >= 0) {
        lseek(fb_fd, 0, SEEK_SET);
        write(fb_fd, backbuffer, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
    }
}

static uint32_t window_at(int x, int y) {
    for (int i = stack_count - 1; i >= 0; i--) {
        uint32_t wid = window_stack[i];
        struct x_window *win = find_window(wid);
        if (!win || !win->mapped) continue;

        if (wid == ROOT_WINDOW_ID) return ROOT_WINDOW_ID;

        if (x >= win->x && x < win->x + win->width &&
            y >= win->y && y < win->y + win->height) {
            return wid;
        }
    }
    return ROOT_WINDOW_ID;
}

static void send_event(int client_fd, XEvent *ev) {
    if (client_fd >= 0) {
        send(client_fd, ev, sizeof(XEvent), MSG_DONTWAIT);
    }
}

static void handle_client_request(int client_idx) {
    int fd = clients[client_idx].fd;
    uint8_t req_buf[512];
    ssize_t n = recv(fd, req_buf, sizeof(req_buf), MSG_DONTWAIT);
    if (n < 0) {
        return; /* Non-blocking: no data ready */
    }
    if (n == 0) {
        /* Client disconnected / EOF */
        close(fd);
        clients[client_idx].active = false;
        return;
    }

    struct x11_req_header *hdr = (struct x11_req_header *)req_buf;

    switch (hdr->opcode) {
        case X_OP_CONNECT: {
            struct x11_connect_reply reply;
            memset(&reply, 0, sizeof(reply));
            reply.status = 1;
            reply.major_version = 11;
            reply.minor_version = 0;
            reply.root_window = ROOT_WINDOW_ID;
            reply.width = SCREEN_WIDTH;
            reply.height = SCREEN_HEIGHT;
            reply.depth = 32;
            reply.white_pixel = COLOR_WHITE;
            reply.black_pixel = COLOR_BLACK;
            reply.resource_id_base = (client_idx + 1) * 1000;
            clients[client_idx].resource_base = reply.resource_id_base;
            send(fd, &reply, sizeof(reply), 0);
            break;
        }

        case X_OP_CREATE_WINDOW: {
            struct x11_create_window_req *req = (struct x11_create_window_req *)req_buf;
            if (window_count < MAX_WINDOWS) {
                struct x_window *win = &windows[window_count++];
                win->wid = req->wid;
                win->parent = req->parent ? req->parent : ROOT_WINDOW_ID;
                win->client_fd = fd;
                win->x = req->x;
                win->y = req->y;
                win->width = req->width;
                win->height = req->height;
                win->border_width = req->border_width;
                win->border_pixel = req->border_pixel;
                win->bg_pixel = req->background_pixel;
                win->mapped = false;
                win->event_mask = 0;
                strcpy(win->title, "X11 Client");

                /* Allocate backing pixmap */
                win->pixmap = (uint32_t *)malloc(win->width * win->height * sizeof(uint32_t));
                if (win->pixmap) {
                    for (int p = 0; p < win->width * win->height; p++) {
                        win->pixmap[p] = win->bg_pixel;
                    }
                }

                window_stack[stack_count++] = win->wid;
            }
            break;
        }

        case X_OP_MAP_WINDOW:
        case X_OP_MAP_RAISED: {
            struct x11_map_window_req *req = (struct x11_map_window_req *)req_buf;
            struct x_window *win = find_window(req->wid);
            if (win) {
                win->mapped = true;
                stack_raise(win->wid);
                focused_window = win->wid;
                screen_dirty = true;

                /* Send Expose event to client */
                XEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = Expose;
                ev.xexpose.window = win->wid;
                ev.xexpose.x = 0;
                ev.xexpose.y = 0;
                ev.xexpose.width = win->width;
                ev.xexpose.height = win->height;
                ev.xexpose.count = 0;
                send_event(win->client_fd, &ev);
            }
            break;
        }

        case X_OP_UNMAP_WINDOW: {
            struct x11_unmap_window_req *req = (struct x11_unmap_window_req *)req_buf;
            struct x_window *win = find_window(req->wid);
            if (win) {
                win->mapped = false;
                screen_dirty = true;
            }
            break;
        }

        case X_OP_DESTROY_WINDOW: {
            struct x11_destroy_window_req *req = (struct x11_destroy_window_req *)req_buf;
            for (int i = 0; i < window_count; i++) {
                if (windows[i].wid == req->wid) {
                    if (windows[i].pixmap) {
                        free(windows[i].pixmap);
                        windows[i].pixmap = NULL;
                    }
                    windows[i].mapped = false;
                    for (int j = i; j < window_count - 1; j++) {
                        windows[j] = windows[j + 1];
                    }
                    window_count--;
                    break;
                }
            }
            screen_dirty = true;
            break;
        }

        case X_OP_MOVE_WINDOW:
        case X_OP_RESIZE_WINDOW:
        case X_OP_MOVE_RESIZE_WINDOW: {
            struct x11_move_resize_req *req = (struct x11_move_resize_req *)req_buf;
            struct x_window *win = find_window(req->wid);
            if (win) {
                if (hdr->opcode == X_OP_MOVE_WINDOW || hdr->opcode == X_OP_MOVE_RESIZE_WINDOW) {
                    win->x = req->x;
                    win->y = req->y;
                }
                if (hdr->opcode == X_OP_RESIZE_WINDOW || hdr->opcode == X_OP_MOVE_RESIZE_WINDOW) {
                    if (win->width != (int)req->width || win->height != (int)req->height) {
                        win->width = (int)req->width;
                        win->height = (int)req->height;
                        if (win->pixmap) free(win->pixmap);
                        win->pixmap = (uint32_t *)malloc(win->width * win->height * sizeof(uint32_t));
                        if (win->pixmap) {
                            for (int p = 0; p < win->width * win->height; p++) {
                                win->pixmap[p] = win->bg_pixel;
                            }
                        }
                    }
                }
                screen_dirty = true;

                /* Send ConfigureNotify */
                XEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = ConfigureNotify;
                ev.xconfigure.window = win->wid;
                ev.xconfigure.x = win->x;
                ev.xconfigure.y = win->y;
                ev.xconfigure.width = win->width;
                ev.xconfigure.height = win->height;
                send_event(win->client_fd, &ev);
            }
            break;
        }

        case X_OP_CREATE_GC: {
            struct x11_create_gc_req *req = (struct x11_create_gc_req *)req_buf;
            if (gc_count < MAX_GCS) {
                struct x_gc *gc = &gcs[gc_count++];
                gc->gid = req->gcontext;
                gc->fg = req->foreground;
                gc->bg = req->background;
                gc->line_width = 1;
            }
            break;
        }

        case X_OP_SET_FG: {
            struct x11_set_gc_color_req *req = (struct x11_set_gc_color_req *)req_buf;
            struct x_gc *gc = find_gc(req->gcontext);
            if (gc) gc->fg = req->color;
            break;
        }

        case X_OP_SET_BG: {
            struct x11_set_gc_color_req *req = (struct x11_set_gc_color_req *)req_buf;
            struct x_gc *gc = find_gc(req->gcontext);
            if (gc) gc->bg = req->color;
            break;
        }

        case X_OP_DRAW_POINT: {
            struct x11_draw_point_req *req = (struct x11_draw_point_req *)req_buf;
            struct x_window *win = find_window(req->drawable);
            struct x_gc *gc = find_gc(req->gcontext);
            if (win && gc) {
                win_draw_pixel(win, req->x, req->y, gc->fg);
                if (win->mapped) screen_dirty = true;
            }
            break;
        }

        case X_OP_DRAW_LINE: {
            struct x11_draw_line_req *req = (struct x11_draw_line_req *)req_buf;
            struct x_window *win = find_window(req->drawable);
            struct x_gc *gc = find_gc(req->gcontext);
            if (win && gc) {
                win_draw_line(win, req->x1, req->y1, req->x2, req->y2, gc->fg);
                if (win->mapped) screen_dirty = true;
            }
            break;
        }

        case X_OP_DRAW_RECT: {
            struct x11_draw_rect_req *req = (struct x11_draw_rect_req *)req_buf;
            struct x_window *win = find_window(req->drawable);
            struct x_gc *gc = find_gc(req->gcontext);
            if (win && gc) {
                win_draw_rect(win, req->x, req->y, req->width, req->height, gc->fg);
                if (win->mapped) screen_dirty = true;
            }
            break;
        }

        case X_OP_FILL_RECT: {
            struct x11_draw_rect_req *req = (struct x11_draw_rect_req *)req_buf;
            struct x_window *win = find_window(req->drawable);
            struct x_gc *gc = find_gc(req->gcontext);
            if (win && gc) {
                win_draw_fill_rect(win, req->x, req->y, req->width, req->height, gc->fg);
                if (win->mapped) screen_dirty = true;
            }
            break;
        }

        case X_OP_DRAW_ARC: {
            struct x11_draw_rect_req *req = (struct x11_draw_rect_req *)req_buf;
            struct x_window *win = find_window(req->drawable);
            struct x_gc *gc = find_gc(req->gcontext);
            if (win && gc) {
                win_draw_arc(win, req->x, req->y, req->width, req->height, gc->fg);
                if (win->mapped) screen_dirty = true;
            }
            break;
        }

        case X_OP_FILL_ARC: {
            struct x11_draw_rect_req *req = (struct x11_draw_rect_req *)req_buf;
            struct x_window *win = find_window(req->drawable);
            struct x_gc *gc = find_gc(req->gcontext);
            if (win && gc) {
                win_fill_arc(win, req->x, req->y, req->width, req->height, gc->fg);
                if (win->mapped) screen_dirty = true;
            }
            break;
        }

        case X_OP_DRAW_STRING: {
            struct x11_draw_string_req *req = (struct x11_draw_string_req *)req_buf;
            struct x_window *win = find_window(req->drawable);
            struct x_gc *gc = find_gc(req->gcontext);
            if (win && gc) {
                win_draw_string(win, req->x, req->y, req->text, gc->fg);
                if (win->mapped) screen_dirty = true;
            }
            break;
        }

        case X_OP_CLEAR_WINDOW: {
            uint32_t wid = *(uint32_t *)(req_buf + sizeof(struct x11_req_header));
            struct x_window *win = find_window(wid);
            if (win) {
                win_draw_fill_rect(win, 0, 0, win->width, win->height, win->bg_pixel);
                if (win->mapped) screen_dirty = true;
            }
            break;
        }

        case X_OP_CLEAR_AREA: {
            struct x11_draw_rect_req *req = (struct x11_draw_rect_req *)req_buf;
            struct x_window *win = find_window(req->drawable);
            if (win) {
                win_draw_fill_rect(win, req->x, req->y, req->width, req->height, win->bg_pixel);
                if (win->mapped) screen_dirty = true;
            }
            break;
        }

        case X_OP_SELECT_INPUT: {
            struct x11_select_input_req *req = (struct x11_select_input_req *)req_buf;
            struct x_window *win = find_window(req->wid);
            if (win) {
                win->event_mask = req->event_mask;
            }
            break;
        }

        case X_OP_STORE_NAME: {
            struct x11_store_name_req *req = (struct x11_store_name_req *)req_buf;
            struct x_window *win = find_window(req->wid);
            if (win) {
                strncpy(win->title, req->name, sizeof(win->title) - 1);
                screen_dirty = true;
            }
            break;
        }

        case X_OP_GET_GEOMETRY: {
            struct x11_get_geometry_req *req = (struct x11_get_geometry_req *)req_buf;
            struct x_window *win = find_window(req->drawable);
            struct x11_get_geometry_reply reply;
            memset(&reply, 0, sizeof(reply));
            reply.root = ROOT_WINDOW_ID;
            reply.depth = 32;
            if (win) {
                reply.x = win->x;
                reply.y = win->y;
                reply.width = win->width;
                reply.height = win->height;
                reply.border_width = win->border_width;
            }
            send(fd, &reply, sizeof(reply), 0);
            break;
        }

        case X_OP_QUERY_POINTER: {
            struct x11_query_pointer_req *req = (struct x11_query_pointer_req *)req_buf;
            struct x_window *win = find_window(req->wid);
            struct x11_query_pointer_reply reply;
            memset(&reply, 0, sizeof(reply));
            reply.root = ROOT_WINDOW_ID;
            reply.child = window_at(mouse_x, mouse_y);
            reply.root_x = mouse_x;
            reply.root_y = mouse_y;
            reply.win_x = win ? (mouse_x - win->x) : mouse_x;
            reply.win_y = win ? (mouse_y - win->y) : mouse_y;
            reply.mask = mouse_buttons;
            send(fd, &reply, sizeof(reply), 0);
            break;
        }

        case X_OP_QUERY_TREE: {
            struct x11_query_tree_req *req = (struct x11_query_tree_req *)req_buf;
            struct x_window *win = find_window(req->wid);
            struct x11_query_tree_reply reply;
            memset(&reply, 0, sizeof(reply));
            reply.root = ROOT_WINDOW_ID;
            reply.parent = win ? win->parent : 0;
            reply.nchildren = 0;

            for (int i = 0; i < window_count; i++) {
                if (windows[i].parent == req->wid && windows[i].wid != req->wid) {
                    if (reply.nchildren < 32) {
                        reply.children[reply.nchildren++] = windows[i].wid;
                    }
                }
            }
            send(fd, &reply, sizeof(reply), 0);
            break;
        }

        case X_OP_SYNC: {
            uint32_t resp = 1;
            send(fd, &resp, sizeof(resp), 0);
            break;
        }

        default:
            break;
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("Starting DUnix X11 Display Server (Xorg / Xserver)...\n");

    /* 1. Open Framebuffer & Switch to 1024x768x32 graphics mode */
    fb_fd = open("/dev/fb0", O_RDWR, 0);
    if (fb_fd < 0) {
        perror("Xserver: cannot open /dev/fb0");
        return 1;
    }

    struct fb_var_screeninfo var;
    memset(&var, 0, sizeof(var));
    var.xres = SCREEN_WIDTH;
    var.yres = SCREEN_HEIGHT;
    var.bits_per_pixel = 32;
    if (ioctl(fb_fd, FBIOPUT_VSCREENINFO, &var) < 0) {
        perror("Xserver: FBIOPUT_VSCREENINFO failed");
    }

    /* 2. Allocate Backbuffer */
    backbuffer = (uint32_t *)malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint32_t));
    if (!backbuffer) {
        perror("Xserver: cannot allocate backbuffer");
        close(fb_fd);
        return 1;
    }

    /* 3. Open PS/2 Mouse Device */
    mouse_fd = open("/dev/mouse", O_RDONLY, 0);
    if (mouse_fd < 0) {
        mouse_fd = open("/dev/input/mice", O_RDONLY, 0);
    }

    /* 4. Configure Stdin Terminal into Raw Non-Canonical Mode */
    struct termios raw_t;
    tcgetattr(STDIN_FILENO, &raw_t);
    raw_t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_t);

    /* 5. Initialize Root Window */
    init_root_window();

    /* 6. Open TCP Server Socket on 127.0.0.1:6000 */
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Xserver: socket failed");
        return 1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(X11_TCP_PORT);
    saddr.sin_addr.s_addr = htonl(0x7F000001); /* 127.0.0.1 */

    if (bind(server_sock, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        perror("Xserver: bind failed on 127.0.0.1:6000");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 16) < 0) {
        perror("Xserver: listen failed");
        close(server_sock);
        return 1;
    }

    int flags = fcntl(server_sock, F_GETFL, 0);
    fcntl(server_sock, F_SETFL, flags | O_NONBLOCK);

    memset(clients, 0, sizeof(clients));

    printf("Xserver: Display :0.0 active on 127.0.0.1:6000 (1024x768x32bpp)\n");

    /* Initial draw */
    redraw_desktop();

    /* Streaming mouse packet buffer */
    static uint8_t s_mouse_pkt[3];
    static int s_mouse_idx = 0;

    /* Main X11 Server Event Loop */
    for (;;) {
        /* Accept new client connections */
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int client_fd = accept(server_sock, (struct sockaddr *)&caddr, &clen);
        if (client_fd >= 0) {
            int cflags = fcntl(client_fd, F_GETFL, 0);
            fcntl(client_fd, F_SETFL, cflags | O_NONBLOCK);

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (!clients[i].active) {
                    clients[i].fd = client_fd;
                    clients[i].active = true;
                    break;
                }
            }
        }

        /* Read Mouse Events */
        if (mouse_fd >= 0) {
            uint8_t mbuf[32];
            ssize_t mn = read(mouse_fd, mbuf, sizeof(mbuf));
            for (ssize_t i = 0; i < mn; i++) {
                uint8_t b = mbuf[i];
                if (s_mouse_idx == 0) {
                    if (!(b & 0x08) || (b & 0xC0)) {
                        continue; /* Align to byte 0 with bit 3 set and overflow bits cleared */
                    }
                    s_mouse_pkt[0] = b;
                    s_mouse_idx = 1;
                } else if (s_mouse_idx == 1) {
                    s_mouse_pkt[1] = b;
                    s_mouse_idx = 2;
                } else if (s_mouse_idx == 2) {
                    s_mouse_pkt[2] = b;
                    s_mouse_idx = 0;

                    if (s_mouse_pkt[0] & 0xC0) {
                        continue;
                    }

                    int dx = (s_mouse_pkt[0] & 0x10) ? (int)(s_mouse_pkt[1] - 256) : (int)s_mouse_pkt[1];
                    int dy = (s_mouse_pkt[0] & 0x20) ? (int)(s_mouse_pkt[2] - 256) : (int)s_mouse_pkt[2];
                    dy = -dy;

                    if (dx < -127) dx = -127;
                    if (dx > 127)  dx = 127;
                    if (dy < -127) dy = -127;
                    if (dy > 127)  dy = 127;

                    mouse_x += dx;
                    mouse_y += dy;
                    if (mouse_x < 0) mouse_x = 0;
                    if (mouse_x >= SCREEN_WIDTH) mouse_x = SCREEN_WIDTH - 1;
                    if (mouse_y < 0) mouse_y = 0;
                    if (mouse_y >= SCREEN_HEIGHT) mouse_y = SCREEN_HEIGHT - 1;

                    uint8_t old_btn = mouse_buttons;
                    mouse_buttons = s_mouse_pkt[0] & 0x07;

                    uint32_t target_wid = window_at(mouse_x, mouse_y);
                    struct x_window *twin = find_window(target_wid);

                    /* Handle Left Button */
                    if ((mouse_buttons & 1) && !(old_btn & 1)) {
                        if (target_wid != ROOT_WINDOW_ID && target_wid != focused_window) {
                            stack_raise(target_wid);
                            focused_window = target_wid;
                        }
                        if (twin) {
                            XEvent ev;
                            memset(&ev, 0, sizeof(ev));
                            ev.type = ButtonPress;
                            ev.xbutton.window = target_wid;
                            ev.xbutton.root = ROOT_WINDOW_ID;
                            ev.xbutton.x_root = mouse_x;
                            ev.xbutton.y_root = mouse_y;
                            ev.xbutton.x = mouse_x - twin->x;
                            ev.xbutton.y = mouse_y - twin->y;
                            ev.xbutton.button = Button1;
                            send_event(twin->client_fd, &ev);
                        }
                    } else if (!(mouse_buttons & 1) && (old_btn & 1)) {
                        if (twin) {
                            XEvent ev;
                            memset(&ev, 0, sizeof(ev));
                            ev.type = ButtonRelease;
                            ev.xbutton.window = target_wid;
                            ev.xbutton.root = ROOT_WINDOW_ID;
                            ev.xbutton.x_root = mouse_x;
                            ev.xbutton.y_root = mouse_y;
                            ev.xbutton.x = mouse_x - twin->x;
                            ev.xbutton.y = mouse_y - twin->y;
                            ev.xbutton.button = Button1;
                            send_event(twin->client_fd, &ev);
                        }
                    }

                    /* Handle Right Button */
                    if ((mouse_buttons & 2) && !(old_btn & 2)) {
                        if (twin) {
                            XEvent ev;
                            memset(&ev, 0, sizeof(ev));
                            ev.type = ButtonPress;
                            ev.xbutton.window = target_wid;
                            ev.xbutton.root = ROOT_WINDOW_ID;
                            ev.xbutton.x_root = mouse_x;
                            ev.xbutton.y_root = mouse_y;
                            ev.xbutton.x = mouse_x - twin->x;
                            ev.xbutton.y = mouse_y - twin->y;
                            ev.xbutton.button = Button3;
                            send_event(twin->client_fd, &ev);
                        }
                    } else if (!(mouse_buttons & 2) && (old_btn & 2)) {
                        if (twin) {
                            XEvent ev;
                            memset(&ev, 0, sizeof(ev));
                            ev.type = ButtonRelease;
                            ev.xbutton.window = target_wid;
                            ev.xbutton.root = ROOT_WINDOW_ID;
                            ev.xbutton.x_root = mouse_x;
                            ev.xbutton.y_root = mouse_y;
                            ev.xbutton.x = mouse_x - twin->x;
                            ev.xbutton.y = mouse_y - twin->y;
                            ev.xbutton.button = Button3;
                            send_event(twin->client_fd, &ev);
                        }
                    }

                    /* Pointer Motion */
                    if (dx != 0 || dy != 0) {
                        if (twin && (twin->event_mask & PointerMotionMask)) {
                            XEvent ev;
                            memset(&ev, 0, sizeof(ev));
                            ev.type = MotionNotify;
                            ev.xmotion.window = target_wid;
                            ev.xmotion.root = ROOT_WINDOW_ID;
                            ev.xmotion.x_root = mouse_x;
                            ev.xmotion.y_root = mouse_y;
                            ev.xmotion.x = mouse_x - twin->x;
                            ev.xmotion.y = mouse_y - twin->y;
                            send_event(twin->client_fd, &ev);
                        }
                    }

                    screen_dirty = true;
                }
            }
        }

        /* Read Keyboard Events from stdin */
        char kbuf[16];
        ssize_t kn = read(STDIN_FILENO, kbuf, sizeof(kbuf));
        if (kn > 0) {
            struct x_window *fwin = find_window(focused_window);
            if (fwin) {
                for (ssize_t i = 0; i < kn; i++) {
                    XEvent ev;
                    memset(&ev, 0, sizeof(ev));
                    ev.type = KeyPress;
                    ev.xkey.window = focused_window;
                    ev.xkey.root = ROOT_WINDOW_ID;
                    ev.xkey.keycode = (unsigned char)kbuf[i];
                    send_event(fwin->client_fd, &ev);
                }
            }
        }

        /* Process Client Requests */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                handle_client_request(i);
            }
        }

        /* Redraw frame if dirty */
        if (screen_dirty) {
            redraw_desktop();
            screen_dirty = false;
        }

        usleep(10000); /* ~100 FPS loop */
    }

    free(backbuffer);
    close(fb_fd);
    close(server_sock);
    return 0;
}
