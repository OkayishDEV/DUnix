#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <time.h>
#include <math.h>
#include <dui/protocol.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "../xserver/font8x16.c"

#define SCREEN_WIDTH   1024
#define SCREEN_HEIGHT  768
#define MAX_WINDOWS    64
#define MAX_CLIENTS    16
#define NUM_TAGS       5

#define BAR_H          22
#define BORDER_W       2
#define TITLE_H        18

/* DWM Color Scheme */
#define COLOR_ROOT_BG       0x00141414  /* Dark charcoal desktop */
#define COLOR_ROOT_GRID     0x00202020  /* Subtle grid pattern */
#define COLOR_BAR_BG        0x00222222  /* DWM dark gray top bar */
#define COLOR_BAR_BORDER    0x00333333  /* Top bar separator */
#define COLOR_BAR_FG        0x00AAAAAA  /* Normal text */
#define COLOR_BAR_FG_BRIGHT 0x00EEEEEE  /* Bright text */
#define COLOR_SEL_BG        0x00005577  /* DWM cyan / petrol blue */
#define COLOR_SEL_FG        0x00FFFFFF  /* Pure white */
#define COLOR_SEL_BORDER    0x00005577  /* Active window border */
#define COLOR_NORM_BORDER   0x00383838  /* Inactive window border */
#define COLOR_BTN_BG        0x002D2D2D  /* Launcher button bg */
#define COLOR_BTN_FG        0x00CCCCCC  /* Launcher button text */
#define COLOR_ACCENT        0x0000AAFF  /* Bright cyan accent */
#define COLOR_CLOSE_BTN     0x00CC3333  /* Minimal close button */

#define LAYOUT_TILED    0
#define LAYOUT_FLOATING 1
#define LAYOUT_MONOCLE  2

struct window {
    uint32_t wid;
    int client_fd;
    int x, y;             /* On-screen position */
    int width, height;    /* Current display width/height */
    int client_w, client_h; /* Backing pixmap dimensions */
    uint32_t bg_color;
    uint32_t flags;
    bool mapped;
    int tag;              /* Workspace tag (1..5) */
    char title[64];
    uint32_t *pixmap;
};

struct client {
    int fd;
    bool active;
    uint32_t id;
};

static int fb_fd = -1;
static int mouse_fd = -1;
static int server_fd = -1;
static uint32_t *backbuffer = NULL;

static int drm_fd = -1;
static uint32_t drm_front_fb = 0;
static uint32_t drm_back_fb = 0;
static uint32_t *drm_front_vram = NULL;
static uint32_t *drm_back_vram = NULL;
static uint32_t *drm_vram = NULL;
static bool drm_hw_flip_active = false;
static bool drm_front_is_active = true;

static struct window windows[MAX_WINDOWS];
static int window_count = 0;
static uint32_t z_order[MAX_WINDOWS];
static int z_count = 0;

static struct client clients[MAX_CLIENTS];
static uint32_t next_client_id = 1;

static int mouse_x = SCREEN_WIDTH / 2;
static int mouse_y = SCREEN_HEIGHT / 2;
static uint8_t mouse_buttons = 0;

static bool screen_dirty = true;
static uint32_t focused_window = 0;

static int current_tag = 1;
static int current_layout = LAYOUT_FLOATING;

static bool is_dragging = false;
static uint32_t drag_wid = 0;
static int drag_offset_x = 0;
static int drag_offset_y = 0;

/* Clean classic arrow cursor */
static const uint16_t cursor_mask[16] = {
    0x8000, 0xC000, 0xE000, 0xF000, 0xF800, 0xFC00, 0xFE00, 0xFF00,
    0xFF80, 0xFFC0, 0xFC00, 0xDC00, 0x8E00, 0x0E00, 0x0700, 0x0300
};
static const uint16_t cursor_shape[16] = {
    0x0000, 0x4000, 0x6000, 0x7000, 0x7800, 0x7C00, 0x7E00, 0x7800,
    0x4C00, 0x0C00, 0x0600, 0x0600, 0x0000, 0x0000, 0x0000, 0x0000
};

static inline int iabs(int v) { return v < 0 ? -v : v; }

static struct window *find_window(uint32_t wid) {
    if (wid == 0) return NULL;
    for (int i = 0; i < window_count; i++) {
        if (windows[i].wid == wid) return &windows[i];
    }
    return NULL;
}

static void destroy_window(uint32_t wid) {
    if (wid == 0) return;
    printf("[DWS] DESTROY: wid=%u\n", wid);
    for (int i = 0; i < window_count; i++) {
        if (windows[i].wid == wid) {
            if (windows[i].pixmap) {
                free(windows[i].pixmap);
                windows[i].pixmap = NULL;
            }
            for (int j = i; j < window_count - 1; j++) {
                windows[j] = windows[j + 1];
            }
            window_count--;
            break;
        }
    }
    for (int i = 0; i < z_count; i++) {
        if (z_order[i] == wid) {
            for (int j = i; j < z_count - 1; j++) {
                z_order[j] = z_order[j + 1];
            }
            z_count--;
            break;
        }
    }
    if (focused_window == wid) {
        focused_window = 0;
        for (int i = z_count - 1; i >= 0; i--) {
            struct window *w = find_window(z_order[i]);
            if (w && w->mapped && w->tag == current_tag) {
                focused_window = w->wid;
                break;
            }
        }
    }
    screen_dirty = true;
}

