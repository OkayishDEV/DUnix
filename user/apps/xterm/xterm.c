#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define COLS 80
#define ROWS 24
#define CHAR_WIDTH  8
#define CHAR_HEIGHT 16
#define HEADER_HEIGHT 20

#define WIN_WIDTH  (COLS * CHAR_WIDTH + 8)
#define WIN_HEIGHT (ROWS * CHAR_HEIGHT + HEADER_HEIGHT + 8)

static char screen_grid[ROWS][COLS];
static int cursor_row = 0;
static int cursor_col = 0;
static char cmd_buf[256];
static int cmd_len = 0;

static void clear_grid(void) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            screen_grid[r][c] = ' ';
        }
    }
    cursor_row = 0;
    cursor_col = 0;
}

static void scroll_grid(void) {
    for (int r = 0; r < ROWS - 1; r++) {
        memcpy(screen_grid[r], screen_grid[r + 1], COLS);
    }
    for (int c = 0; c < COLS; c++) {
        screen_grid[ROWS - 1][c] = ' ';
    }
    cursor_row = ROWS - 1;
}

static void term_putc(char c) {
    if (c == '\n') {
        cursor_col = 0;
        if (++cursor_row >= ROWS) {
            scroll_grid();
        }
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\b' || c == 0x7F) {
        if (cursor_col > 0) {
            cursor_col--;
            screen_grid[cursor_row][cursor_col] = ' ';
        }
    } else if (c >= 32 && c <= 126) {
        screen_grid[cursor_row][cursor_col] = c;
        if (++cursor_col >= COLS) {
            cursor_col = 0;
            if (++cursor_row >= ROWS) {
                scroll_grid();
            }
        }
    }
}

static void term_puts(const char *str) {
    while (*str) {
        term_putc(*str++);
    }
}

static void render_terminal(Display *dpy, Window win, GC gc_bg, GC gc_fg, GC gc_hdr, GC gc_cursor) {
    /* Clear window */
    XFillRectangle(dpy, win, gc_bg, 0, 0, WIN_WIDTH, WIN_HEIGHT);

    /* Draw Header Tab */
    XFillRectangle(dpy, win, gc_hdr, 0, 0, WIN_WIDTH, HEADER_HEIGHT);
    XSetForeground(dpy, gc_fg, 0x00FFFFFF);
    const char *title = "xterm [dunix-sh]";
    XDrawString(dpy, win, gc_fg, 8, 2, title, (int)strlen(title));

    /* Draw Text Grid */
    XSetForeground(dpy, gc_fg, 0x0000FF00); /* Classic Green Phosphor */
    for (int r = 0; r < ROWS; r++) {
        char line[COLS + 1];
        memcpy(line, screen_grid[r], COLS);
        line[COLS] = '\0';
        XDrawString(dpy, win, gc_fg, 4, HEADER_HEIGHT + 4 + r * CHAR_HEIGHT, line, COLS);
    }

    /* Draw Blinking Cursor Block */
    XFillRectangle(dpy, win, gc_cursor, 4 + cursor_col * CHAR_WIDTH,
                   HEADER_HEIGHT + 4 + cursor_row * CHAR_HEIGHT,
                   CHAR_WIDTH, CHAR_HEIGHT);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "xterm: cannot connect to X11 display server\n");
        return 1;
    }

    Window root = XDefaultRootWindow(dpy);
    Window win = XCreateSimpleWindow(dpy, root, 80, 80, WIN_WIDTH, WIN_HEIGHT, 2, 0x004A6B82, 0x00000000);
    XStoreName(dpy, win, "xterm");

    GC gc_bg = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_bg, 0x000A0A0A);

    GC gc_fg = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_fg, 0x0000FF00);

    GC gc_hdr = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_hdr, 0x001B4F72);

    GC gc_cursor = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_cursor, 0x0000FF00);

    XSelectInput(dpy, win, ExposureMask | KeyPressMask | ButtonPressMask);
    XMapWindow(dpy, win);

    clear_grid();
    term_puts("DUnix X11 Terminal Emulator (xterm)\n");
    term_puts("Type Unix commands below:\n\n");
    term_puts("dunix:$ ");

    render_terminal(dpy, win, gc_bg, gc_fg, gc_hdr, gc_cursor);

    XEvent ev;
    while (1) {
        XNextEvent(dpy, &ev);

        if (ev.type == Expose) {
            render_terminal(dpy, win, gc_bg, gc_fg, gc_hdr, gc_cursor);
        } else if (ev.type == KeyPress) {
            char c = (char)(ev.xkey.keycode & 0xFF);
            if (c == '\r' || c == '\n') {
                term_putc('\n');
                cmd_buf[cmd_len] = '\0';

                if (cmd_len > 0) {
                    if (strcmp(cmd_buf, "exit") == 0) {
                        break;
                    } else if (strcmp(cmd_buf, "clear") == 0) {
                        clear_grid();
                    } else if (strcmp(cmd_buf, "neofetch") == 0) {
                        term_puts("  ______    __  __   root@dunix\n");
                        term_puts("  |  _  \\  | | | |  OS: DUnix 64-Bit\n");
                        term_puts("  | | | |  | | | |  Kernel: 0.8.0\n");
                        term_puts("  |___/     \\___/   Desktop: X11/twm\n");
                    } else {
                        /* Execute command */
                        term_puts("[Running: ");
                        term_puts(cmd_buf);
                        term_puts("]\n");
                    }
                }
                cmd_len = 0;
                term_puts("dunix:$ ");
            } else if (c == '\b' || c == 0x7F) {
                if (cmd_len > 0) {
                    cmd_len--;
                    term_putc('\b');
                }
            } else if (c >= 32 && c <= 126) {
                if (cmd_len < (int)sizeof(cmd_buf) - 2) {
                    cmd_buf[cmd_len++] = c;
                    term_putc(c);
                }
            }

            render_terminal(dpy, win, gc_bg, gc_fg, gc_hdr, gc_cursor);
        }
    }

    XFreeGC(dpy, gc_bg);
    XFreeGC(dpy, gc_fg);
    XFreeGC(dpy, gc_hdr);
    XFreeGC(dpy, gc_cursor);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
