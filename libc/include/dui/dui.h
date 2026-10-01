/* DUI - DUnix UI Client Library
 * Simple, modern API for creating windows and drawing on the DUnix desktop.
 * No GCs, no server-side resource management. Just connect, create, draw, done.
 *
 * Copyright (c) 2026 DUnix Project. All rights reserved.
 */

#ifndef _DUI_DUI_H
#define _DUI_DUI_H

#include <stdint.h>
#include <stdbool.h>
#include <dui/protocol.h>

/* Connection handle */
typedef struct dui_connection {
    int      fd;
    uint32_t client_id;
    uint32_t screen_width;
    uint32_t screen_height;
    uint32_t next_wid;
} DuiConnection;

/* Window handle — just an integer ID */
typedef uint32_t DuiWindow;

/* Event structure (simplified from wire format) */
typedef struct {
    uint32_t type;     /* DWS_EV_* constant */
    uint32_t window;   /* Source window ID */
    union {
        struct { int x, y; uint32_t button; int root_x, root_y; } mouse;
        struct { uint32_t keycode; char ch; }                     key;
        struct { uint32_t width, height; }                        resize;
    };
} DuiEvent;

/* ─── Connection ─── */
DuiConnection *dui_connect(void);
void           dui_disconnect(DuiConnection *conn);
uint32_t       dui_screen_width(DuiConnection *conn);
uint32_t       dui_screen_height(DuiConnection *conn);

/* ─── Window Management ─── */
DuiWindow dui_create_window(DuiConnection *conn, int x, int y, int w, int h,
                            const char *title, uint32_t bg_color, uint32_t flags);
void dui_destroy_window(DuiConnection *conn, DuiWindow win);
void dui_show(DuiConnection *conn, DuiWindow win);
void dui_hide(DuiConnection *conn, DuiWindow win);
void dui_move(DuiConnection *conn, DuiWindow win, int x, int y);
void dui_resize(DuiConnection *conn, DuiWindow win, int w, int h);
void dui_set_title(DuiConnection *conn, DuiWindow win, const char *title);
void dui_raise(DuiConnection *conn, DuiWindow win);

/* ─── Drawing (all coordinates relative to window client area) ─── */
void dui_fill_rect(DuiConnection *conn, DuiWindow win, int x, int y, int w, int h, uint32_t color);
void dui_draw_rect(DuiConnection *conn, DuiWindow win, int x, int y, int w, int h, uint32_t color);
void dui_draw_line(DuiConnection *conn, DuiWindow win, int x1, int y1, int x2, int y2, uint32_t color);
void dui_draw_text(DuiConnection *conn, DuiWindow win, int x, int y, const char *text, uint32_t color);
void dui_set_pixel(DuiConnection *conn, DuiWindow win, int x, int y, uint32_t color);
void dui_clear(DuiConnection *conn, DuiWindow win, uint32_t color);
void dui_flush(DuiConnection *conn, DuiWindow win);
void dui_fill_circle(DuiConnection *conn, DuiWindow win, int cx, int cy, int r, uint32_t color);
void dui_draw_circle(DuiConnection *conn, DuiWindow win, int cx, int cy, int r, uint32_t color);
void dui_blit_buffer(DuiConnection *conn, DuiWindow win, const uint32_t *pixels, int w, int h);

/* ─── Events ─── */
int  dui_next_event(DuiConnection *conn, DuiEvent *ev);  /* Blocking */
int  dui_poll_event(DuiConnection *conn, DuiEvent *ev);  /* Non-blocking: 1=event, 0=none */
int  dui_has_event(DuiConnection *conn);                 /* Non-blocking peek */

#endif /* _DUI_DUI_H */
