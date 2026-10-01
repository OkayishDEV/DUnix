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
#define NUM_TAGS       4

#define BAR_H          32
#define BORDER_W       1
#define TITLE_H        28

/* Modern Glassmorphic Dark UI Theme */
#define COLOR_WALLPAPER_TOP   0x000B0F19  /* Deep obsidian navy */
#define COLOR_WALLPAPER_MID   0x00141D30  /* Rich twilight blue */
#define COLOR_WALLPAPER_BOT   0x00080C14  /* Dark charcoal base */
#define COLOR_ROOT_GRID       0x00192234  /* High-tech subtle dot matrix */

#define COLOR_BAR_TOP         0x00111622  /* Translucent dark glass top */
#define COLOR_BAR_BOT         0x00161D2B  /* Translucent dark glass bottom */
#define COLOR_BAR_BORDER      0x00242E42  /* Luminous 1px separator */
#define COLOR_BAR_FG          0x0094A3B8  /* Soft slate text */
#define COLOR_BAR_FG_BRIGHT   0x00F8FAFC  /* Crisp white text */

#define COLOR_PILL_BG         0x001E2536  /* Dark glass pill */
#define COLOR_PILL_BORDER     0x002E384D  /* Pill border */
#define COLOR_PILL_HOVER      0x002B354C  /* Hover state */

#define COLOR_ACCENT_BLUE     0x002563EB  /* Vibrant electric blue */
#define COLOR_ACCENT_CYAN     0x0038BDF8  /* Bright cyan indicator */
#define COLOR_ACCENT_GREEN    0x0010B981  /* Vibrant emerald green */
#define COLOR_ACCENT_AMBER    0x00F59E0B  /* Warm amber */
#define COLOR_ACCENT_RED      0x00EF4444  /* Coral red */

/* Modern Window Colors */
#define COLOR_WIN_BORDER_ACT  0x003B82F6  /* Active window accent border */
#define COLOR_WIN_BORDER_NORM 0x00252D3D  /* Inactive window border */
#define COLOR_TITLE_ACT_TOP   0x001E2638  /* Active titlebar gradient top */
#define COLOR_TITLE_ACT_BOT   0x00161C2A  /* Active titlebar gradient bottom */
#define COLOR_TITLE_NORM_TOP  0x00141822  /* Inactive titlebar top */
#define COLOR_TITLE_NORM_BOT  0x0010131B  /* Inactive titlebar bottom */
#define COLOR_TITLE_SEP       0x0010141E  /* 1px titlebar bottom separator */
#define COLOR_TITLE_HIGHLIGHT 0x002E3A52  /* 1px titlebar top highlight */

/* Traffic Light Window Controls */
#define COLOR_BTN_CLOSE       0x00FF5F56  /* macOS Coral Red */
#define COLOR_BTN_CLOSE_HI    0x00FF3B30  /* Hover state */
#define COLOR_BTN_CLOSE_BOR   0x00E0443E  /* Border */
#define COLOR_BTN_MIN         0x00FFBD2E  /* macOS Amber */
#define COLOR_BTN_MIN_HI      0x00FF9500  /* Hover state */
#define COLOR_BTN_MIN_BOR     0x00DEA123  /* Border */
#define COLOR_BTN_MAX         0x0027C93F  /* macOS Emerald */
#define COLOR_BTN_MAX_HI      0x001CD036  /* Hover state */
#define COLOR_BTN_MAX_BOR     0x001AAB29  /* Border */

#define LAYOUT_TILED    0
#define LAYOUT_FLOATING 1
#define LAYOUT_MONOCLE  2