static void disconnect_client(int cidx) {
    if (cidx < 0 || cidx >= MAX_CLIENTS) return;
    struct client *c = &clients[cidx];
    if (!c->active) return;
    printf("[DWS] disconnect_client cidx=%d id=%u\n", cidx, c->id);
    int client_fd = c->fd;
    close(c->fd);
    c->active = false;

    /* Destroy and remove all windows owned by this client */
    for (int i = window_count - 1; i >= 0; i--) {
        if (windows[i].client_fd == client_fd) {
            destroy_window(windows[i].wid);
        }
    }
    screen_dirty = true;
}

static void send_event(int fd, struct dws_message *msg) {
    if (fd >= 0) {
        send(fd, msg, sizeof(struct dws_message), MSG_DONTWAIT);
    }
}

static void stack_raise(uint32_t wid) {
    int idx = -1;
    for (int i = 0; i < z_count; i++) {
        if (z_order[i] == wid) {
            idx = i; break;
        }
    }
    if (idx >= 0 && idx < z_count - 1) {
        for (int i = idx; i < z_count - 1; i++) {
            z_order[i] = z_order[i + 1];
        }
        z_order[z_count - 1] = wid;
    }
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
        for (int c = 0; c < w; c++) row[c] = color;
    }
}

static void draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = iabs(x2 - x1), dy = iabs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1, sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    while (1) {
        draw_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

static void draw_rect(int x, int y, int w, int h, uint32_t color) {
    draw_line(x, y, x + w - 1, y, color);
    draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
    draw_line(x, y, x, y + h - 1, color);
    draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
}

static void draw_char(int x, int y, char c, uint32_t color) {
    const uint8_t *glyph = font8x16[(unsigned char)c];
    for (int r = 0; r < 16; r++) {
        for (int c2 = 0; c2 < 8; c2++) {
            if (glyph[r] & (0x80 >> c2)) draw_pixel(x + c2, y + r, color);
        }
    }
}

static void draw_string(int x, int y, const char *str, uint32_t color) {
    if (!str) return;
    while (*str) {
        draw_char(x, y, *str++, color);
        x += 8;
    }
}

/* Window backing pixmap drawing routines */
static void win_draw_pixel(struct window *win, int x, int y, uint32_t color) {
    if (!win || !win->pixmap) return;
    if (x >= 0 && x < win->client_w && y >= 0 && y < win->client_h) {
        win->pixmap[y * win->client_w + x] = color;
    }
}

static void win_draw_fill_rect(struct window *win, int x, int y, int w, int h, uint32_t color) {
    if (!win || !win->pixmap) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > win->client_w) w = win->client_w - x;
    if (y + h > win->client_h) h = win->client_h - y;
    if (w <= 0 || h <= 0) return;
    for (int r = y; r < y + h; r++) {
        uint32_t *row = &win->pixmap[r * win->client_w + x];
        for (int c = 0; c < w; c++) row[c] = color;
    }
}

static void win_draw_line(struct window *win, int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = iabs(x2 - x1), dy = iabs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1, sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    while (1) {
        win_draw_pixel(win, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

static void win_draw_rect(struct window *win, int x, int y, int w, int h, uint32_t color) {
    win_draw_line(win, x, y, x + w - 1, y, color);
    win_draw_line(win, x, y + h - 1, x + w - 1, y + h - 1, color);
    win_draw_line(win, x, y, x, y + h - 1, color);
    win_draw_line(win, x + w - 1, y, x + w - 1, y + h - 1, color);
}

static void win_draw_char(struct window *win, int x, int y, char c, uint32_t color) {
    const uint8_t *glyph = font8x16[(unsigned char)c];
    for (int r = 0; r < 16; r++) {
        for (int c2 = 0; c2 < 8; c2++) {
            if (glyph[r] & (0x80 >> c2)) win_draw_pixel(win, x + c2, y + r, color);
        }
    }
}

static void win_draw_string(struct window *win, int x, int y, const char *str, uint32_t color) {
    if (!str) return;
    while (*str) {
        win_draw_char(win, x, y, *str++, color);
        x += 8;
    }
}

static void win_draw_circle(struct window *win, int cx, int cy, int radius, uint32_t color) {
    if (!win) return;
    int x = radius, y = 0, err = 0;
    while (x >= y) {
        win_draw_pixel(win, cx + x, cy + y, color);
        win_draw_pixel(win, cx + y, cy + x, color);
        win_draw_pixel(win, cx - y, cy + x, color);
        win_draw_pixel(win, cx - x, cy + y, color);
        win_draw_pixel(win, cx - x, cy - y, color);
        win_draw_pixel(win, cx - y, cy - x, color);
        win_draw_pixel(win, cx + y, cy - x, color);
        win_draw_pixel(win, cx + x, cy - y, color);
        if (err <= 0) { y += 1; err += 2*y + 1; }
        if (err > 0) { x -= 1; err -= 2*x + 1; }
    }
}

static void win_fill_circle(struct window *win, int cx, int cy, int radius, uint32_t color) {
    if (!win) return;
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                win_draw_pixel(win, cx + x, cy + y, color);
            }
        }
    }
}

static void launch_app(const char *path) {
    pid_t pid = fork();
    if (pid == 0) {
        if (server_fd >= 0) close(server_fd);
        if (mouse_fd >= 0) close(mouse_fd);
        if (drm_fd >= 0) close(drm_fd);
        if (fb_fd >= 0) close(fb_fd);
        char *args[] = {(char*)path, NULL};
        execve(path, args, NULL);
        exit(1);
    }
}

