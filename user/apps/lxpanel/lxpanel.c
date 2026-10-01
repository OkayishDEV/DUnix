#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>

#define SCREEN_W  1024
#define SCREEN_H  768
#define PANEL_H   32
#define PANEL_Y   (SCREEN_H - PANEL_H)

#define MENU_W    220
#define MENU_H    210
#define ITEM_H    26

static const char *menu_categories[] = {
    "=== LXDE Applications ===",
    " [>] Accessories",
    " [>] System Tools",
    " [>] Graphics & Toys",
    " [>] Development",
    " [>] File Manager",
    " -----------------------",
    " [X] Logout / Shutdown"
};

#define NUM_CAT (int)(sizeof(menu_categories) / sizeof(menu_categories[0]))

static void launch(const char *cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = { (char *)cmd, NULL };
        execve(cmd, argv, NULL);
        exit(1);
    }
}

static void draw_panel(Display *dpy, Window panel, GC gc_bg, GC gc_btn, GC gc_text, GC gc_accent) {
    /* 1. Clear Panel background */
    XFillRectangle(dpy, panel, gc_bg, 0, 0, SCREEN_W, PANEL_H);

    /* 2. Top Border Highlight */
    XSetForeground(dpy, gc_accent, 0x004A6B82);
    XDrawLine(dpy, panel, gc_accent, 0, 0, SCREEN_W, 0);

    /* 3. Start Button [ LXDE | Start ] */
    XFillRectangle(dpy, panel, gc_btn, 4, 3, 96, PANEL_H - 6);
    XSetForeground(dpy, gc_accent, 0x001B4F72);
    XDrawRectangle(dpy, panel, gc_accent, 4, 3, 96, PANEL_H - 6);
    XSetForeground(dpy, gc_text, 0x00FFFFFF);
    const char *start_lbl = "LXDE  Start";
    XDrawString(dpy, panel, gc_text, 12, 8, start_lbl, (int)strlen(start_lbl));

    /* 4. Quick Launchers */
    /* Terminal */
    XFillRectangle(dpy, panel, gc_btn, 106, 3, 50, PANEL_H - 6);
    XDrawRectangle(dpy, panel, gc_accent, 106, 3, 50, PANEL_H - 6);
    XDrawString(dpy, panel, gc_text, 112, 8, "Term", 4);

    /* Files */
    XFillRectangle(dpy, panel, gc_btn, 160, 3, 50, PANEL_H - 6);
    XDrawRectangle(dpy, panel, gc_accent, 160, 3, 50, PANEL_H - 6);
    XDrawString(dpy, panel, gc_text, 166, 8, "Files", 5);

    /* Calc */
    XFillRectangle(dpy, panel, gc_btn, 214, 3, 46, PANEL_H - 6);
    XDrawRectangle(dpy, panel, gc_accent, 214, 3, 46, PANEL_H - 6);
    XDrawString(dpy, panel, gc_text, 220, 8, "Calc", 4);

    /* Clock */
    XFillRectangle(dpy, panel, gc_btn, 264, 3, 50, PANEL_H - 6);
    XDrawRectangle(dpy, panel, gc_accent, 264, 3, 50, PANEL_H - 6);
    XDrawString(dpy, panel, gc_text, 270, 8, "Clock", 5);

    /* 5. Taskbar Area (Shows running window labels) */
    XSetForeground(dpy, gc_text, 0x00BDC3C7);
    XDrawString(dpy, panel, gc_text, 330, 8, "| Active Tasks: [xterm] [pcmanfm] [xeyes]", 41);

    /* 6. System Tray Area (Right Side) */
    /* Network */
    XFillRectangle(dpy, panel, gc_btn, 740, 3, 110, PANEL_H - 6);
    XDrawRectangle(dpy, panel, gc_accent, 740, 3, 110, PANEL_H - 6);
    XSetForeground(dpy, gc_text, 0x002ECC71); /* Green net status */
    XDrawString(dpy, panel, gc_text, 746, 8, "eth0: 10.0.2.15", 15);

    /* Memory */
    XFillRectangle(dpy, panel, gc_btn, 856, 3, 76, PANEL_H - 6);
    XDrawRectangle(dpy, panel, gc_accent, 856, 3, 76, PANEL_H - 6);
    XSetForeground(dpy, gc_text, 0x00F39C12); /* Orange RAM status */
    XDrawString(dpy, panel, gc_text, 862, 8, "RAM: 124M", 9);

    /* Clock */
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[32];
    if (tm_info) {
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    } else {
        strcpy(time_str, "12:00:00");
    }

    XFillRectangle(dpy, panel, gc_btn, 938, 3, 80, PANEL_H - 6);
    XDrawRectangle(dpy, panel, gc_accent, 938, 3, 80, PANEL_H - 6);
    XSetForeground(dpy, gc_text, 0x00FFFFFF);
    XDrawString(dpy, panel, gc_text, 946, 8, time_str, (int)strlen(time_str));
}