struct window {
    uint32_t wid;
    int client_fd;
    int x, y;             /* On-screen position (client area top-left) */
    int width, height;    /* Current display width/height */
    int client_w, client_h; /* Backing pixmap dimensions */
    uint32_t bg_color;
    uint32_t flags;
    bool mapped;
    int tag;              /* Workspace tag (1..4) */
    char title[64];
    uint32_t *pixmap;
    bool is_maximized;
    int saved_x, saved_y, saved_w, saved_h;
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

static bool start_menu_open = false;
static int start_menu_x = 6;
static int start_menu_y = 36;

static bool toast_visible = true;
static int toast_timer = 240; /* ~4.5 seconds */

/* Modern 18x18 Aerodynamic Cursor with Crisp Edge & Shadow */
static const uint8_t cursor_body[18][18] = {
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0},
    {1,2,2,2,2,1,1,1,1,1,1,0,0,0,0,0,0,0},
    {1,2,2,1,2,1,0,0,0,0,0,0,0,0,0,0,0,0},
    {1,2,1,0,1,2,1,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,0,0,1,2,1,0,0,0,0,0,0,0,0,0,0,0},
    {1,0,0,0,0,1,2,1,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,1,2,1,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

static inline int iabs(int v) { return v < 0 ? -v : v; }

/* Safe channel-separated 32-bit alpha blending */
static inline uint32_t blend_color(uint32_t bg, uint32_t fg, uint8_t alpha) {
    uint32_t inv_a = 255 - alpha;
    uint32_t r = (((bg >> 16) & 0xFF) * inv_a + ((fg >> 16) & 0xFF) * alpha) >> 8;
    uint32_t g = (((bg >> 8) & 0xFF) * inv_a + ((fg >> 8) & 0xFF) * alpha) >> 8;
    uint32_t b = ((bg & 0xFF) * inv_a + (fg & 0xFF) * alpha) >> 8;
    return (r << 16) | (g << 8) | b;
}

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

/* Multi-layer soft drop shadow */
static void draw_shadow_rect(int x, int y, int w, int h, int shadow_size) {
    static const uint8_t shadow_alphas[] = { 65, 45, 30, 18, 10, 4 };
    if (shadow_size > 6) shadow_size = 6;
    for (int s = shadow_size; s >= 1; s--) {
        uint8_t a = shadow_alphas[s - 1];
        int sx = x - s;
        int sy = y - s + 2; /* Natural downward lighting bias */
        int sw = w + 2 * s;
        int sh = h + 2 * s;

        /* Top line */
        int y_top = sy;
        if (y_top >= BAR_H && y_top < SCREEN_HEIGHT) {
            int x1 = sx < 0 ? 0 : sx;
            int x2 = sx + sw > SCREEN_WIDTH ? SCREEN_WIDTH : sx + sw;
            for (int px = x1; px < x2; px++) {
                backbuffer[y_top * SCREEN_WIDTH + px] = blend_color(backbuffer[y_top * SCREEN_WIDTH + px], 0x000000, a);
            }
        }
        /* Bottom line */
        int y_bot = sy + sh - 1;
        if (y_bot >= BAR_H && y_bot < SCREEN_HEIGHT) {
            int x1 = sx < 0 ? 0 : sx;
            int x2 = sx + sw > SCREEN_WIDTH ? SCREEN_WIDTH : sx + sw;
            for (int px = x1; px < x2; px++) {
                backbuffer[y_bot * SCREEN_WIDTH + px] = blend_color(backbuffer[y_bot * SCREEN_WIDTH + px], 0x000000, a);
            }
        }
        /* Left line */
        int x_left = sx;
        if (x_left >= 0 && x_left < SCREEN_WIDTH) {
            int y1 = sy < BAR_H ? BAR_H : sy;
            int y2 = sy + sh > SCREEN_HEIGHT ? SCREEN_HEIGHT : sy + sh;
            for (int py = y1; py < y2; py++) {
                backbuffer[py * SCREEN_WIDTH + x_left] = blend_color(backbuffer[py * SCREEN_WIDTH + x_left], 0x000000, a);
            }
        }
        /* Right line */
        int x_right = sx + sw - 1;
        if (x_right >= 0 && x_right < SCREEN_WIDTH) {
            int y1 = sy < BAR_H ? BAR_H : sy;
            int y2 = sy + sh > SCREEN_HEIGHT ? SCREEN_HEIGHT : sy + sh;
            for (int py = y1; py < y2; py++) {
                backbuffer[py * SCREEN_WIDTH + x_right] = blend_color(backbuffer[py * SCREEN_WIDTH + x_right], 0x000000, a);
            }
        }
    }
}

/* Rounded rectangle fill */
static void draw_rounded_rect_fill(int x, int y, int w, int h, int r, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;

    draw_fill_rect(x, y + r, w, h - 2 * r, color);
    draw_fill_rect(x + r, y, w - 2 * r, r, color);
    draw_fill_rect(x + r, y + h - r, w - 2 * r, r, color);

    for (int cy = 0; cy < r; cy++) {
        for (int cx = 0; cx < r; cx++) {
            int dx = r - cx - 1;
            int dy = r - cy - 1;
            if (dx * dx + dy * dy <= r * r) {
                draw_pixel(x + cx, y + cy, color);
                draw_pixel(x + w - 1 - cx, y + cy, color);
                draw_pixel(x + cx, y + h - 1 - cy, color);
                draw_pixel(x + w - 1 - cx, y + h - 1 - cy, color);
            }
        }
    }
}

/* Top-only rounded rectangle fill for titlebars */
static void draw_top_rounded_rect_fill(int x, int y, int w, int h, int r, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h) r = h;

    draw_fill_rect(x, y + r, w, h - r, color);
    draw_fill_rect(x + r, y, w - 2 * r, r, color);
    for (int cy = 0; cy < r; cy++) {
        for (int cx = 0; cx < r; cx++) {
            int dx = r - cx - 1;
            int dy = r - cy - 1;
            if (dx * dx + dy * dy <= r * r) {
                draw_pixel(x + cx, y + cy, color);
                draw_pixel(x + w - 1 - cx, y + cy, color);
            }
        }
    }
}

/* Crisp anti-aliased circle with optional border */
static void draw_smooth_circle(int cx, int cy, int radius, uint32_t fill_color, uint32_t border_color) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            int dist_sq = x * x + y * y;
            if (dist_sq <= radius * radius) {
                uint32_t col = (dist_sq >= (radius - 1) * (radius - 1) && border_color != 0) ? border_color : fill_color;
                draw_pixel(cx + x, cy + y, col);
            }
        }
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