/* DWM Tiling Engine */
static void arrange_windows(void) {
    if (current_layout != LAYOUT_TILED) return;

    struct window *visible[MAX_WINDOWS];
    int count = 0;
    for (int i = 0; i < z_count; i++) {
        struct window *w = find_window(z_order[i]);
        if (w && w->mapped && (w->tag == current_tag)) {
            visible[count++] = w;
        }
    }
    if (count == 0) return;

    int area_x = BORDER_W;
    int area_y = BAR_H + TITLE_H + BORDER_W;
    int area_w = SCREEN_WIDTH - 2 * BORDER_W;
    int area_h = SCREEN_HEIGHT - BAR_H - TITLE_H - 2 * BORDER_W;

    if (count == 1) {
        visible[0]->x = area_x;
        visible[0]->y = area_y;
        visible[0]->width = area_w;
        visible[0]->height = area_h;
    } else if (count == 2) {
        int mw = (area_w * 54) / 100;
        visible[0]->x = area_x;
        visible[0]->y = area_y;
        visible[0]->width = mw - BORDER_W;
        visible[0]->height = area_h;

        visible[1]->x = area_x + mw + BORDER_W;
        visible[1]->y = area_y;
        visible[1]->width = area_w - mw - BORDER_W;
        visible[1]->height = area_h;
    } else {
        int mw = (area_w * 54) / 100;
        visible[0]->x = area_x;
        visible[0]->y = area_y;
        visible[0]->width = mw - BORDER_W;
        visible[0]->height = area_h;

        int nstack = count - 1;
        int sh = (SCREEN_HEIGHT - BAR_H) / nstack;
        for (int i = 0; i < nstack; i++) {
            visible[1 + i]->x = area_x + mw + BORDER_W;
            visible[1 + i]->y = BAR_H + i * sh + TITLE_H + BORDER_W;
            visible[1 + i]->width = area_w - mw - BORDER_W;
            visible[1 + i]->height = sh - TITLE_H - 2 * BORDER_W;
            if (visible[1 + i]->height < 10) visible[1 + i]->height = 10;
        }
    }
}

