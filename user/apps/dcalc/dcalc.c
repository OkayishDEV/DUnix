#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dui/dui.h>
#include <dui/protocol.h>

#define BG_COLOR        0x00131722
#define DISP_BG         0x001A2130
#define DISP_BORDER     0x002B374E
#define DISP_FG         0x00F8FAFC
#define DISP_BADGE_BG   0x002563EB
#define DISP_BADGE_FG   0x00FFFFFF

#define BTN_NUM_BG      0x00232B3C
#define BTN_NUM_BORDER  0x00333E56
#define BTN_NUM_FG      0x00F1F5F9
#define BTN_NUM_HI      0x003A465E

#define BTN_FN_BG       0x00333D52
#define BTN_FN_BORDER   0x0047556E
#define BTN_FN_FG       0x00E2E8F0

#define BTN_OP_BG       0x002563EB
#define BTN_OP_BORDER   0x001D4ED8
#define BTN_OP_FG       0x00FFFFFF

#define BTN_EQ_BG       0x0010B981
#define BTN_EQ_BORDER   0x00059669
#define BTN_EQ_FG       0x00FFFFFF

#define BTN_CLR_BG      0x00EF4444
#define BTN_CLR_BORDER  0x00DC2626
#define BTN_CLR_FG      0x00FFFFFF

const char *buttons[5][4] = {
    {"C", "+/-", "%", "/"},
    {"7", "8", "9", "x"},
    {"4", "5", "6", "-"},
    {"1", "2", "3", "+"},
    {"0", ".", "=", ""}
};

static double operand = 0;
static double current = 0;
static char op = 0;
static int new_num = 1;
static char disp_str[64] = "0";

static void calculate(void) {
    if (op == '+') operand += current;
    else if (op == '-') operand -= current;
    else if (op == 'x') operand *= current;
    else if (op == '/') {
        if (current != 0) operand /= current;
    } else {
        operand = current;
    }
    current = operand;
    snprintf(disp_str, sizeof(disp_str), "%g", current);
    new_num = 1;
}

static void handle_btn(const char *btn) {
    if (strcmp(btn, "C") == 0) {
        operand = 0; current = 0; op = 0; new_num = 1; strcpy(disp_str, "0");
    } else if (strcmp(btn, "+/-") == 0) {
        current = -current; snprintf(disp_str, sizeof(disp_str), "%g", current);
    } else if (strcmp(btn, "%") == 0) {
        current = current / 100.0; snprintf(disp_str, sizeof(disp_str), "%g", current);
    } else if (strcmp(btn, "+") == 0 || strcmp(btn, "-") == 0 || strcmp(btn, "x") == 0 || strcmp(btn, "/") == 0) {
        if (!new_num) { calculate(); }
        op = btn[0]; operand = current; new_num = 1;
    } else if (strcmp(btn, "=") == 0) {
        calculate(); op = 0;
    } else if (btn[0] >= '0' && btn[0] <= '9') {
        if (new_num) { strcpy(disp_str, btn); new_num = 0; }
        else {
            if (strlen(disp_str) < 15) strcat(disp_str, btn);
        }
        current = atof(disp_str);
    } else if (strcmp(btn, ".") == 0) {
        if (new_num) { strcpy(disp_str, "0."); new_num = 0; }
        else if (strchr(disp_str, '.') == NULL) {
            strcat(disp_str, ".");
        }
        current = atof(disp_str);
    }
}

static void redraw(DuiConnection *conn, DuiWindow win) {
    dui_clear(conn, win, BG_COLOR);

    /* Display box */
    dui_fill_rect(conn, win, 10, 10, 240, 60, DISP_BG);
    dui_draw_rect(conn, win, 10, 10, 240, 60, DISP_BORDER);
    dui_draw_line(conn, win, 11, 11, 249, 11, 0x00333E56);

    /* Operator badge if active */
    if (op) {
        char op_buf[4];
        snprintf(op_buf, sizeof(op_buf), "%c", op);
        dui_fill_rect(conn, win, 16, 16, 22, 18, DISP_BADGE_BG);
        dui_draw_text(conn, win, 23, 17, op_buf, DISP_BADGE_FG);
    }

    int txt_w = (int)strlen(disp_str) * 8;
    dui_draw_text(conn, win, 10 + 240 - txt_w - 12, 32, disp_str, DISP_FG);

    /* Buttons */
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            if (r == 4 && c == 3) continue;
            const char *b = buttons[r][c];
            int x = 10 + c * 60;
            int y = 80 + r * 55;
            int w = (r == 4 && c == 0) ? 120 : 55;
            if (r == 4 && c > 0) x = 10 + (c + 1) * 60 - 55;

            uint32_t bg = BTN_NUM_BG;
            uint32_t border = BTN_NUM_BORDER;
            uint32_t fg = BTN_NUM_FG;
            uint32_t hi = BTN_NUM_HI;

            if (c == 3) {
                bg = BTN_OP_BG; border = BTN_OP_BORDER; fg = BTN_OP_FG; hi = 0x003B82F6;
            } else if (r == 0) {
                bg = BTN_FN_BG; border = BTN_FN_BORDER; fg = BTN_FN_FG; hi = 0x0047556E;
                if (strcmp(b, "C") == 0) {
                    bg = BTN_CLR_BG; border = BTN_CLR_BORDER; fg = BTN_CLR_FG; hi = 0x00F87171;
                }
            } else if (strcmp(b, "=") == 0) {
                bg = BTN_EQ_BG; border = BTN_EQ_BORDER; fg = BTN_EQ_FG; hi = 0x0034D399;
            }

            dui_fill_rect(conn, win, x, y, w, 50, bg);
            dui_draw_rect(conn, win, x, y, w, 50, border);
            dui_draw_line(conn, win, x + 1, y + 1, x + w - 2, y + 1, hi);

            int bw = (int)strlen(b) * 8;
            dui_draw_text(conn, win, x + (w - bw) / 2, y + 25 - 8, b, fg);
        }
    }
    dui_flush(conn, win);
}

static void handle_click(int mx, int my) {
    if (my < 80) return;
    int r = (my - 80) / 55;
    if (r >= 5) return;
    int c = 0;
    if (r == 4) {
        if (mx >= 10 && mx < 130) c = 0;
        else if (mx >= 130 && mx < 190) c = 1;
        else if (mx >= 190 && mx < 250) c = 2;
        else return;
    } else {
        c = (mx - 10) / 60;
        if (c >= 4) return;
    }
    handle_btn(buttons[r][c]);
}

int main(void) {
    DuiConnection *conn = dui_connect();
    if (!conn) return 1;

    DuiWindow win = dui_create_window(conn, 300, 100, 260, 380, "Calculator", BG_COLOR, DWS_WIN_DECORATED);
    dui_show(conn, win);
    redraw(conn, win);

    DuiEvent ev;
    while (1) {
        if (dui_next_event(conn, &ev) > 0) {
            if (ev.type == DWS_EV_CLOSE_REQ && ev.window == win) {
                break;
            } else if (ev.type == DWS_EV_EXPOSE && ev.window == win) {
                redraw(conn, win);
            } else if (ev.type == DWS_EV_MOUSE_DOWN && ev.window == win) {
                handle_click(ev.mouse.x, ev.mouse.y);
                redraw(conn, win);
            }
        }
    }

    dui_destroy_window(conn, win);
    dui_disconnect(conn);
    return 0;
}
