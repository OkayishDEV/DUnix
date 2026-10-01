#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define SCREEN_W 1024
#define SCREEN_H 768
#define PANEL_H  32

#define MENU_WIDTH   220
#define MENU_HEIGHT  190
#define ITEM_HEIGHT  24

static const char *openbox_menu_items[] = {
    "=== LXDE / Openbox Menu ===",
    "  > PCManFM File Manager",
    "  > XTerm Terminal",
    "  > XClock Analog Clock",
    "  > XCalc Calculator",
    "  > XEyes Mouse Tracker",
    "  > Refresh Desktop",
    "  > Logout / Exit LXDE"
};

#define NUM_ITEMS (int)(sizeof(openbox_menu_items) / sizeof(openbox_menu_items[0]))

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

    printf("Starting Openbox Window Manager for LXDE...\n");

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "openbox: unable to connect to X11 display server\n");
        return 1;
    }

    Window root = XDefaultRootWindow(dpy);

    /* Create Openbox Desktop Menu Window */
    Window menu_win = XCreateSimpleWindow(dpy, root, 120, 120, MENU_WIDTH, MENU_HEIGHT, 2, 0x002C3E50, 0x00ECEFF1);
    XStoreName(dpy, menu_win, "Openbox Menu");

    GC gc = XCreateGC(dpy, menu_win, 0, NULL);

    GC gc_header = XCreateGC(dpy, menu_win, 0, NULL);
    XSetForeground(dpy, gc_header, 0x002C3E50); /* Dark Slate / Openbox header */

    GC gc_text = XCreateGC(dpy, menu_win, 0, NULL);
    XSetForeground(dpy, gc_text, 0x001A252F);

    GC gc_sel = XCreateGC(dpy, menu_win, 0, NULL);
    XSetForeground(dpy, gc_sel, 0x003498DB); /* LXDE Blue highlight */

    XSelectInput(dpy, root, ButtonPressMask | ButtonReleaseMask);
    XSelectInput(dpy, menu_win, ExposureMask | ButtonPressMask | PointerMotionMask);

    int selected_item = -1;

    printf("openbox: Openbox Window Manager active (LXDE Edition)\n");

    XEvent ev;
    while (1) {
        XNextEvent(dpy, &ev);

        if (ev.type == ButtonPress) {
            if (ev.xbutton.window == root) {
                if (ev.xbutton.button == Button3 || ev.xbutton.button == Button1) {
                    /* Right Click or Click on Desktop: show Openbox root menu */
                    int mx = ev.xbutton.x_root;
                    int my = ev.xbutton.y_root;
                    if (mx + MENU_WIDTH > SCREEN_W) mx = SCREEN_W - MENU_WIDTH;
                    if (my + MENU_HEIGHT > SCREEN_H - PANEL_H) my = SCREEN_H - PANEL_H - MENU_HEIGHT;

                    XMoveWindow(dpy, menu_win, mx, my);
                    XMapRaised(dpy, menu_win);
                }
            } else if (ev.xbutton.window == menu_win) {
                int clicked_item = ev.xbutton.y / ITEM_HEIGHT;
                if (clicked_item == 1) {
                    launch_app("/bin/pcmanfm");
                } else if (clicked_item == 2) {
                    launch_app("/bin/xterm");
                } else if (clicked_item == 3) {
                    launch_app("/bin/xclock");
                } else if (clicked_item == 4) {
                    launch_app("/bin/xcalc");
                } else if (clicked_item == 5) {
                    launch_app("/bin/xeyes");
                } else if (clicked_item == 6) {
                    XClearWindow(dpy, root);
                } else if (clicked_item == 7) {
                    /* Exit LXDE */
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
                XSetForeground(dpy, gc_text, 0x00FFFFFF);
                XDrawString(dpy, menu_win, gc_text, 12, 4, openbox_menu_items[0], (int)strlen(openbox_menu_items[0]));

                for (int i = 1; i < NUM_ITEMS; i++) {
                    int y = i * ITEM_HEIGHT;
                    if (i == selected_item) {
                        XFillRectangle(dpy, menu_win, gc_sel, 2, y, MENU_WIDTH - 4, ITEM_HEIGHT);
                        XSetForeground(dpy, gc_text, 0x00FFFFFF);
                    } else {
                        XSetForeground(dpy, gc_text, 0x001A252F);
                    }
                    XDrawString(dpy, menu_win, gc_text, 10, y + 4, openbox_menu_items[i], (int)strlen(openbox_menu_items[i]));
                }
            }
        } else if (ev.type == Expose && ev.xexpose.window == menu_win) {
            /* Draw Title Header */
            XFillRectangle(dpy, menu_win, gc_header, 0, 0, MENU_WIDTH, ITEM_HEIGHT);
            XSetForeground(dpy, gc_text, 0x00FFFFFF);
            XDrawString(dpy, menu_win, gc_text, 12, 4, openbox_menu_items[0], (int)strlen(openbox_menu_items[0]));

            /* Draw items */
            for (int i = 1; i < NUM_ITEMS; i++) {
                int y = i * ITEM_HEIGHT;
                XSetForeground(dpy, gc_text, 0x001A252F);
                XDrawString(dpy, menu_win, gc_text, 10, y + 4, openbox_menu_items[i], (int)strlen(openbox_menu_items[i]));
            }
        }

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
