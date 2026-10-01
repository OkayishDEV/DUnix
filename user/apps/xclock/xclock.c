#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#define CLOCK_SIZE 180
#define HEADER_H   20

#define PI 3.14159265358979323846

static void draw_clock(Display *dpy, Window win, GC gc_bg, GC gc_fg, GC gc_hdr, GC gc_hands, GC gc_sec) {
    /* Clear window */
    XFillRectangle(dpy, win, gc_bg, 0, 0, CLOCK_SIZE, CLOCK_SIZE + HEADER_H);

    /* Draw Header */
    XFillRectangle(dpy, win, gc_hdr, 0, 0, CLOCK_SIZE, HEADER_H);
    XSetForeground(dpy, gc_fg, 0x00FFFFFF);
    const char *title = "xclock";
    XDrawString(dpy, win, gc_fg, 8, 2, title, (int)strlen(title));

    int cx = CLOCK_SIZE / 2;
    int cy = HEADER_H + CLOCK_SIZE / 2;
    int r = CLOCK_SIZE / 2 - 12;

    /* Draw Outer Bezel / Rim */
    XSetForeground(dpy, gc_fg, 0x001B4F72);
    XDrawArc(dpy, win, gc_fg, cx - r, cy - r, r * 2, r * 2, 0, 360 * 64);
    XDrawArc(dpy, win, gc_fg, cx - r + 1, cy - r + 1, (r - 1) * 2, (r - 1) * 2, 0, 360 * 64);

    /* Draw 12 Hour Marks */
    XSetForeground(dpy, gc_fg, 0x002C3E50);
    for (int h = 0; h < 12; h++) {
        double angle = h * (PI / 6.0);
        int x1 = cx + (int)((r - 8) * sin(angle));
        int y1 = cy - (int)((r - 8) * cos(angle));
        int x2 = cx + (int)(r * sin(angle));
        int y2 = cy - (int)(r * cos(angle));
        XDrawLine(dpy, win, gc_fg, x1, y1, x2, y2);
    }

    /* Get current time simulation or monotonic ticks */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    int sec = t ? t->tm_sec : 30;
    int min = t ? t->tm_min : 15;
    int hr  = t ? (t->tm_hour % 12) : 10;

    /* Draw Hour Hand */
    double hr_angle = (hr + min / 60.0) * (PI / 6.0);
    int hx = cx + (int)((r * 0.5) * sin(hr_angle));
    int hy = cy - (int)((r * 0.5) * cos(hr_angle));
    XDrawLine(dpy, win, gc_hands, cx, cy, hx, hy);

    /* Draw Minute Hand */
    double min_angle = (min + sec / 60.0) * (PI / 30.0);
    int mx = cx + (int)((r * 0.75) * sin(min_angle));
    int my = cy - (int)((r * 0.75) * cos(min_angle));
    XDrawLine(dpy, win, gc_hands, cx, cy, mx, my);

    /* Draw Second Hand */
    double sec_angle = sec * (PI / 30.0);
    int sx = cx + (int)((r * 0.85) * sin(sec_angle));
    int sy = cy - (int)((r * 0.85) * cos(sec_angle));
    XDrawLine(dpy, win, gc_sec, cx, cy, sx, sy);

    /* Center Pivot Point */
    XFillArc(dpy, win, gc_hands, cx - 3, cy - 3, 6, 6, 0, 360 * 64);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "xclock: cannot connect to X11 display server\n");
        return 1;
    }

    Window root = XDefaultRootWindow(dpy);
    Window win = XCreateSimpleWindow(dpy, root, 760, 60, CLOCK_SIZE, CLOCK_SIZE + HEADER_H, 2, 0x004A6B82, 0x00ECEFF1);
    XStoreName(dpy, win, "xclock");

    GC gc_bg = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_bg, 0x00ECEFF1);

    GC gc_fg = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_fg, 0x001B4F72);

    GC gc_hdr = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_hdr, 0x001B4F72);

    GC gc_hands = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_hands, 0x00111111);

    GC gc_sec = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_sec, 0x00E74C3C); /* Red second hand */

    XSelectInput(dpy, win, ExposureMask | ButtonPressMask);
    XMapWindow(dpy, win);

    draw_clock(dpy, win, gc_bg, gc_fg, gc_hdr, gc_hands, gc_sec);

    XEvent ev;
    while (1) {
        if (XPending(dpy)) {
            XNextEvent(dpy, &ev);
            if (ev.type == Expose) {
                draw_clock(dpy, win, gc_bg, gc_fg, gc_hdr, gc_hands, gc_sec);
            }
        } else {
            draw_clock(dpy, win, gc_bg, gc_fg, gc_hdr, gc_hands, gc_sec);
            sleep(1);
        }
    }

    XFreeGC(dpy, gc_bg);
    XFreeGC(dpy, gc_fg);
    XFreeGC(dpy, gc_hdr);
    XFreeGC(dpy, gc_hands);
    XFreeGC(dpy, gc_sec);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
