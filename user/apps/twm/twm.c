#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MENU_WIDTH   220
#define MENU_HEIGHT  170
#define ITEM_HEIGHT  24

static const char *menu_items[] = {
    "=== DUnix Applications ===",
    "  > XTerm Terminal",
    "  > XClock Analog Clock",
    "  > XCalc Calculator",
    "  > XEyes Mouse Tracker",
    "  > Redraw Screen",
    "  > Exit X11 Desktop"
};

#define NUM_ITEMS (int)(sizeof(menu_items) / sizeof(menu_items[0]))

static void launch_app(const char *cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = { (char *)cmd, NULL };
        execve(cmd, argv, NULL);
        exit(1);
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("Starting twm (Tab Window Manager)...\n");

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "twm: unable to connect to X11 display server\n");
        return 1;
    }

    Window root = XDefaultRootWindow(dpy);
    int screen_w = XDisplayWidth(dpy, 0);
    int screen_h = XDisplayHeight(dpy, 0);
    (void)screen_w; (void)screen_h;

    /* Create TWM Root Menu Window */
    Window menu_win = XCreateSimpleWindow(dpy, root, 100, 100, MENU_WIDTH, MENU_HEIGHT, 2, 0x00112233, 0x00E0E8F0);
    XStoreName(dpy, menu_win, "TWM Menu");

    GC gc = XCreateGC(dpy, menu_win, 0, NULL);
    GC gc_header = XCreateGC(dpy, menu_win, 0, NULL);
    XSetForeground(dpy, gc_header, 0x001B4F72);
    XSetBackground(dpy, gc_header, 0x00D4E6F1);

    GC gc_text = XCreateGC(dpy, menu_win, 0, NULL);
    XSetForeground(dpy, gc_text, 0x00000000);

    GC gc_sel = XCreateGC(dpy, menu_win, 0, NULL);
    XSetForeground(dpy, gc_sel, 0x002980B9);

    XSelectInput(dpy, root, ButtonPressMask | ButtonReleaseMask);
    XSelectInput(dpy, menu_win, ExposureMask | ButtonPressMask | PointerMotionMask);

    int selected_item = -1;

    printf("twm: Tab Window Manager active\n");

    XEvent ev;
    while (1) {
        XNextEvent(dpy, &ev);

        if (ev.type == ButtonPress) {
            if (ev.xbutton.window == root) {
                /* Clicked on Desktop: toggle/open menu at mouse position */
                int mx = ev.xbutton.x_root;
                int my = ev.xbutton.y_root;
                if (mx + MENU_WIDTH > 1024) mx = 1024 - MENU_WIDTH;
                if (my + MENU_HEIGHT > 768) my = 768 - MENU_HEIGHT;

                XMoveWindow(dpy, menu_win, mx, my);
                XMapRaised(dpy, menu_win);
            } else if (ev.xbutton.window == menu_win) {
                int clicked_item = ev.xbutton.y / ITEM_HEIGHT;
                if (clicked_item == 1) {
                    launch_app("/bin/xterm");
                } else if (clicked_item == 2) {
                    launch_app("/bin/xclock");
                } else if (clicked_item == 3) {
                    launch_app("/bin/xcalc");
                } else if (clicked_item == 4) {
                    launch_app("/bin/xeyes");
                } else if (clicked_item == 5) {
                    XClearWindow(dpy, root);
                } else if (clicked_item == 6) {
                    /* Exit Desktop */
                    break;
                }
                XUnmapWindow(dpy, menu_win);
            }
        } else if (ev.type == MotionNotify && ev.xmotion.window == menu_win) {
            int item = ev.xmotion.y / ITEM_HEIGHT;
            if (item != selected_item && item > 0 && item < NUM_ITEMS) {
                selected_item = item;
                /* Redraw menu items */
                XClearWindow(dpy, menu_win);
                XFillRectangle(dpy, menu_win, gc_header, 0, 0, MENU_WIDTH, ITEM_HEIGHT);
                XDrawString(dpy, menu_win, gc_header, 12, 4, menu_items[0], (int)strlen(menu_items[0]));

                for (int i = 1; i < NUM_ITEMS; i++) {
                    int y = i * ITEM_HEIGHT;
                    if (i == selected_item) {
                        XFillRectangle(dpy, menu_win, gc_sel, 2, y, MENU_WIDTH - 4, ITEM_HEIGHT);
                        XSetForeground(dpy, gc_text, 0x00FFFFFF);
                    } else {
                        XSetForeground(dpy, gc_text, 0x00000000);
                    }
                    XDrawString(dpy, menu_win, gc_text, 10, y + 4, menu_items[i], (int)strlen(menu_items[i]));
                }
            }
        } else if (ev.type == Expose && ev.xexpose.window == menu_win) {
            /* Draw Title Header */
            XFillRectangle(dpy, menu_win, gc_header, 0, 0, MENU_WIDTH, ITEM_HEIGHT);
            XSetForeground(dpy, gc_text, 0x00FFFFFF);
            XDrawString(dpy, menu_win, gc_text, 12, 4, menu_items[0], (int)strlen(menu_items[0]));

            /* Draw items */
            for (int i = 1; i < NUM_ITEMS; i++) {
                int y = i * ITEM_HEIGHT;
                XSetForeground(dpy, gc_text, 0x00000000);
                XDrawString(dpy, menu_win, gc_text, 10, y + 4, menu_items[i], (int)strlen(menu_items[i]));
            }
        }

        /* Check child process exits */
        int status;
        waitpid(-1, &status, WNOHANG);
    }

    XFreeGC(dpy, gc);
    XFreeGC(dpy, gc_header);
    XFreeGC(dpy, gc_text);
    XFreeGC(dpy, gc_sel);
    XDestroyWindow(dpy, menu_win);
    XCloseDisplay(dpy);
    return 0;
}
