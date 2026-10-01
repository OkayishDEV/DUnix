#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#define CALC_W 200
#define CALC_H 260
#define HEADER_H 20
#define DISPLAY_H 36
#define BTN_W 42
#define BTN_H 34

static const char *buttons[5][4] = {
    {"C", "SQ", "%", "/"},
    {"7", "8", "9", "*"},
    {"4", "5", "6", "-"},
    {"1", "2", "3", "+"},
    {"0", ".", "+/-", "="}
};

static char display_str[32] = "0";
static double current_val = 0.0;
static double accumulator = 0.0;
static char current_op = 0;
static bool clear_on_next = true;

static void render_calc(Display *dpy, Window win, GC gc_bg, GC gc_hdr, GC gc_lcd, GC gc_btn, GC gc_op, GC gc_text) {
    /* Window Background */
    XFillRectangle(dpy, win, gc_bg, 0, 0, CALC_W, CALC_H);

    /* Header Tab */
    XFillRectangle(dpy, win, gc_hdr, 0, 0, CALC_W, HEADER_H);
    XSetForeground(dpy, gc_text, 0x00FFFFFF);
    const char *title = "xcalc";
    XDrawString(dpy, win, gc_text, 8, 2, title, (int)strlen(title));

    /* LCD Screen Display */
    XFillRectangle(dpy, win, gc_lcd, 10, HEADER_H + 8, CALC_W - 20, DISPLAY_H);
    XSetForeground(dpy, gc_text, 0x0000FF66); /* Classic Green Phosphor Readout */

    int str_w = (int)strlen(display_str) * 8;
    int tx = CALC_W - 24 - str_w;
    if (tx < 14) tx = 14;
    XDrawString(dpy, win, gc_text, tx, HEADER_H + 18, display_str, (int)strlen(display_str));

    /* Draw Button Grid */
    int start_y = HEADER_H + 8 + DISPLAY_H + 8;
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = 10 + c * (BTN_W + 5);
            int by = start_y + r * (BTN_H + 4);

            GC bg_to_use = (c == 3 || (r == 0 && c > 0) || (r == 4 && c == 3)) ? gc_op : gc_btn;
            XFillRectangle(dpy, win, bg_to_use, bx, by, BTN_W, BTN_H);

            /* Bevel border */
            XSetForeground(dpy, gc_text, 0x00333333);
            XDrawRectangle(dpy, win, gc_text, bx, by, BTN_W, BTN_H);

            /* Label */
            const char *label = buttons[r][c];
            int lx = bx + (BTN_W - (int)strlen(label) * 8) / 2;
            int ly = by + 9;
            XSetForeground(dpy, gc_text, 0x00FFFFFF);
            XDrawString(dpy, win, gc_text, lx, ly, label, (int)strlen(label));
        }
    }
}

static void button_clicked(int r, int c) {
    const char *btn = buttons[r][c];

    if (strcmp(btn, "C") == 0) {
        strcpy(display_str, "0");
        current_val = 0.0;
        accumulator = 0.0;
        current_op = 0;
        clear_on_next = true;
    } else if (strcmp(btn, "SQ") == 0) {
        double v = atof(display_str);
        if (v >= 0) {
            snprintf(display_str, sizeof(display_str), "%g", sqrt(v));
        } else {
            strcpy(display_str, "Error");
        }
        clear_on_next = true;
    } else if (strcmp(btn, "+/-") == 0) {
        if (display_str[0] == '-') {
            memmove(display_str, display_str + 1, strlen(display_str));
        } else if (strcmp(display_str, "0") != 0) {
            memmove(display_str + 1, display_str, strlen(display_str) + 1);
            display_str[0] = '-';
        }
    } else if (btn[0] >= '0' && btn[0] <= '9') {
        if (clear_on_next || strcmp(display_str, "0") == 0) {
            display_str[0] = btn[0];
            display_str[1] = '\0';
            clear_on_next = false;
        } else {
            if (strlen(display_str) < 14) {
                strcat(display_str, btn);
            }
        }
    } else if (strcmp(btn, ".") == 0) {
        if (clear_on_next) {
            strcpy(display_str, "0.");
            clear_on_next = false;
        } else if (!strchr(display_str, '.')) {
            strcat(display_str, ".");
        }
    } else if (strcmp(btn, "=") == 0 || strcmp(btn, "+") == 0 ||
               strcmp(btn, "-") == 0 || strcmp(btn, "*") == 0 ||
               strcmp(btn, "/") == 0) {
        double val = atof(display_str);
        if (current_op == '+') accumulator += val;
        else if (current_op == '-') accumulator -= val;
        else if (current_op == '*') accumulator *= val;
        else if (current_op == '/') {
            if (val != 0.0) accumulator /= val;
            else { strcpy(display_str, "Div/0"); clear_on_next = true; return; }
        } else {
            accumulator = val;
        }

        snprintf(display_str, sizeof(display_str), "%g", accumulator);
        current_op = (strcmp(btn, "=") == 0) ? 0 : btn[0];
        clear_on_next = true;
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "xcalc: cannot connect to X11 display server\n");
        return 1;
    }

    Window root = XDefaultRootWindow(dpy);
    Window win = XCreateSimpleWindow(dpy, root, 480, 240, CALC_W, CALC_H, 2, 0x004A6B82, 0x002C3E50);
    XStoreName(dpy, win, "xcalc");

    GC gc_bg = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_bg, 0x002C3E50);

    GC gc_hdr = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_hdr, 0x001B4F72);

    GC gc_lcd = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_lcd, 0x000F1B24);

    GC gc_btn = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_btn, 0x0034495E);

    GC gc_op = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_op, 0x00D35400); /* Orange operator button */

    GC gc_text = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_text, 0x00FFFFFF);

    XSelectInput(dpy, win, ExposureMask | ButtonPressMask);
    XMapWindow(dpy, win);

    render_calc(dpy, win, gc_bg, gc_hdr, gc_lcd, gc_btn, gc_op, gc_text);

    XEvent ev;
    while (1) {
        XNextEvent(dpy, &ev);

        if (ev.type == Expose) {
            render_calc(dpy, win, gc_bg, gc_hdr, gc_lcd, gc_btn, gc_op, gc_text);
        } else if (ev.type == ButtonPress) {
            int mx = ev.xbutton.x;
            int my = ev.xbutton.y;

            int start_y = HEADER_H + 8 + DISPLAY_H + 8;
            for (int r = 0; r < 5; r++) {
                for (int c = 0; c < 4; c++) {
                    int bx = 10 + c * (BTN_W + 5);
                    int by = start_y + r * (BTN_H + 4);

                    if (mx >= bx && mx < bx + BTN_W &&
                        my >= by && my < by + BTN_H) {
                        button_clicked(r, c);
                        render_calc(dpy, win, gc_bg, gc_hdr, gc_lcd, gc_btn, gc_op, gc_text);
                        break;
                    }
                }
            }
        }
    }

    XFreeGC(dpy, gc_bg);
    XFreeGC(dpy, gc_hdr);
    XFreeGC(dpy, gc_lcd);
    XFreeGC(dpy, gc_btn);
    XFreeGC(dpy, gc_op);
    XFreeGC(dpy, gc_text);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