/* DWM Top Status Bar */
static void draw_bar(void) {
    /* 1. Bar background */
    draw_fill_rect(0, 0, SCREEN_WIDTH, BAR_H, COLOR_BAR_BG);
    draw_line(0, BAR_H - 1, SCREEN_WIDTH - 1, BAR_H - 1, COLOR_BAR_BORDER);

    /* 2. Workspace tags: [ 1 ] [ 2 ] [ 3 ] [ 4 ] [ 5 ] */
    int tx = 0;
    for (int t = 1; t <= NUM_TAGS; t++) {
        int tw = 26;
        bool active = (t == current_tag);

        /* Check if tag has windows */
        bool has_wins = false;
        for (int i = 0; i < window_count; i++) {
            if (windows[i].mapped && windows[i].tag == t) {
                has_wins = true; break;
            }
        }

        uint32_t bg = active ? COLOR_SEL_BG : COLOR_BAR_BG;
        uint32_t fg = active ? COLOR_SEL_FG : (has_wins ? COLOR_BAR_FG_BRIGHT : COLOR_BAR_FG);

        draw_fill_rect(tx, 0, tw, BAR_H - 1, bg);

        /* Small indicator dot on active windows */
        if (has_wins && !active) {
            draw_fill_rect(tx + 2, 2, 3, 3, COLOR_ACCENT);
        }

        char tstr[4];
        snprintf(tstr, sizeof(tstr), " %d", t);
        draw_string(tx + 3, 3, tstr, fg);

        draw_line(tx + tw, 0, tx + tw, BAR_H - 1, COLOR_BAR_BORDER);
        tx += tw + 1;
    }

    /* 3. Layout indicator: []= or ><> or [M] */
    const char *layout_sym = "[]=";
    if (current_layout == LAYOUT_FLOATING) layout_sym = "><>";
    else if (current_layout == LAYOUT_MONOCLE) layout_sym = "[M]";

    draw_fill_rect(tx, 0, 32, BAR_H - 1, COLOR_BAR_BG);
    draw_string(tx + 4, 3, layout_sym, COLOR_ACCENT);
    draw_line(tx + 32, 0, tx + 32, BAR_H - 1, COLOR_BAR_BORDER);
    tx += 33;

    /* 4. Quick App Launchers: [+term] [+web] [+3D] [+files] [+calc] [+clock] [+snake] [+mines] [+2048] */
    const char *launchers[] = {"+term", "+web", "+3D", "+files", "+calc", "+clock", "+snake", "+mines", "+2048"};
    int lw[] = {44, 40, 32, 48, 44, 48, 48, 48, 44};
    for (int i = 0; i < 9; i++) {
        draw_fill_rect(tx, 1, lw[i], BAR_H - 2, COLOR_BTN_BG);
        draw_string(tx + 4, 3, launchers[i], COLOR_BTN_FG);
        draw_line(tx + lw[i], 0, tx + lw[i], BAR_H - 1, COLOR_BAR_BORDER);
        tx += lw[i] + 2;
    }

    /* 5. Focused Window Title in DWM titlebox */
    struct window *fwin = find_window(focused_window);
    if (fwin && fwin->mapped) {
        int max_title_w = 260;
        draw_fill_rect(tx, 0, max_title_w, BAR_H - 1, COLOR_SEL_BG);

        char title_buf[40];
        strncpy(title_buf, fwin->title, 28);
        title_buf[28] = '\0';
        draw_string(tx + 8, 3, title_buf, COLOR_SEL_FG);

        /* Clean close button [x] on titlebox */
        int cbx = tx + max_title_w - 20;
        draw_fill_rect(cbx, 2, 16, BAR_H - 5, COLOR_CLOSE_BTN);
        draw_string(cbx + 4, 3, "x", 0xFFFFFF);

        draw_line(tx + max_title_w, 0, tx + max_title_w, BAR_H - 1, COLOR_BAR_BORDER);
    }

    /* 6. System Status & Clock (Right-aligned) */
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char status_str[64];
    snprintf(status_str, sizeof(status_str), "DUnix 64-Bit | root | %02d:%02d:%02d",
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

    int sw = strlen(status_str) * 8;
    int sx = SCREEN_WIDTH - sw - 12;
    if (sx > tx + 350) {
        draw_string(sx, 3, status_str, COLOR_BAR_FG_BRIGHT);
    }
}

/* Full Desktop Redraw */
static void redraw_desktop(void) {
    printf("[DWS] redraw_desktop: wins=%d z=%d foc=%u\n", window_count, z_count, focused_window);
    /* 1. Draw suckless minimalist desktop background */
    draw_fill_rect(0, BAR_H, SCREEN_WIDTH, SCREEN_HEIGHT - BAR_H, COLOR_ROOT_BG);
    for (int y = BAR_H + 16; y < SCREEN_HEIGHT; y += 32) {
        for (int x = 16; x < SCREEN_WIDTH; x += 32) {
            draw_pixel(x, y, COLOR_ROOT_GRID);
        }
    }

    /* 2. Recalculate tiling layout */
    arrange_windows();

    /* 3. Draw mapped windows for current tag */
    for (int i = 0; i < z_count; i++) {
        struct window *win = find_window(z_order[i]);
        if (!win || !win->mapped || (win->tag != current_tag)) continue;

        bool is_focused = (win->wid == focused_window);
        uint32_t bcolor = is_focused ? COLOR_SEL_BORDER : COLOR_NORM_BORDER;

        int win_top = win->y - TITLE_H;
        if (win_top < BAR_H) win_top = BAR_H;

        /* Razor-sharp 2px border around window and titlebar */
        draw_rect(win->x - BORDER_W, win_top - BORDER_W,
                  win->width + 2 * BORDER_W, win->height + (win->y - win_top) + 2 * BORDER_W, bcolor);
        draw_rect(win->x - 1, win_top - 1,
                  win->width + 2, win->height + (win->y - win_top) + 2, bcolor);

        /* Fill titlebar */
        draw_fill_rect(win->x, win_top, win->width, win->y - win_top, is_focused ? COLOR_SEL_BG : COLOR_BTN_BG);
        draw_line(win->x, win->y - 1, win->x + win->width - 1, win->y - 1, COLOR_BAR_BORDER);

        /* Title text */
        char title_buf[64];
        int max_chars = (win->width - 26) / 8;
        if (max_chars > 60) max_chars = 60;
        if (max_chars < 4) max_chars = 4;
        strncpy(title_buf, win->title, max_chars);
        title_buf[max_chars] = '\0';
        draw_string(win->x + 6, win_top + 1, title_buf, is_focused ? COLOR_SEL_FG : COLOR_BTN_FG);

        /* Clean close button [x] on top-right of window frame */
        int w_cbx = win->x + win->width - 17;
        draw_fill_rect(w_cbx, win_top + 1, 15, TITLE_H - 3, COLOR_CLOSE_BTN);
        draw_string(w_cbx + 4, win_top + 1, "x", 0xFFFFFF);

        /* Fill client background */
        draw_fill_rect(win->x, win->y, win->width, win->height, win->bg_color);

        /* Composite client pixmap */
        if (win->pixmap) {
            int draw_w = (win->width < win->client_w) ? win->width : win->client_w;
            int draw_h = (win->height < win->client_h) ? win->height : win->client_h;

            for (int r = 0; r < draw_h; r++) {
                int sy = win->y + r;
                if (sy < BAR_H || sy >= SCREEN_HEIGHT) continue;
                for (int c = 0; c < draw_w; c++) {
                    int sx = win->x + c;
                    if (sx < 0 || sx >= SCREEN_WIDTH) continue;
                    backbuffer[sy * SCREEN_WIDTH + sx] = win->pixmap[r * win->client_w + c];
                }
            }
        }
    }

    /* 4. Draw DWM top bar */
    draw_bar();

    /* 5. Draw mouse cursor */
    for (int cy = 0; cy < 16; cy++) {
        for (int cx = 0; cx < 16; cx++) {
            int px = mouse_x + cx, py = mouse_y + cy;
            if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                if (cursor_mask[cy] & (0x8000 >> cx)) {
                    if (cursor_shape[cy] & (0x8000 >> cx)) draw_pixel(px, py, 0x000000);
                    else draw_pixel(px, py, 0xFFFFFF);
                }
            }
        }
    }

    /* 6. Commit to framebuffer / DRM Scanout */
    if (drm_hw_flip_active) {
        uint32_t flip_fb = drm_front_is_active ? drm_back_fb : drm_front_fb;
        drmModePageFlip(drm_fd, 1, flip_fb, DRM_MODE_PAGE_FLIP_EVENT, NULL);
        drm_front_is_active = !drm_front_is_active;
        backbuffer = drm_front_is_active ? drm_back_vram : drm_front_vram;
    } else if (fb_fd >= 0) {
        lseek(fb_fd, 0, SEEK_SET);
        write(fb_fd, backbuffer, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
    }
}

static void handle_bar_click(int x) {
    /* Tag clicks: [ 1 ] [ 2 ] [ 3 ] [ 4 ] [ 5 ] */
    if (x < 27 * NUM_TAGS) {
        int clicked_tag = x / 27 + 1;
        if (clicked_tag >= 1 && clicked_tag <= NUM_TAGS) {
            current_tag = clicked_tag;
            /* Focus top window on this tag */
            focused_window = 0;
            for (int i = z_count - 1; i >= 0; i--) {
                struct window *w = find_window(z_order[i]);
                if (w && w->mapped && w->tag == current_tag) {
                    focused_window = w->wid;
                    break;
                }
            }
            screen_dirty = true;
            return;
        }
    }

    /* Layout toggle: x between 135 and 168 */
    if (x >= 135 && x < 168) {
        current_layout = (current_layout == LAYOUT_TILED) ? LAYOUT_FLOATING : LAYOUT_TILED;
        screen_dirty = true;
        return;
    }

    /* Launcher buttons */
    if (x >= 168 && x < 214) { launch_app("/bin/dterm"); return; }
    if (x >= 214 && x < 256) { launch_app("/bin/dweb"); return; }
    if (x >= 256 && x < 290) { launch_app("/bin/glgears"); return; }
    if (x >= 290 && x < 340) { launch_app("/bin/dfiles"); return; }
    if (x >= 340 && x < 386) { launch_app("/bin/dcalc"); return; }
    if (x >= 386 && x < 436) { launch_app("/bin/dclock"); return; }
    if (x >= 436 && x < 486) { launch_app("/bin/snake"); return; }
    if (x >= 486 && x < 536) { launch_app("/bin/minesweeper"); return; }
    if (x >= 536 && x < 582) { launch_app("/bin/2048"); return; }

    /* Close button on focused window titlebox or drag by titlebox */
    struct window *fwin = find_window(focused_window);
    if (fwin && fwin->mapped) {
        int title_start = 584;
        int max_title_w = 260;
        int cbx = title_start + max_title_w - 20;
        if (x >= cbx && x < cbx + 18) {
            struct dws_message ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = DWS_EV_CLOSE_REQ;
            ev.window_id = fwin->wid;
            send_event(fwin->client_fd, &ev);
            destroy_window(fwin->wid);
            return;
        } else if (x >= title_start && x < cbx) {
            is_dragging = true;
            drag_wid = fwin->wid;
            drag_offset_x = mouse_x - fwin->x;
            drag_offset_y = mouse_y - fwin->y;
            return;
        }
    }
}

static void process_mouse(void) {
    static uint8_t mbuf[64];
    static int mbuf_len = 0;

    uint8_t tmp[64];
    ssize_t n = read(mouse_fd, tmp, sizeof(tmp));
    if (n <= 0) return;

    for (int i = 0; i < n && mbuf_len < (int)sizeof(mbuf); i++) {
        mbuf[mbuf_len++] = tmp[i];
    }

    int pos = 0;
    while (pos + 2 < mbuf_len) {
        if (!(mbuf[pos] & 0x08)) {
            pos++;
            continue;
        }

        uint8_t b0 = mbuf[pos];
        uint8_t b1 = mbuf[pos + 1];
        uint8_t b2 = mbuf[pos + 2];
        pos += 3;

        int dx = (int)b1;
        int dy = (int)b2;
        if (b0 & 0x10) dx -= 256;
        if (b0 & 0x20) dy -= 256;

        mouse_x += dx;
        mouse_y -= dy;
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x >= SCREEN_WIDTH) mouse_x = SCREEN_WIDTH - 1;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y >= SCREEN_HEIGHT) mouse_y = SCREEN_HEIGHT - 1;

        uint8_t btns = b0 & 0x07;
        bool l_down = (btns & 1) && !(mouse_buttons & 1);
        bool l_up = !(btns & 1) && (mouse_buttons & 1);
        bool r_down = (btns & 2) && !(mouse_buttons & 2);
        bool r_up = !(btns & 2) && (mouse_buttons & 2);
        mouse_buttons = btns;
        screen_dirty = true;

        if (is_dragging) {
            struct window *win = find_window(drag_wid);
            if (win) {
                current_layout = LAYOUT_FLOATING; /* Auto-switch to floating when dragged */
                win->x = mouse_x - drag_offset_x;
                win->y = mouse_y - drag_offset_y;
                if (win->y < BAR_H + TITLE_H + BORDER_W) win->y = BAR_H + TITLE_H + BORDER_W;
            }
            if (l_up || r_up) is_dragging = false;
        } else {
            if (mouse_y < BAR_H) {
                if (l_down) handle_bar_click(mouse_x);
            } else {
                /* Hit test windows in Z-order */
                struct window *hit = NULL;
                for (int i = z_count - 1; i >= 0; i--) {
                    struct window *w = find_window(z_order[i]);
                    if (w && w->mapped && w->tag == current_tag) {
                        int w_top = w->y - TITLE_H;
                        if (w_top < BAR_H) w_top = BAR_H;
                        if (mouse_x >= w->x - BORDER_W && mouse_x < w->x + w->width + BORDER_W &&
                            mouse_y >= w_top - BORDER_W && mouse_y < w->y + w->height + BORDER_W) {
                            hit = w;
                            break;
                        }
                    }
                }

                if (hit) {
                    int h_top = hit->y - TITLE_H;
                    if (h_top < BAR_H) h_top = BAR_H;

                    /* Focus follows mouse */
                    if (hit->wid != focused_window) {
                        focused_window = hit->wid;
                        struct dws_message ev;
                        memset(&ev, 0, sizeof(ev));
                        ev.type = DWS_EV_FOCUS_IN;
                        ev.window_id = hit->wid;
                        send_event(hit->client_fd, &ev);
                        screen_dirty = true;
                    }
                    if (l_down) {
                        stack_raise(hit->wid);
                    }

                    /* 1. Close button click on window titlebar [x] */
                    int w_cbx = hit->x + hit->width - 18;
                    if (l_down && mouse_x >= w_cbx && mouse_x <= hit->x + hit->width + BORDER_W &&
                        mouse_y >= h_top - BORDER_W && mouse_y <= hit->y) {
                        struct dws_message ev;
                        memset(&ev, 0, sizeof(ev));
                        ev.type = DWS_EV_CLOSE_REQ;
                        ev.window_id = hit->wid;
                        send_event(hit->client_fd, &ev);
                        destroy_window(hit->wid);
                        return;
                    }

                    /* 2. Drag window if clicking on titlebar or border */
                    if ((l_down && (mouse_y < hit->y || mouse_x < hit->x || mouse_x >= hit->x + hit->width || mouse_y >= hit->y + hit->height)) ||
                        r_down) {
                        is_dragging = true;
                        drag_wid = hit->wid;
                        drag_offset_x = mouse_x - hit->x;
                        drag_offset_y = mouse_y - hit->y;
                    } else if (mouse_y >= hit->y && (l_down || l_up || dx || dy)) {
                        /* 3. Client area event */
                        struct dws_message ev;
                        memset(&ev, 0, sizeof(ev));
                        ev.type = l_down ? DWS_EV_MOUSE_DOWN : (l_up ? DWS_EV_MOUSE_UP : DWS_EV_MOUSE_MOVE);
                        ev.window_id = hit->wid;
                        ev.mouse.mx = mouse_x - hit->x;
                        ev.mouse.my = mouse_y - hit->y;
                        ev.mouse.button = (btns & 2) ? 3 : 1;
                        ev.mouse.root_x = mouse_x;
                        ev.mouse.root_y = mouse_y;
                        send_event(hit->client_fd, &ev);
                    }
                }
            }
        }
    }

    if (pos < mbuf_len) {
        int remain = mbuf_len - pos;
        for (int i = 0; i < remain; i++) mbuf[i] = mbuf[pos + i];
        mbuf_len = remain;
    } else {
        mbuf_len = 0;
    }
}

