#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#define EYES_W   180
#define EYES_H   110
#define HEADER_H 20

#define EYE_RX   36
#define EYE_RY   34
#define PUPIL_R  10

static void draw_eyes(Display *dpy, Window win, GC gc_bg, GC gc_hdr, GC gc_white, GC gc_black, GC gc_text) {
    /* Clear Window */
    XFillRectangle(dpy, win, gc_bg, 0, 0, EYES_W, EYES_H);

    /* Header Tab */
    XFillRectangle(dpy, win, gc_hdr, 0, 0, EYES_W, HEADER_H);
    XSetForeground(dpy, gc_text, 0x00FFFFFF);
    const char *title = "xeyes";
    XDrawString(dpy, win, gc_text, 8, 2, title, (int)strlen(title));

    /* Query pointer position relative to root */
    Window root_ret, child_ret;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;
    XQueryPointer(dpy, win, &root_ret, &child_ret, &root_x, &root_y, &win_x, &win_y, &mask);

    int eye_cy = HEADER_H + (EYES_H - HEADER_H) / 2;
    int eye_lx = 48;
    int eye_rx = EYES_W - 48;

    /* Draw Left Eyeball White */
    XFillArc(dpy, win, gc_white, eye_lx - EYE_RX, eye_cy - EYE_RY, EYE_RX * 2, EYE_RY * 2, 0, 360 * 64);
    XDrawArc(dpy, win, gc_black, eye_lx - EYE_RX, eye_cy - EYE_RY, EYE_RX * 2, EYE_RY * 2, 0, 360 * 64);

    /* Draw Right Eyeball White */
    XFillArc(dpy, win, gc_white, eye_rx - EYE_RX, eye_cy - EYE_RY, EYE_RX * 2, EYE_RY * 2, 0, 360 * 64);
    XDrawArc(dpy, win, gc_black, eye_rx - EYE_RX, eye_cy - EYE_RY, EYE_RX * 2, EYE_RY * 2, 0, 360 * 64);

    /* Compute Pupil Positions towards cursor (win_x, win_y) */
    /* Left Pupil */
    double dx_l = win_x - eye_lx;
    double dy_l = win_y - eye_cy;
    double dist_l = sqrt(dx_l * dx_l + dy_l * dy_l);
    double max_dist_l = EYE_RX - PUPIL_R - 4;
    int pl_x = eye_lx;
    int pl_y = eye_cy;
    if (dist_l > 0) {
        double factor = (dist_l < max_dist_l) ? dist_l : max_dist_l;
        pl_x += (int)(dx_l / dist_l * factor);
        pl_y += (int)(dy_l / dist_l * factor);
    }
    XFillArc(dpy, win, gc_black, pl_x - PUPIL_R, pl_y - PUPIL_R, PUPIL_R * 2, PUPIL_R * 2, 0, 360 * 64);

    /* Right Pupil */
    double dx_r = win_x - eye_rx;
    double dy_r = win_y - eye_cy;
    double dist_r = sqrt(dx_r * dx_r + dy_r * dy_r);
    double max_dist_r = EYE_RX - PUPIL_R - 4;
    int pr_x = eye_rx;
    int pr_y = eye_cy;
    if (dist_r > 0) {
        double factor = (dist_r < max_dist_r) ? dist_r : max_dist_r;
        pr_x += (int)(dx_r / dist_r * factor);
        pr_y += (int)(dy_r / dist_r * factor);
    }
    XFillArc(dpy, win, gc_black, pr_x - PUPIL_R, pr_y - PUPIL_R, PUPIL_R * 2, PUPIL_R * 2, 0, 360 * 64);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "xeyes: cannot connect to X11 display server\n");
        return 1;
    }

    Window root = XDefaultRootWindow(dpy);
    Window win = XCreateSimpleWindow(dpy, root, 540, 60, EYES_W, EYES_H, 2, 0x004A6B82, 0x00D6DBDF);
    XStoreName(dpy, win, "xeyes");

    GC gc_bg = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_bg, 0x00D6DBDF);

    GC gc_hdr = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_hdr, 0x001B4F72);

    GC gc_white = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_white, 0x00FFFFFF);

    GC gc_black = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_black, 0x00000000);

    GC gc_text = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_text, 0x00FFFFFF);

    XSelectInput(dpy, win, ExposureMask | PointerMotionMask);
    XMapWindow(dpy, win);

    draw_eyes(dpy, win, gc_bg, gc_hdr, gc_white, gc_black, gc_text);

    XEvent ev;
    while (1) {
        if (XPending(dpy)) {
            XNextEvent(dpy, &ev);
            if (ev.type == Expose || ev.type == MotionNotify) {
                draw_eyes(dpy, win, gc_bg, gc_hdr, gc_white, gc_black, gc_text);
            }
        } else {
            draw_eyes(dpy, win, gc_bg, gc_hdr, gc_white, gc_black, gc_text);
            usleep(50000); /* 20 fps mouse tracking */
        }
    }

    XFreeGC(dpy, gc_bg);
    XFreeGC(dpy, gc_hdr);
    XFreeGC(dpy, gc_white);
    XFreeGC(dpy, gc_black);
    XFreeGC(dpy, gc_text);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
