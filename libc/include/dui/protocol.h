/* DWS Protocol - DUnix Windowing System
 * Native display protocol for DUnix OS.
 * All messages are fixed-size (DWS_MSG_SIZE bytes) for simplicity.
 * No GCs, no server-side resource management — just windows and drawing.
 *
 * Copyright (c) 2026 DUnix Project. All rights reserved.
 */

#ifndef _DUI_PROTOCOL_H
#define _DUI_PROTOCOL_H

#include <stdint.h>

#define DWS_PORT     7000
#define DWS_MSG_SIZE 256

/* ─── Request opcodes (client → server) ─── */
#define DWS_REQ_CONNECT         0x0001
#define DWS_REQ_CREATE_WINDOW   0x0002
#define DWS_REQ_DESTROY_WINDOW  0x0003
#define DWS_REQ_SHOW            0x0004
#define DWS_REQ_HIDE            0x0005
#define DWS_REQ_MOVE            0x0006
#define DWS_REQ_RESIZE          0x0007
#define DWS_REQ_SET_TITLE       0x0008
#define DWS_REQ_RAISE           0x0009
#define DWS_REQ_FILL_RECT       0x0010
#define DWS_REQ_DRAW_RECT       0x0011
#define DWS_REQ_DRAW_LINE       0x0012
#define DWS_REQ_DRAW_TEXT       0x0013
#define DWS_REQ_SET_PIXEL       0x0014
#define DWS_REQ_CLEAR           0x0015
#define DWS_REQ_FLUSH           0x0016
#define DWS_REQ_FILL_CIRCLE     0x0017
#define DWS_REQ_DRAW_CIRCLE     0x0018
#define DWS_REQ_BLIT_BUFFER     0x0019

/* ─── Event types (server → client) ─── */
#define DWS_EV_CONNECTED        0x8001
#define DWS_EV_EXPOSE           0x8002
#define DWS_EV_MOUSE_DOWN       0x8003
#define DWS_EV_MOUSE_UP         0x8004
#define DWS_EV_MOUSE_MOVE       0x8005
#define DWS_EV_KEY_DOWN         0x8006
#define DWS_EV_KEY_UP           0x8007
#define DWS_EV_CLOSE_REQ        0x8008
#define DWS_EV_RESIZED          0x8009
#define DWS_EV_FOCUS_IN         0x800A
#define DWS_EV_FOCUS_OUT        0x800B

/* ─── Window creation flags ─── */
#define DWS_WIN_DECORATED       0x0001  /* Server draws titlebar + border */
#define DWS_WIN_RESIZABLE       0x0002  /* Allow user resize */
#define DWS_WIN_PANEL           0x0004  /* Always-on-top, undecorated panel */
#define DWS_WIN_POPUP           0x0008  /* Undecorated popup, above normals */

/* ─── Standard colors ─── */
#define DWS_COLOR_BLACK         0x00000000
#define DWS_COLOR_WHITE         0x00FFFFFF

/* ─── Universal message structure (exactly DWS_MSG_SIZE bytes) ─── */
struct dws_message {
    uint32_t type;        /* Request opcode or event type */
    uint32_t window_id;   /* Target (req) / source (event) window, 0 = global */
    union {
        /* DWS_REQ_CREATE_WINDOW */
        struct {
            int32_t  x, y;
            uint32_t width, height;
            uint32_t bg_color;
            uint32_t flags;
            char     title[64];
        } create;

        /* DWS_REQ_FILL_RECT, DWS_REQ_DRAW_RECT */
        struct {
            int32_t  x, y;
            uint32_t width, height;
            uint32_t color;
        } rect;

        /* DWS_REQ_DRAW_LINE */
        struct {
            int32_t  x1, y1, x2, y2;
            uint32_t color;
        } line;

        /* DWS_REQ_DRAW_TEXT */
        struct {
            int32_t  x, y;
            uint32_t color;
            uint32_t len;
            char     text[128];
        } text;

        /* DWS_REQ_SET_PIXEL */
        struct {
            int32_t  x, y;
            uint32_t color;
        } pixel;

        /* DWS_REQ_CLEAR */
        struct {
            uint32_t color;
        } clear;

        /* DWS_REQ_MOVE */
        struct {
            int32_t x, y;
        } move;

        /* DWS_REQ_RESIZE */
        struct {
            uint32_t width, height;
        } resize;

        /* DWS_REQ_SET_TITLE */
        struct {
            char title[64];
        } set_title;

        /* DWS_REQ_FILL_CIRCLE, DWS_REQ_DRAW_CIRCLE */
        struct {
            int32_t  cx, cy;
            uint32_t radius;
            uint32_t color;
        } circle;

        /* DWS_EV_CONNECTED */
        struct {
            uint32_t screen_width;
            uint32_t screen_height;
            uint32_t client_id;
        } connected;

        /* DWS_EV_EXPOSE, DWS_EV_RESIZED */
        struct {
            uint32_t ev_width, ev_height;
        } expose;

        /* DWS_EV_MOUSE_DOWN / DWS_EV_MOUSE_UP / DWS_EV_MOUSE_MOVE */
        struct {
            int32_t  mx, my;           /* Relative to window client area */
            uint32_t button;           /* 1=left, 2=middle, 3=right */
            int32_t  root_x, root_y;   /* Absolute screen coordinates */
        } mouse;

        /* DWS_EV_KEY_DOWN / DWS_EV_KEY_UP */
        struct {
            uint32_t keycode;
            char     ch;
        } key;

        /* Force union to exactly (DWS_MSG_SIZE - 8) bytes */
        uint8_t _pad[DWS_MSG_SIZE - 8];
    };
};

#endif /* _DUI_PROTOCOL_H */