/* Modern Tiling Engine with Gaps */
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

    int gap = 8;
    int area_x = gap;
    int area_y = BAR_H + gap + TITLE_H;
    int area_w = SCREEN_WIDTH - 2 * gap;
    int area_h = SCREEN_HEIGHT - BAR_H - 2 * gap - TITLE_H;

    if (count == 1) {
        visible[0]->x = area_x;
        visible[0]->y = area_y;
        visible[0]->width = area_w;
        visible[0]->height = area_h;
    } else if (count == 2) {
        int mw = (area_w * 54) / 100;
        visible[0]->x = area_x;
        visible[0]->y = area_y;
        visible[0]->width = mw - gap / 2;
        visible[0]->height = area_h;

        visible[1]->x = area_x + mw + gap / 2;
        visible[1]->y = area_y;
        visible[1]->width = area_w - mw - gap / 2;
        visible[1]->height = area_h;
    } else {
        int mw = (area_w * 54) / 100;
        visible[0]->x = area_x;
        visible[0]->y = area_y;
        visible[0]->width = mw - gap / 2;
        visible[0]->height = area_h;

        int nstack = count - 1;
        int sh = (area_h + TITLE_H) / nstack;
        for (int i = 0; i < nstack; i++) {
            visible[1 + i]->x = area_x + mw + gap / 2;
            visible[1 + i]->y = area_y + i * sh;
            visible[1 + i]->width = area_w - mw - gap / 2;
            visible[1 + i]->height = sh - TITLE_H - gap;
            if (visible[1 + i]->height < 10) visible[1 + i]->height = 10;
        }
    }
}

/* High-resolution procedural luxury wallpaper */
static void draw_wallpaper(void) {
    /* 1. Deep luxury vertical gradient */
    for (int y = BAR_H; y < SCREEN_HEIGHT; y++) {
        float t = (float)(y - BAR_H) / (float)(SCREEN_HEIGHT - BAR_H);
        uint8_t r, g, b;
        if (t < 0.55f) {
            float k = t / 0.55f;
            r = (uint8_t)(11 + (20 - 11) * k);
            g = (uint8_t)(15 + (29 - 15) * k);
            b = (uint8_t)(25 + (48 - 25) * k);
        } else if (t < 0.75f) {
            float k = (t - 0.55f) / 0.20f;
            r = (uint8_t)(20 + (24 - 20) * k);
            g = (uint8_t)(29 + (42 - 29) * k);
            b = (uint8_t)(48 + (76 - 48) * k);
        } else {
            float k = (t - 0.75f) / 0.25f;
            r = (uint8_t)(24 + (8 - 24) * k);
            g = (uint8_t)(42 + (12 - 42) * k);
            b = (uint8_t)(76 + (20 - 76) * k);
        }
        uint32_t line_color = (r << 16) | (g << 8) | b;
        uint32_t *row = &backbuffer[y * SCREEN_WIDTH];
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            row[x] = line_color;
        }
    }

    /* 2. Modern subtle carbon dot grid */
    for (int y = BAR_H + 20; y < SCREEN_HEIGHT - 20; y += 32) {
        for (int x = 20; x < SCREEN_WIDTH - 20; x += 32) {
            backbuffer[y * SCREEN_WIDTH + x] = blend_color(backbuffer[y * SCREEN_WIDTH + x], 0x0064748B, 40);
        }
    }

    /* 3. Center Desktop Emblem & Brand Watermark */
    int cx = SCREEN_WIDTH / 2;
    int cy = 340;

    int d_size = 28;
    for (int i = 0; i <= d_size; i++) {
        draw_line(cx - i, cy - d_size + i, cx + i, cy - d_size + i, 0x00151E2E);
        draw_line(cx - i, cy + d_size - i, cx + i, cy + d_size - i, 0x00151E2E);
    }
    draw_line(cx, cy - d_size, cx + d_size, cy, 0x0038BDF8);
    draw_line(cx + d_size, cy, cx, cy + d_size, 0x002563EB);
    draw_line(cx, cy + d_size, cx - d_size, cy, 0x0038BDF8);
    draw_line(cx - d_size, cy, cx, cy - d_size, 0x002563EB);

    draw_string(cx - 4, cy - 8, "D", COLOR_BAR_FG_BRIGHT);

    const char *logo_text = "D U N I X";
    int lw = (int)strlen(logo_text) * 8;
    draw_string(cx - lw / 2, cy + 42, logo_text, 0x0094A3B8);

    const char *sub_text = "64-BIT ADVANCED OPERATING SYSTEM";
    int sw = (int)strlen(sub_text) * 8;
    draw_string(cx - sw / 2, cy + 62, sub_text, 0x00475569);

    const char *spec_text = "DUnix Workstation 2.0  *  DRM/KMS Accelerated  *  x86_64";
    int spw = (int)strlen(spec_text) * 8;
    draw_string(cx - spw / 2, SCREEN_HEIGHT - 22, spec_text, 0x00334155);
}