static void draw_menu(Display *dpy, Window menu, GC gc_header, GC gc_text, GC gc_sel, int selected) {
    XClearWindow(dpy, menu);

    /* Header */
    XFillRectangle(dpy, menu, gc_header, 0, 0, MENU_W, ITEM_H);
    XSetForeground(dpy, gc_text, 0x00FFFFFF);
    XDrawString(dpy, menu, gc_text, 10, 5, menu_categories[0], (int)strlen(menu_categories[0]));

    for (int i = 1; i < NUM_CAT; i++) {
        int y = i * ITEM_H;
        if (i == selected && i != 6) {
            XFillRectangle(dpy, menu, gc_sel, 2, y, MENU_W - 4, ITEM_H);
            XSetForeground(dpy, gc_text, 0x00FFFFFF);
        } else {
            XSetForeground(dpy, gc_text, (i == 7) ? 0x00C0392B : 0x002C3E50);
        }
        XDrawString(dpy, menu, gc_text, 10, y + 5, menu_categories[i], (int)strlen(menu_categories[i]));
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("Starting LXPanel Desktop Panel & Taskbar for LXDE...\n");

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "lxpanel: unable to connect to X11 display server\n");
        return 1;
    }

    Window root = XDefaultRootWindow(dpy);

    /* Create Panel Window across bottom */
    Window panel = XCreateSimpleWindow(dpy, root, 0, PANEL_Y, SCREEN_W, PANEL_H, 0, 0x00000000, 0x002C3E50);
    XStoreName(dpy, panel, "LXPanel");

    /* Create Start Menu Window */
    Window menu = XCreateSimpleWindow(dpy, root, 4, PANEL_Y - MENU_H - 2, MENU_W, MENU_H, 2, 0x002C3E50, 0x00ECEFF1);
    XStoreName(dpy, menu, "LXDE Start Menu");

    GC gc_bg = XCreateGC(dpy, panel, 0, NULL);
    XSetForeground(dpy, gc_bg, 0x00243342); /* Dark metallic slate panel */

    GC gc_btn = XCreateGC(dpy, panel, 0, NULL);
    XSetForeground(dpy, gc_btn, 0x0034495E);

    GC gc_text = XCreateGC(dpy, panel, 0, NULL);
    XSetForeground(dpy, gc_text, 0x00FFFFFF);

    GC gc_accent = XCreateGC(dpy, panel, 0, NULL);
    XSetForeground(dpy, gc_accent, 0x003498DB);

    GC gc_header = XCreateGC(dpy, menu, 0, NULL);
    XSetForeground(dpy, gc_header, 0x002980B9);

    GC gc_sel = XCreateGC(dpy, menu, 0, NULL);
    XSetForeground(dpy, gc_sel, 0x003498DB);

    XSelectInput(dpy, panel, ExposureMask | ButtonPressMask | PointerMotionMask);
    XSelectInput(dpy, menu, ExposureMask | ButtonPressMask | PointerMotionMask);

    XMapWindow(dpy, panel);

    draw_panel(dpy, panel, gc_bg, gc_btn, gc_text, gc_accent);

    bool menu_open = false;
    int selected_item = -1;

    printf("lxpanel: LXDE Desktop Panel active on bottom bar\n");

    XEvent ev;
    while (1) {
        if (XPending(dpy)) {
            XNextEvent(dpy, &ev);

            if (ev.type == Expose) {
                if (ev.xexpose.window == panel) {
                    draw_panel(dpy, panel, gc_bg, gc_btn, gc_text, gc_accent);
                } else if (ev.xexpose.window == menu && menu_open) {
                    draw_menu(dpy, menu, gc_header, gc_text, gc_sel, selected_item);
                }
            } else if (ev.type == ButtonPress) {
                if (ev.xbutton.window == panel) {
                    int bx = ev.xbutton.x;
                    /* Click on Start button (4..100) */
                    if (bx >= 4 && bx <= 100) {
                        menu_open = !menu_open;
                        if (menu_open) {
                            XMapRaised(dpy, menu);
                            draw_menu(dpy, menu, gc_header, gc_text, gc_sel, selected_item);
                        } else {
                            XUnmapWindow(dpy, menu);
                        }
                    }
                    /* Click on Term (106..156) */
                    else if (bx >= 106 && bx <= 156) {
                        launch("/bin/xterm");
                        if (menu_open) { menu_open = false; XUnmapWindow(dpy, menu); }
                    }
                    /* Click on Files (160..210) */
                    else if (bx >= 160 && bx <= 210) {
                        launch("/bin/pcmanfm");
                        if (menu_open) { menu_open = false; XUnmapWindow(dpy, menu); }
                    }
                    /* Click on Calc (214..260) */
                    else if (bx >= 214 && bx <= 260) {
                        launch("/bin/xcalc");
                        if (menu_open) { menu_open = false; XUnmapWindow(dpy, menu); }
                    }
                    /* Click on Clock (264..314) */
                    else if (bx >= 264 && bx <= 314) {
                        launch("/bin/xclock");
                        if (menu_open) { menu_open = false; XUnmapWindow(dpy, menu); }
                    }
                } else if (ev.xbutton.window == menu && menu_open) {
                    int clicked = ev.xbutton.y / ITEM_H;
                    if (clicked == 1) {
                        /* Accessories: Calc */
                        launch("/bin/xcalc");
                    } else if (clicked == 2) {
                        /* System: Terminal */
                        launch("/bin/xterm");
                    } else if (clicked == 3) {
                        /* Graphics: XEyes */
                        launch("/bin/xeyes");
                    } else if (clicked == 4) {
                        /* Development: CC */
                        launch("/bin/xterm");
                    } else if (clicked == 5) {
                        /* PCManFM */
                        launch("/bin/pcmanfm");
                    } else if (clicked == 7) {
                        /* Logout */
                        break;
                    }
                    menu_open = false;
                    XUnmapWindow(dpy, menu);
                }
            } else if (ev.type == MotionNotify && ev.xmotion.window == menu && menu_open) {
                int item = ev.xmotion.y / ITEM_H;
                if (item != selected_item && item > 0 && item < NUM_CAT) {
                    selected_item = item;
                    draw_menu(dpy, menu, gc_header, gc_text, gc_sel, selected_item);
                }
            }
        } else {
            /* Update clock on panel once per second */
            draw_panel(dpy, panel, gc_bg, gc_btn, gc_text, gc_accent);
            sleep(1);
        }

        int status;
        waitpid(-1, &status, WNOHANG);
    }

    XFreeGC(dpy, gc_bg);
    XFreeGC(dpy, gc_btn);
    XFreeGC(dpy, gc_text);
    XFreeGC(dpy, gc_accent);
    XFreeGC(dpy, gc_header);
    XFreeGC(dpy, gc_sel);
    XDestroyWindow(dpy, menu);
    XDestroyWindow(dpy, panel);
    XCloseDisplay(dpy);
    return 0;
}
