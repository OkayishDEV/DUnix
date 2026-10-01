#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <dui/dui.h>
#include <dui/protocol.h>

#define BG_COLOR 0x00FFFFFF
#define FACE_COLOR 0x00ECF0F1
#define BORDER_COLOR 0x002C3E50
#define MARKER_COLOR 0x0034495E
#define HAND_H_COLOR 0x002C3E50
#define HAND_M_COLOR 0x002C3E50
#define HAND_S_COLOR 0x00E74C3C

void draw_hand(DuiConnection *conn, DuiWindow win, int cx, int cy, float angle, int length, uint32_t color) {
    int ex = cx + (int)(sin(angle) * length);
    int ey = cy - (int)(cos(angle) * length);
    dui_draw_line(conn, win, cx, cy, ex, ey, color);
}

void redraw(DuiConnection *conn, DuiWindow win) {
    dui_clear(conn, win, BG_COLOR);
    
    int cx = 100;
    int cy = 100;
    int r = 90;
    
    dui_fill_circle(conn, win, cx, cy, r, FACE_COLOR);
    dui_draw_circle(conn, win, cx, cy, r, BORDER_COLOR);
    
    for (int i = 0; i < 12; i++) {
        float angle = i * M_PI / 6.0;
        int sx = cx + (int)(sin(angle) * (r - 10));
        int sy = cy - (int)(cos(angle) * (r - 10));
        int ex = cx + (int)(sin(angle) * r);
        int ey = cy - (int)(cos(angle) * r);
        dui_draw_line(conn, win, sx, sy, ex, ey, MARKER_COLOR);
    }
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    float h_angle = (t->tm_hour % 12 + t->tm_min / 60.0) * M_PI / 6.0;
    float m_angle = (t->tm_min + t->tm_sec / 60.0) * M_PI / 30.0;
    float s_angle = t->tm_sec * M_PI / 30.0;
    
    draw_hand(conn, win, cx, cy, h_angle, r - 40, HAND_H_COLOR);
    draw_hand(conn, win, cx, cy, m_angle, r - 20, HAND_M_COLOR);
    draw_hand(conn, win, cx, cy, s_angle, r - 10, HAND_S_COLOR);
    
    dui_flush(conn, win);
}

int main(void) {
    DuiConnection *conn = dui_connect();
    if (!conn) return 1;

    DuiWindow win = dui_create_window(conn, 200, 200, 200, 200, "DUnix Clock", BG_COLOR, DWS_WIN_DECORATED);
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
        usleep(1000000);
    }

    dui_destroy_window(conn, win);
    dui_disconnect(conn);
    return 0;
}