/* Modern Glassmorphic Top Bar & System Tray */
static void draw_bar(void) {
    /* 1. Bar background gradient */
    for (int y = 0; y < BAR_H - 1; y++) {
        float t = (float)y / (float)BAR_H;
        uint32_t bg = blend_color(COLOR_BAR_TOP, COLOR_BAR_BOT, (uint8_t)(t * 255.0f));
        uint32_t *row = &backbuffer[y * SCREEN_WIDTH];
        for (int x = 0; x < SCREEN_WIDTH; x++) row[x] = bg;
    }
    draw_line(0, BAR_H - 1, SCREEN_WIDTH - 1, BAR_H - 1, COLOR_BAR_BORDER);

    /* 2. Start Menu Button: [ ❖ DUnix ] */
    bool start_hover = (mouse_x >= 6 && mouse_x < 90 && mouse_y < BAR_H);
    uint32_t s_bg = (start_menu_open || start_hover) ? COLOR_ACCENT_BLUE : COLOR_PILL_BG;
    uint32_t s_bor = (start_menu_open || start_hover) ? 0x0060A5FA : COLOR_PILL_BORDER;
    draw_rounded_rect_fill(6, 4, 82, 24, 4, s_bg);
    draw_rect(6, 4, 82, 24, s_bor);
    draw_line(13, 16, 17, 11, COLOR_ACCENT_CYAN);
    draw_line(17, 11, 21, 16, COLOR_ACCENT_CYAN);
    draw_line(21, 16, 17, 21, COLOR_ACCENT_CYAN);
    draw_line(17, 21, 13, 16, COLOR_ACCENT_CYAN);
    draw_pixel(17, 16, COLOR_BAR_FG_BRIGHT);
    draw_string(27, 8, "DUnix", COLOR_BAR_FG_BRIGHT);

    /* 3. Workspace Switcher: [ 1 ] [ 2 ] [ 3 ] [ 4 ] */
    int tx = 94;
    for (int t = 1; t <= NUM_TAGS; t++) {
        int tw = 26;
        bool active = (t == current_tag);
        bool has_wins = false;
        for (int i = 0; i < window_count; i++) {
            if (windows[i].mapped && windows[i].tag == t) {
                has_wins = true; break;
            }
        }
        uint32_t wbg = active ? COLOR_ACCENT_BLUE : COLOR_PILL_BG;
        uint32_t wfg = active ? COLOR_BAR_FG_BRIGHT : (has_wins ? COLOR_BAR_FG_BRIGHT : COLOR_BAR_FG);
        draw_rounded_rect_fill(tx, 4, tw, 24, 4, wbg);
        draw_rect(tx, 4, tw, 24, active ? 0x0060A5FA : COLOR_PILL_BORDER);

        if (has_wins && !active) {
            draw_smooth_circle(tx + 5, 8, 2, COLOR_ACCENT_CYAN, 0);
        }
        char tstr[4];
        snprintf(tstr, sizeof(tstr), "%d", t);
        draw_string(tx + (tw - 8) / 2, 8, tstr, wfg);
        tx += tw + 4;
    }

    /* 4. Quick Launch Dock */
    struct { const char *label; int w; const char *cmd; } launchers[] = {
        { ">_ Term",  58, "/bin/dterm" },
        { "@ Web",    50, "/bin/dweb" },
        { "+ Files",  58, "/bin/dfiles" },
        { "= Calc",   50, "/bin/dcalc" },
        { "o Clock",  56, "/bin/dclock" },
        { "* 3D",     46, "/bin/glgears" },
        { "! Games",  58, "/bin/snake" }
    };
    int lx = 216;
    for (size_t i = 0; i < sizeof(launchers)/sizeof(launchers[0]); i++) {
        int lw = launchers[i].w;
        bool hover = (mouse_x >= lx && mouse_x < lx + lw && mouse_y < BAR_H);
        draw_rounded_rect_fill(lx, 4, lw, 24, 4, hover ? COLOR_PILL_HOVER : COLOR_PILL_BG);
        draw_rect(lx, 4, lw, 24, hover ? 0x00475569 : COLOR_PILL_BORDER);
        draw_string(lx + 6, 8, launchers[i].label, hover ? COLOR_BAR_FG_BRIGHT : COLOR_BAR_FG);
        lx += lw + 4;
    }

    /* 5. Running Taskbar Pills */
    int task_x = lx + 8;
    for (int i = 0; i < z_count; i++) {
        struct window *w = find_window(z_order[i]);
        if (!w || w->tag != current_tag) continue;
        if (task_x + 90 > SCREEN_WIDTH - 360) break;

        bool is_foc = (w->wid == focused_window);
        int tw = 88;
        bool hover = (mouse_x >= task_x && mouse_x < task_x + tw && mouse_y < BAR_H);
        uint32_t tbg = is_foc ? 0x001E3A8A : (hover ? COLOR_PILL_HOVER : COLOR_PILL_BG);
        uint32_t tbor = is_foc ? COLOR_ACCENT_BLUE : COLOR_PILL_BORDER;
        draw_rounded_rect_fill(task_x, 4, tw, 24, 4, tbg);
        draw_rect(task_x, 4, tw, 24, tbor);

        draw_smooth_circle(task_x + 7, 16, 3, w->mapped ? COLOR_ACCENT_GREEN : COLOR_ACCENT_AMBER, 0);

        char ttitle[10];
        strncpy(ttitle, w->title, 8);
        ttitle[8] = '\0';
        draw_string(task_x + 14, 8, ttitle, is_foc ? COLOR_BAR_FG_BRIGHT : COLOR_BAR_FG);

        task_x += tw + 4;
    }

    /* 6. System Status Tray (Right side) */
    int px = SCREEN_WIDTH - 28;
    bool p_hover = (mouse_x >= px && mouse_x < px + 22 && mouse_y < BAR_H);
    draw_rounded_rect_fill(px, 4, 22, 24, 4, p_hover ? COLOR_ACCENT_RED : COLOR_PILL_BG);
    draw_rect(px, 4, 22, 24, p_hover ? 0x00DC2626 : COLOR_PILL_BORDER);
    draw_smooth_circle(px + 11, 16, 5, p_hover ? COLOR_ACCENT_RED : COLOR_PILL_BG, COLOR_BAR_FG_BRIGHT);
    draw_line(px + 11, 10, px + 11, 15, COLOR_BAR_FG_BRIGHT);
    draw_pixel(px + 11, 11, p_hover ? COLOR_ACCENT_RED : COLOR_PILL_BG);

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[32];
    if (tm_info) {
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    } else {
        strcpy(time_str, "00:00:00");
    }
    int clk_w = 76;
    int clk_x = px - clk_w - 4;
    draw_rounded_rect_fill(clk_x, 4, clk_w, 24, 4, COLOR_PILL_BG);
    draw_rect(clk_x, 4, clk_w, 24, COLOR_PILL_BORDER);
    draw_string(clk_x + 6, 8, time_str, COLOR_BAR_FG_BRIGHT);

    int ram_w = 64;
    int ram_x = clk_x - ram_w - 4;
    draw_rounded_rect_fill(ram_x, 4, ram_w, 24, 4, COLOR_PILL_BG);
    draw_rect(ram_x, 4, ram_w, 24, COLOR_PILL_BORDER);
    draw_string(ram_x + 6, 8, "128 MB", COLOR_ACCENT_AMBER);

    int net_w = 98;
    int net_x = ram_x - net_w - 4;
    draw_rounded_rect_fill(net_x, 4, net_w, 24, 4, COLOR_PILL_BG);
    draw_rect(net_x, 4, net_w, 24, COLOR_PILL_BORDER);
    draw_smooth_circle(net_x + 8, 16, 3, COLOR_ACCENT_GREEN, 0);
    draw_string(net_x + 16, 8, "10.0.2.15", COLOR_BAR_FG_BRIGHT);

    const char *lay_str = (current_layout == LAYOUT_TILED) ? "Tile" : "Float";
    int lay_w = 54;
    int lay_x = net_x - lay_w - 4;
    draw_rounded_rect_fill(lay_x, 4, lay_w, 24, 4, COLOR_PILL_BG);
    draw_rect(lay_x, 4, lay_w, 24, COLOR_PILL_BORDER);
    draw_string(lay_x + 8, 8, lay_str, COLOR_ACCENT_CYAN);
}