static void process_keyboard(void) {
    char buf[16];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0) return;

    for (int i = 0; i < n; i++) {
        char c = buf[i];

        /* Tab to cycle window focus when multiple windows are open */
        if (c == '\t' && z_count > 1) {
            int cur_idx = -1;
            for (int j = 0; j < z_count; j++) {
                if (z_order[j] == focused_window) {
                    cur_idx = j;
                    break;
                }
            }
            int next_idx = (cur_idx + 1) % z_count;
            struct window *nw = find_window(z_order[next_idx]);
            if (nw && nw->mapped && nw->tag == current_tag) {
                focused_window = nw->wid;
                stack_raise(nw->wid);
                screen_dirty = true;
                continue;
            }
        }

        /* Ensure a window is focused if one exists */
        if (focused_window == 0) {
            for (int j = z_count - 1; j >= 0; j--) {
                struct window *w = find_window(z_order[j]);
                if (w && w->mapped && w->tag == current_tag) {
                    focused_window = w->wid;
                    break;
                }
            }
        }

        if (focused_window != 0) {
            struct window *win = find_window(focused_window);
            if (win) {
                struct dws_message ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = DWS_EV_KEY_DOWN;
                ev.window_id = win->wid;
                ev.key.ch = c;
                ev.key.keycode = c;
                send_event(win->client_fd, &ev);
            }
        }
    }
}

