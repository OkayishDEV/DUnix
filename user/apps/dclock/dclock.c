#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <dui/dui.h>
#include <dui/protocol.h>

#define BG_COLOR        0x00131722
#define BEZEL_COLOR     0x001C2334
#define BEZEL_BORDER    0x002B374E
#define FACE_COLOR      0x000F131D
#define RING1_COLOR     0x001B2436
#define RING2_COLOR     0x00243048
#define MARKER_COLOR    0x0064748B
#define MARKER_HI_COLOR 0x0094A3B8
#define HAND_H_COLOR    0x00CBD5E1
#define HAND_M_COLOR    0x00F8FAFC
#define HAND_S_COLOR    0x0006B6D4
#define HUB_COLOR       0x00F8FAFC

static void draw_hand(DuiConnection *conn, DuiWindow win, int cx, int cy, float angle, int length, int width, uint32_t color) {
    int ex = cx + (int)(sin(angle) * length);
    int ey = cy - (int)(cos(angle) * length);
    dui_draw_line(conn, win, cx, cy, ex, ey, color);

    if (width >= 2) {
        dui_draw_line(conn, win, cx + 1, cy, ex + 1, ey, color);
        dui_draw_line(conn, win, cx, cy + 1, ex, ey + 1, color);
    }
    if (width >= 3) {
        dui_draw_line(conn, win, cx - 1, cy, ex - 1, ey, color);
        dui_draw_line(conn, win, cx, cy - 1, ex, ey - 1, color);
    }
}

static void redraw(DuiConnection *conn, DuiWindow win) {
    dui_clear(conn, win, BG_COLOR);

    int cx = 120;
    int cy = 110;
    int r = 95;

    /* Outer bezel & dial face */
    dui_fill_circle(conn, win, cx, cy, r, BEZEL_COLOR);
    dui_draw_circle(conn, win, cx, cy, r, BEZEL_BORDER);
    dui_fill_circle(conn, win, cx, cy, r - 4, FACE_COLOR);
    dui_draw_circle(conn, win, cx, cy, r - 4, RING2_COLOR);

    /* Concentric decorative dial rings */
    dui_draw_circle(conn, win, cx, cy, r - 26, RING1_COLOR);
    dui_draw_circle(conn, win, cx, cy, 32, RING1_COLOR);

    /* 12 Hour tick marks */
    for (int i = 0; i < 12; i++) {
        float angle = (float)(i * M_PI / 6.0);
        int tick_len = (i % 3 == 0) ? 14 : 8;
        int sx = cx + (int)(sin(angle) * (r - 6 - tick_len));
        int sy = cy - (int)(cos(angle) * (r - 6 - tick_len));
        int ex = cx + (int)(sin(angle) * (r - 6));
        int ey = cy - (int)(cos(angle) * (r - 6));
        uint32_t col = (i % 3 == 0) ? MARKER_HI_COLOR : MARKER_COLOR;
        dui_draw_line(conn, win, sx, sy, ex, ey, col);
        if (i % 3 == 0) {
            dui_draw_line(conn, win, sx + 1, sy, ex + 1, ey, col);
        }
    }

    /* Numerals for 12, 3, 6, 9 */
    dui_draw_text(conn, win, cx - 8, cy - 72, "12", 0x00CBD5E1);
    dui_draw_text(conn, win, cx + 64, cy - 8, "3", 0x00CBD5E1);
    dui_draw_text(conn, win, cx - 4, cy + 56, "6", 0x00CBD5E1);
    dui_draw_text(conn, win, cx - 72, cy - 8, "9", 0x00CBD5E1);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    float h_angle = (float)((t->tm_hour % 12 + t->tm_min / 60.0) * M_PI / 6.0);
    float m_angle = (float)((t->tm_min + t->tm_sec / 60.0) * M_PI / 30.0);
    float s_angle = (float)(t->tm_sec * M_PI / 30.0);

    /* Watch hands */
    draw_hand(conn, win, cx, cy, h_angle, r - 46, 3, HAND_H_COLOR);
    draw_hand(conn, win, cx, cy, m_angle, r - 26, 2, HAND_M_COLOR);

    /* Second hand with tail counterweight */
    int tail_x = cx - (int)(sin(s_angle) * 16);
    int tail_y = cy + (int)(cos(s_angle) * 16);
    int tip_x = cx + (int)(sin(s_angle) * (r - 12));
    int tip_y = cy - (int)(cos(s_angle) * (r - 12));
    dui_draw_line(conn, win, tail_x, tail_y, tip_x, tip_y, HAND_S_COLOR);

    /* Center hub */
    dui_fill_circle(conn, win, cx, cy, 4, HUB_COLOR);
    dui_fill_circle(conn, win, cx, cy, 2, HAND_S_COLOR);

    /* Digital time pill badge */
    char dig_str[32];
    snprintf(dig_str, sizeof(dig_str), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    int dw = (int)strlen(dig_str) * 8;
    dui_fill_rect(conn, win, cx - 46, 222, 92, 22, 0x001B2232);
    dui_draw_rect(conn, win, cx - 46, 222, 92, 22, 0x002B374E);
    dui_draw_text(conn, win, cx - dw / 2, 226, dig_str, 0x0038BDF8);

    dui_flush(conn, win);
}

int main(void) {
    DuiConnection *conn = dui_connect();
    if (!conn) return 1;

    DuiWindow win = dui_create_window(conn, 740, 120, 240, 260, "System Clock", BG_COLOR, DWS_WIN_DECORATED);
    dui_show(conn, win);

    DuiEvent ev;
    while (1) {
        if (dui_poll_event(conn, &ev) > 0) {
            if (ev.type == DWS_EV_CLOSE_REQ && ev.window == win) {
                break;
            } else if (ev.type == DWS_EV_EXPOSE && ev.window == win) {
                redraw(conn, win);
            }
        }
        redraw(conn, win);
        usleep(500000);
    }

    dui_destroy_window(conn, win);
    dui_disconnect(conn);
    return 0;
}