/* Modern Start Menu Popup */
static void draw_start_menu(void) {
    if (!start_menu_open) return;

    int mx = start_menu_x;
    int my = start_menu_y;
    int mw = 236;
    int mh = 330;

    draw_shadow_rect(mx, my, mw, mh, 5);
    draw_rounded_rect_fill(mx, my, mw, mh, 6, 0x00131824);
    draw_rect(mx, my, mw, mh, 0x002B374C);

    draw_top_rounded_rect_fill(mx, my, mw, 38, 6, 0x001A2234);
    draw_line(mx, my + 38, mx + mw - 1, my + 38, 0x002B374C);
    draw_line(mx + 12, my + 15, mx + 16, my + 10, COLOR_ACCENT_CYAN);
    draw_line(mx + 16, my + 10, mx + 20, my + 15, COLOR_ACCENT_CYAN);
    draw_line(mx + 20, my + 15, mx + 16, my + 20, COLOR_ACCENT_CYAN);
    draw_line(mx + 16, my + 20, mx + 12, my + 15, COLOR_ACCENT_CYAN);
    draw_string(mx + 26, my + 8, "DUNIX WORKSTATION", COLOR_ACCENT_CYAN);
    draw_string(mx + 12, my + 22, "64-Bit Desktop v0.8.0", 0x0064748B);

    struct { const char *icon; const char *name; const char *cmd; } items[] = {
        { ">_", "Terminal Emulator", "/bin/dterm" },
        { "W ", "DWeb Browser",      "/bin/dweb" },
        { "F ", "File Manager",      "/bin/dfiles" },
        { "= ", "Calculator",        "/bin/dcalc" },
        { "C ", "System Clock",      "/bin/dclock" },
        { "* ", "3D OpenGL Demo",    "/bin/glgears" },
        { "S ", "Snake Arcade",      "/bin/snake" },
        { "M ", "Minesweeper",       "/bin/minesweeper" },
        { "# ", "2048 Puzzle",       "/bin/2048" },
        { "I ", "OS Installer",      "/bin/dinstall" }
    };
    int item_y = my + 44;
    for (size_t i = 0; i < sizeof(items)/sizeof(items[0]); i++) {
        bool hover = (mouse_x >= mx + 6 && mouse_x < mx + mw - 6 &&
                      mouse_y >= item_y && mouse_y < item_y + 20);
        if (hover) {
            draw_rounded_rect_fill(mx + 6, item_y, mw - 12, 20, 3, 0x001E283C);
            draw_fill_rect(mx + 6, item_y + 3, 3, 14, COLOR_ACCENT_CYAN);
        }
        draw_string(mx + 14, item_y + 2, items[i].icon, hover ? COLOR_ACCENT_CYAN : COLOR_BAR_FG);
        draw_string(mx + 36, item_y + 2, items[i].name, hover ? COLOR_BAR_FG_BRIGHT : COLOR_BAR_FG);
        item_y += 22;
    }

    draw_line(mx + 10, item_y + 3, mx + mw - 10, item_y + 3, 0x002B374C);
    item_y += 8;

    bool reboot_hover = (mouse_x >= mx + 6 && mouse_x < mx + mw - 6 && mouse_y >= item_y && mouse_y < item_y + 20);
    if (reboot_hover) {
        draw_rounded_rect_fill(mx + 6, item_y, mw - 12, 20, 3, 0x001E283C);
    }
    draw_string(mx + 14, item_y + 2, "R ", reboot_hover ? COLOR_ACCENT_AMBER : COLOR_BAR_FG);
    draw_string(mx + 36, item_y + 2, "Restart System", reboot_hover ? COLOR_BAR_FG_BRIGHT : COLOR_BAR_FG);
    item_y += 22;

    bool power_hover = (mouse_x >= mx + 6 && mouse_x < mx + mw - 6 && mouse_y >= item_y && mouse_y < item_y + 20);
    if (power_hover) {
        draw_rounded_rect_fill(mx + 6, item_y, mw - 12, 20, 3, 0x003B1A1A);
    }
    draw_string(mx + 14, item_y + 2, "X ", power_hover ? COLOR_ACCENT_RED : COLOR_BAR_FG);
    draw_string(mx + 36, item_y + 2, "Power Off", power_hover ? COLOR_BAR_FG_BRIGHT : COLOR_BAR_FG);
}