static void handle_client_request(int cidx) {
    struct client *c = &clients[cidx];
    struct dws_message msg;
    while (c->active) {
        ssize_t n = recv(c->fd, &msg, sizeof(msg), MSG_DONTWAIT);
        if (n <= 0) {
            if (n == 0 || (n < 0 && n != -11)) {
                disconnect_client(cidx);
            }
            break;
        }

        if (n < (ssize_t)sizeof(msg)) {
            size_t got = (size_t)n;
            uint8_t *p = (uint8_t *)&msg;
            int flags = fcntl(c->fd, F_GETFL, 0);
            fcntl(c->fd, F_SETFL, flags & ~O_NONBLOCK);
            int retries = 0;
            while (got < sizeof(msg)) {
                ssize_t r = recv(c->fd, p + got, sizeof(msg) - got, 0);
                if (r < 0) {
                    if (r == -11) {
                        usleep(200);
                        if (++retries > 50) break;
                        continue;
                    }
                    break;
                }
                if (r == 0) break;
                got += r;
                retries = 0;
            }
            fcntl(c->fd, F_SETFL, flags);
            if (got < sizeof(msg)) {
                disconnect_client(cidx);
                break;
            }
        }

        struct window *win = find_window(msg.window_id);

    switch (msg.type) {
        case DWS_REQ_CONNECT: {
            struct dws_message rep;
            memset(&rep, 0, sizeof(rep));
            rep.type = DWS_EV_CONNECTED;
            rep.connected.screen_width = SCREEN_WIDTH;
            rep.connected.screen_height = SCREEN_HEIGHT;
            rep.connected.client_id = c->id;
            send(c->fd, &rep, sizeof(rep), 0);
            break;
        }
        case DWS_REQ_CREATE_WINDOW: {
            if (window_count < MAX_WINDOWS) {
                struct window *w = &windows[window_count++];
                w->wid = msg.window_id;
                w->client_fd = c->fd;
                w->x = msg.create.x;
                w->y = msg.create.y;
                if (w->y < BAR_H + TITLE_H + BORDER_W) {
                    w->y = BAR_H + TITLE_H + BORDER_W;
                }
                w->width = msg.create.width;
                w->height = msg.create.height;
                w->client_w = msg.create.width;
                w->client_h = msg.create.height;
                w->bg_color = msg.create.bg_color;
                w->flags = msg.create.flags;
                w->mapped = false;
                w->tag = current_tag;
                strncpy(w->title, msg.create.title, 63);
                w->pixmap = malloc(w->client_w * w->client_h * sizeof(uint32_t));
                if (w->pixmap) {
                    for (int i = 0; i < w->client_w * w->client_h; i++) w->pixmap[i] = w->bg_color;
                }
                z_order[z_count++] = w->wid;
                printf("[DWS] CREATE_WINDOW: wid=%u '%s' tag=%d at (%d,%d)\n", w->wid, w->title, w->tag, w->x, w->y);
            }
            break;
        }
        case DWS_REQ_DESTROY_WINDOW: {
            destroy_window(msg.window_id);
            break;
        }
        case DWS_REQ_SHOW: {
            if (win) {
                printf("[DWS] SHOW: wid=%u '%s'\n", win->wid, win->title);
                win->mapped = true;
                stack_raise(win->wid);
                focused_window = win->wid;
                struct dws_message ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = DWS_EV_EXPOSE;
                ev.window_id = win->wid;
                ev.expose.ev_width = win->width;
                ev.expose.ev_height = win->height;
                send_event(c->fd, &ev);
                screen_dirty = true;
            }
            break;
        }
        case DWS_REQ_HIDE:
            if (win) { win->mapped = false; screen_dirty = true; }
            break;
        case DWS_REQ_MOVE:
            if (win) { win->x = msg.move.x; win->y = msg.move.y; screen_dirty = true; }
            break;
        case DWS_REQ_RESIZE:
            if (win) {
                win->width = msg.resize.width; win->height = msg.resize.height;
                win->client_w = msg.resize.width; win->client_h = msg.resize.height;
                if (win->pixmap) free(win->pixmap);
                win->pixmap = malloc(win->client_w * win->client_h * sizeof(uint32_t));
                if (win->pixmap) {
                    for (int i = 0; i < win->client_w * win->client_h; i++) win->pixmap[i] = win->bg_color;
                }
                struct dws_message ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = DWS_EV_RESIZED;
                ev.window_id = win->wid;
                ev.expose.ev_width = win->width;
                ev.expose.ev_height = win->height;
                send_event(c->fd, &ev);
                screen_dirty = true;
            }
            break;
        case DWS_REQ_SET_TITLE:
            if (win) { strncpy(win->title, msg.set_title.title, 63); screen_dirty = true; }
            break;
        case DWS_REQ_RAISE:
            if (win) { stack_raise(win->wid); screen_dirty = true; }
            break;
        case DWS_REQ_FILL_RECT:
            if (win) win_draw_fill_rect(win, msg.rect.x, msg.rect.y, msg.rect.width, msg.rect.height, msg.rect.color);
            break;
        case DWS_REQ_DRAW_RECT:
            if (win) win_draw_rect(win, msg.rect.x, msg.rect.y, msg.rect.width, msg.rect.height, msg.rect.color);
            break;
        case DWS_REQ_DRAW_LINE:
            if (win) win_draw_line(win, msg.line.x1, msg.line.y1, msg.line.x2, msg.line.y2, msg.line.color);
            break;
        case DWS_REQ_DRAW_TEXT:
            if (win) win_draw_string(win, msg.text.x, msg.text.y, msg.text.text, msg.text.color);
            break;
        case DWS_REQ_SET_PIXEL:
            if (win) win_draw_pixel(win, msg.pixel.x, msg.pixel.y, msg.pixel.color);
            break;
        case DWS_REQ_CLEAR:
            if (win) win_draw_fill_rect(win, 0, 0, win->client_w, win->client_h, msg.clear.color);
            break;
        case DWS_REQ_FLUSH:
            screen_dirty = true;
            break;
        case DWS_REQ_FILL_CIRCLE:
            if (win) win_fill_circle(win, msg.circle.cx, msg.circle.cy, msg.circle.radius, msg.circle.color);
            break;
        case DWS_REQ_DRAW_CIRCLE:
            if (win) win_draw_circle(win, msg.circle.cx, msg.circle.cy, msg.circle.radius, msg.circle.color);
            break;
        case DWS_REQ_BLIT_BUFFER: {
            if (win) {
                int bw = (int)msg.rect.width;
                int bh = (int)msg.rect.height;
                uint32_t vram_offset = msg.rect.color;

                if (vram_offset > 0 && drm_vram) {
                    /* Hardware DRI Zero-Copy Path: Client rendered directly into GPU VRAM */
                    uint32_t *src = (uint32_t *)((uint8_t *)drm_vram + vram_offset);
                    if (win->pixmap && bw == win->client_w && bh == win->client_h) {
                        memcpy(win->pixmap, src, (size_t)bw * bh * sizeof(uint32_t));
                    }
                    screen_dirty = true;
                } else if (win->pixmap) {
                    int bx = msg.rect.x;
                    int by = msg.rect.y;

                    if (bx < 0) bx = 0;
                    if (by < 0) by = 0;
                    if (bx + bw > win->client_w) bw = win->client_w - bx;
                    if (by + bh > win->client_h) bh = win->client_h - by;

                    size_t total = (size_t)bw * bh * sizeof(uint32_t);

                    /* Turn off non-blocking temporarily on client socket to read full strip */
                    int flags = fcntl(c->fd, F_GETFL, 0);
                    fcntl(c->fd, F_SETFL, flags & ~O_NONBLOCK);

                    size_t received = 0;
                    uint8_t *dest = (uint8_t *)(win->pixmap + (by * win->client_w + bx));
                    int retries = 0;
                    while (received < total) {
                        ssize_t r = recv(c->fd, dest + received, total - received, 0);
                        if (r < 0) {
                            if (r == -11) {
                                usleep(200);
                                if (++retries > 50) break;
                                continue;
                            }
                            break;
                        }
                        if (r == 0) break;
                        received += r;
                        retries = 0;
                    }

                    fcntl(c->fd, F_SETFL, flags);

                    if (by + bh >= win->client_h) {
                        screen_dirty = true;
                    }
                }
            }
            break;
        }
        }
    }
}

int main(void) {
    printf("Starting DUnix Windowing System (DWS - DWM Mode)...\n");

    /* 1. Attempt DRM/KMS Hardware Acceleration Initialization */
    drm_fd = drmOpen("/dev/dri/card0", NULL);
    if (drm_fd >= 0) {
        drmSetMaster(drm_fd);
        uint32_t bo1 = 0, pitch1 = 0; uint64_t size1 = 0;
        uint32_t bo2 = 0, pitch2 = 0; uint64_t size2 = 0;
        if (drm_create_dumb_buffer(drm_fd, SCREEN_WIDTH, SCREEN_HEIGHT, 32, &bo1, &pitch1, &size1) == 0 &&
            drm_create_dumb_buffer(drm_fd, SCREEN_WIDTH, SCREEN_HEIGHT, 32, &bo2, &pitch2, &size2) == 0) {
            drm_front_vram = (uint32_t *)drm_map_dumb_buffer(drm_fd, bo1, size1);
            drm_back_vram  = (uint32_t *)drm_map_dumb_buffer(drm_fd, bo2, size2);
            drmModeAddFB(drm_fd, SCREEN_WIDTH, SCREEN_HEIGHT, 24, 32, pitch1, bo1, &drm_front_fb);
            drmModeAddFB(drm_fd, SCREEN_WIDTH, SCREEN_HEIGHT, 24, 32, pitch2, bo2, &drm_back_fb);
            if (drm_front_vram && drm_back_vram && drm_front_fb && drm_back_fb) {
                drm_vram = (uint32_t *)mmap(NULL, 16 * 1024 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, 0);
                drmModeModeInfo mode;
                memset(&mode, 0, sizeof(mode));
                mode.hdisplay = SCREEN_WIDTH;
                mode.vdisplay = SCREEN_HEIGHT;
                mode.vrefresh = 60;
                drmModeSetCrtc(drm_fd, 1, drm_front_fb, 0, 0, NULL, 0, &mode);

                drm_hw_flip_active = true;
                drm_front_is_active = true;
                backbuffer = drm_back_vram;
                printf("[DWS] DRM/KMS Hardware Acceleration Active: Zero-Copy Double-Buffering & Hardware Page-Flipping on /dev/dri/card0\n");
            }
        }
    }

    /* 2. Fallback to classic /dev/fb0 if DRM not available */
    if (!drm_hw_flip_active) {
        fb_fd = open("/dev/fb0", O_RDWR);
        if (fb_fd < 0) {
            perror("dws: open /dev/fb0");
            return 1;
        }

        struct fb_var_screeninfo var;
        memset(&var, 0, sizeof(var));
        var.xres = SCREEN_WIDTH;
        var.yres = SCREEN_HEIGHT;
        var.bits_per_pixel = 32;
        ioctl(fb_fd, FBIOPUT_VSCREENINFO, &var);

        backbuffer = malloc(SCREEN_WIDTH * SCREEN_HEIGHT * 4);
        if (!backbuffer) {
            perror("dws: malloc backbuffer");
            return 1;
        }
        printf("[DWS] Operating on fallback framebuffer /dev/fb0\n");
    }

    mouse_fd = open("/dev/mouse", O_RDONLY | O_NONBLOCK);
    if (mouse_fd < 0) {
        perror("dws: open /dev/mouse");
    }

    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("dws: socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    fcntl(server_fd, F_SETFL, fcntl(server_fd, F_GETFL) | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(DWS_PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("dws: bind");
        return 1;
    }

    if (listen(server_fd, 16) < 0) {
        perror("dws: listen");
        return 1;
    }

    memset(windows, 0, sizeof(windows));
    memset(clients, 0, sizeof(clients));

    while (1) {
        int cfd = accept(server_fd, NULL, NULL);
        if (cfd >= 0) {
            fcntl(cfd, F_SETFL, fcntl(cfd, F_GETFL) | O_NONBLOCK);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (!clients[i].active) {
                    clients[i].fd = cfd;
                    clients[i].active = true;
                    clients[i].id = next_client_id++;
                    break;
                }
            }
        }

        if (mouse_fd >= 0) process_mouse();
        process_keyboard();

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].active) {
                handle_client_request(i);
            }
        }

        if (screen_dirty) {
            redraw_desktop();
            screen_dirty = false;
        }

        usleep(16000);
    }

    return 0;
}