/* Modern Notification Toast */
static void draw_toast(void) {
    if (!toast_visible || toast_timer <= 0) return;

    int tx = SCREEN_WIDTH - 276;
    int ty = BAR_H + 10;
    int tw = 266;
    int th = 46;

    draw_shadow_rect(tx, ty, tw, th, 4);
    draw_rounded_rect_fill(tx, ty, tw, th, 5, 0x00131924);
    draw_rect(tx, ty, tw, th, 0x002A354C);

    draw_fill_rect(tx + 2, ty + 6, 4, th - 12, COLOR_ACCENT_BLUE);
    draw_string(tx + 14, ty + 8, "DUnix Workstation 2.0", COLOR_BAR_FG_BRIGHT);
    draw_string(tx + 14, ty + 24, "DRM/KMS Ready (1024x768)", 0x0094A3B8);
    draw_string(tx + tw - 16, ty + 8, "x", 0x0064748B);

    toast_timer--;
    if (toast_timer <= 0) toast_visible = false;
}

/* Full Desktop Redraw */
static void redraw_desktop(void) {
    /* 1. Procedural luxury wallpaper */
    draw_wallpaper();

    /* 2. Recalculate tiling layout if active */
    arrange_windows();

    /* 3. Draw mapped windows for current tag */
    for (int i = 0; i < z_count; i++) {
        struct window *win = find_window(z_order[i]);
        if (!win || !win->mapped || (win->tag != current_tag)) continue;

        bool is_focused = (win->wid == focused_window);
        int win_top = win->y - TITLE_H;
        if (win_top < BAR_H) win_top = BAR_H;
        int full_h = win->height + (win->y - win_top);

        /* Soft Drop Shadow */
        draw_shadow_rect(win->x, win_top, win->width, full_h, is_focused ? 6 : 3);

        /* Window outer accent border */
        uint32_t bcolor = is_focused ? COLOR_WIN_BORDER_ACT : COLOR_WIN_BORDER_NORM;
        draw_rect(win->x - BORDER_W, win_top - BORDER_W,
                  win->width + 2 * BORDER_W, full_h + 2 * BORDER_W, bcolor);

        /* Titlebar gradient with rounded corners */
        for (int r = 0; r < (win->y - win_top); r++) {
            float t = (float)r / (float)(win->y - win_top);
            uint32_t t_top = is_focused ? COLOR_TITLE_ACT_TOP : COLOR_TITLE_NORM_TOP;
            uint32_t t_bot = is_focused ? COLOR_TITLE_ACT_BOT : COLOR_TITLE_NORM_BOT;
            uint32_t row_col = blend_color(t_top, t_bot, (uint8_t)(t * 255.0f));
            for (int c = 0; c < win->width; c++) {
                draw_pixel(win->x + c, win_top + r, row_col);
            }
        }
        draw_line(win->x, win_top, win->x + win->width - 1, win_top, is_focused ? COLOR_TITLE_HIGHLIGHT : COLOR_WIN_BORDER_NORM);
        draw_line(win->x, win->y - 1, win->x + win->width - 1, win->y - 1, COLOR_TITLE_SEP);

        /* macOS Traffic Light Buttons */
        int btn_cy = win_top + (win->y - win_top) / 2;
        int close_cx = win->x + 14;
        int min_cx   = win->x + 28;
        int max_cx   = win->x + 42;

        bool close_hov = (mouse_x >= close_cx - 6 && mouse_x <= close_cx + 6 && mouse_y >= btn_cy - 6 && mouse_y <= btn_cy + 6);
        bool min_hov   = (mouse_x >= min_cx - 6 && mouse_x <= min_cx + 6 && mouse_y >= btn_cy - 6 && mouse_y <= btn_cy + 6);
        bool max_hov   = (mouse_x >= max_cx - 6 && mouse_x <= max_cx + 6 && mouse_y >= btn_cy - 6 && mouse_y <= btn_cy + 6);

        draw_smooth_circle(close_cx, btn_cy, 5, close_hov ? COLOR_BTN_CLOSE_HI : COLOR_BTN_CLOSE, COLOR_BTN_CLOSE_BOR);
        draw_smooth_circle(min_cx,   btn_cy, 5, min_hov   ? COLOR_BTN_MIN_HI   : COLOR_BTN_MIN,   COLOR_BTN_MIN_BOR);
        draw_smooth_circle(max_cx,   btn_cy, 5, max_hov   ? COLOR_BTN_MAX_HI   : COLOR_BTN_MAX,   COLOR_BTN_MAX_BOR);

        /* Centered Window Title */
        char title_buf[64];
        int max_chars = (win->width - 64) / 8;
        if (max_chars > 60) max_chars = 60;
        if (max_chars < 4) max_chars = 4;
        strncpy(title_buf, win->title, max_chars);
        title_buf[max_chars] = '\0';

        int title_w = (int)strlen(title_buf) * 8;
        int tx = win->x + (win->width - title_w) / 2;
        if (tx < win->x + 58) tx = win->x + 58;
        draw_string(tx, win_top + 6, title_buf, is_focused ? COLOR_BAR_FG_BRIGHT : COLOR_BAR_FG);

        /* Client area background */
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

    /* 4. Draw modern top status bar */
    draw_bar();

    /* 5. Draw toast notification */
    draw_toast();

    /* 6. Draw start menu popup if open */
    draw_start_menu();

    /* 7. Draw modern aerodynamic cursor with soft drop shadow */
    for (int cy = 0; cy < 18; cy++) {
        for (int cx = 0; cx < 18; cx++) {
            int px = mouse_x + cx;
            int py = mouse_y + cy;
            if (px >= 0 && px < SCREEN_WIDTH && py >= 0 && py < SCREEN_HEIGHT) {
                uint8_t pixel_type = cursor_body[cy][cx];
                if (pixel_type == 2) {
                    draw_pixel(px, py, 0x00FFFFFF);
                } else if (pixel_type == 1) {
                    draw_pixel(px, py, 0x00000000);
                } else if (pixel_type == 0 && cx > 0 && cy > 0 && cursor_body[cy - 1][cx - 1] != 0) {
                    backbuffer[py * SCREEN_WIDTH + px] = blend_color(backbuffer[py * SCREEN_WIDTH + px], 0x000000, 75);
                }
            }
        }
    }

    /* 8. Commit to framebuffer / DRM Scanout */
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
    /* 1. Start menu button toggle */
    if (x >= 6 && x < 90) {
        start_menu_open = !start_menu_open;
        start_menu_x = 6;
        start_menu_y = BAR_H + 4;
        screen_dirty = true;
        return;
    }

    /* 2. Workspace tags: 1, 2, 3, 4 */
    if (x >= 94 && x < 94 + 4 * 30) {
        int clicked_tag = (x - 94) / 30 + 1;
        if (clicked_tag >= 1 && clicked_tag <= NUM_TAGS) {
            current_tag = clicked_tag;
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

    /* 3. Quick Launchers */
    if (x >= 216 && x < 274) { launch_app("/bin/dterm"); return; }
    if (x >= 274 && x < 324) { launch_app("/bin/dweb"); return; }
    if (x >= 328 && x < 386) { launch_app("/bin/dfiles"); return; }
    if (x >= 388 && x < 438) { launch_app("/bin/dcalc"); return; }
    if (x >= 442 && x < 498) { launch_app("/bin/dclock"); return; }
    if (x >= 502 && x < 548) { launch_app("/bin/glgears"); return; }
    if (x >= 552 && x < 612) { launch_app("/bin/snake"); return; }

    /* 4. Running taskbar items */
    int task_x = 620;
    for (int i = 0; i < z_count; i++) {
        struct window *w = find_window(z_order[i]);
        if (!w || w->tag != current_tag) continue;
        if (task_x + 90 > SCREEN_WIDTH - 360) break;
        if (x >= task_x && x < task_x + 88) {
            if (!w->mapped) {
                w->mapped = true;
                stack_raise(w->wid);
                focused_window = w->wid;
            } else if (focused_window == w->wid) {
                w->mapped = false;
                focused_window = 0;
                for (int j = z_count - 1; j >= 0; j--) {
                    struct window *nw = find_window(z_order[j]);
                    if (nw && nw->mapped && nw->tag == current_tag) {
                        focused_window = nw->wid;
                        break;
                    }
                }
            } else {
                stack_raise(w->wid);
                focused_window = w->wid;
            }
            screen_dirty = true;
            return;
        }
        task_x += 92;
    }

    /* 5. System Tray Clicks */
    /* Power button */
    if (x >= SCREEN_WIDTH - 28) {
        launch_app("/bin/poweroff");
        return;
    }

    /* Layout Switcher */
    int lay_x = SCREEN_WIDTH - 28 - 4 - 76 - 4 - 64 - 4 - 98 - 4 - 54;
    if (x >= lay_x && x < lay_x + 54) {
        current_layout = (current_layout == LAYOUT_TILED) ? LAYOUT_FLOATING : LAYOUT_TILED;
        screen_dirty = true;
        return;
    }
}

static bool handle_start_menu_click(int x, int y) {
    if (!start_menu_open) return false;

    int mx = start_menu_x;
    int my = start_menu_y;
    int mw = 236;
    int mh = 330;

    if (x < mx || x >= mx + mw || y < my || y >= my + mh) {
        start_menu_open = false;
        screen_dirty = true;
        return true;
    }

    const char *cmds[] = {
        "/bin/dterm", "/bin/dweb", "/bin/dfiles", "/bin/dcalc",
        "/bin/dclock", "/bin/glgears", "/bin/snake", "/bin/minesweeper",
        "/bin/2048", "/bin/dinstall"
    };

    int item_y = my + 44;
    for (int i = 0; i < 10; i++) {
        if (x >= mx + 6 && x < mx + mw - 6 && y >= item_y && y < item_y + 20) {
            launch_app(cmds[i]);
            start_menu_open = false;
            screen_dirty = true;
            return true;
        }
        item_y += 22;
    }

    item_y += 8;
    if (x >= mx + 6 && x < mx + mw - 6 && y >= item_y && y < item_y + 20) {
        launch_app("/bin/reboot");
        start_menu_open = false;
        screen_dirty = true;
        return true;
    }
    item_y += 22;
    if (x >= mx + 6 && x < mx + mw - 6 && y >= item_y && y < item_y + 20) {
        launch_app("/bin/poweroff");
        start_menu_open = false;
        screen_dirty = true;
        return true;
    }

    return true;
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
        /* Bit 3 must be 1 and overflow bits 6 and 7 must be 0 for valid byte 0 */
        if (!(mbuf[pos] & 0x08) || (mbuf[pos] & 0xC0)) {
            pos++;
            continue;
        }

        uint8_t b0 = mbuf[pos];
        uint8_t b1 = mbuf[pos + 1];
        uint8_t b2 = mbuf[pos + 2];
        pos += 3;

        int dx = (b0 & 0x10) ? (int)b1 - 256 : (int)b1;
        int dy = (b0 & 0x20) ? (int)b2 - 256 : (int)b2;

        /* Clamp outlier deltas to prevent teleportation to screen edges */
        if (dx < -127) dx = -127;
        if (dx > 127)  dx = 127;
        if (dy < -127) dy = -127;
        if (dy > 127)  dy = 127;

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
                current_layout = LAYOUT_FLOATING;
                win->x = mouse_x - drag_offset_x;
                win->y = mouse_y - drag_offset_y;
                if (win->y < BAR_H + TITLE_H + BORDER_W) win->y = BAR_H + TITLE_H + BORDER_W;
            }
            if (l_up || r_up) is_dragging = false;
        } else {
            /* Dismiss toast notification on click */
            if (l_down && toast_visible && mouse_x >= SCREEN_WIDTH - 276 && mouse_y >= BAR_H + 10 && mouse_y < BAR_H + 56) {
                toast_visible = false;
                screen_dirty = true;
                continue;
            }

            /* Start Menu click handling */
            if (l_down && start_menu_open) {
                if (handle_start_menu_click(mouse_x, mouse_y)) {
                    continue;
                }
            }

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

                /* Right click on empty desktop: Open context start menu under cursor */
                if (r_down && !hit) {
                    start_menu_x = mouse_x;
                    start_menu_y = mouse_y;
                    if (start_menu_x + 236 > SCREEN_WIDTH) start_menu_x = SCREEN_WIDTH - 236;
                    if (start_menu_y + 330 > SCREEN_HEIGHT) start_menu_y = SCREEN_HEIGHT - 330;
                    start_menu_open = true;
                    screen_dirty = true;
                    continue;
                }

                if (hit) {
                    int h_top = hit->y - TITLE_H;
                    if (h_top < BAR_H) h_top = BAR_H;

                    /* Focus window */
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

                    int btn_cy = h_top + (hit->y - h_top) / 2;
                    int close_cx = hit->x + 14;
                    int min_cx   = hit->x + 28;
                    int max_cx   = hit->x + 42;

                    /* 1. Traffic Light Button Clicks */
                    if (l_down) {
                        /* Close Button */
                        if ((mouse_x - close_cx)*(mouse_x - close_cx) + (mouse_y - btn_cy)*(mouse_y - btn_cy) <= 49) {
                            struct dws_message ev;
                            memset(&ev, 0, sizeof(ev));
                            ev.type = DWS_EV_CLOSE_REQ;
                            ev.window_id = hit->wid;
                            send_event(hit->client_fd, &ev);
                            destroy_window(hit->wid);
                            break;
                        }
                        /* Minimize Button */
                        if ((mouse_x - min_cx)*(mouse_x - min_cx) + (mouse_y - btn_cy)*(mouse_y - btn_cy) <= 49) {
                            hit->mapped = false;
                            focused_window = 0;
                            for (int j = z_count - 1; j >= 0; j--) {
                                struct window *nw = find_window(z_order[j]);
                                if (nw && nw->mapped && nw->tag == current_tag) {
                                    focused_window = nw->wid;
                                    break;
                                }
                            }
                            screen_dirty = true;
                            break;
                        }
                        /* Maximize / Restore Button */
                        if ((mouse_x - max_cx)*(mouse_x - max_cx) + (mouse_y - btn_cy)*(mouse_y - btn_cy) <= 49) {
                            if (!hit->is_maximized) {
                                hit->saved_x = hit->x;
                                hit->saved_y = hit->y;
                                hit->saved_w = hit->width;
                                hit->saved_h = hit->height;
                                hit->x = 0;
                                hit->y = BAR_H + TITLE_H;
                                hit->width = SCREEN_WIDTH;
                                hit->height = SCREEN_HEIGHT - BAR_H - TITLE_H;
                                hit->is_maximized = true;
                            } else {
                                hit->x = hit->saved_x;
                                hit->y = hit->saved_y;
                                hit->width = hit->saved_w;
                                hit->height = hit->saved_h;
                                hit->is_maximized = false;
                            }
                            screen_dirty = true;
                            break;
                        }
                    }

                    /* 2. Drag window by titlebar or border */
                    if ((l_down && (mouse_y < hit->y || mouse_x < hit->x || mouse_x >= hit->x + hit->width || mouse_y >= hit->y + hit->height)) ||
                        r_down) {
                        is_dragging = true;
                        drag_wid = hit->wid;
                        drag_offset_x = mouse_x - hit->x;
                        drag_offset_y = mouse_y - hit->y;
                    } else if (mouse_y >= hit->y && (l_down || l_up || dx || dy)) {
                        /* 3. Client area mouse event */
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
                w->is_maximized = false;
                w->saved_x = w->x;
                w->saved_y = w->y;
                w->saved_w = w->width;
                w->saved_h = w->height;
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
    printf("Starting DUnix Windowing System (DWS 2.0 - Modern Workstation)...\n");

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
