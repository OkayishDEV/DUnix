#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define WIN_W    560
#define WIN_H    420
#define HEADER_H 22
#define TOOL_H   28
#define LIST_Y   (HEADER_H + TOOL_H + 4)
#define ITEM_H   22
#define MAX_ITEMS 64

struct file_entry {
    char name[64];
    bool is_dir;
    bool is_exec;
    size_t size;
};

static struct file_entry entries[MAX_ITEMS];
static int entry_count = 0;
static char current_path[256] = "/";
static int selected_index = -1;

static void load_directory(const char *path) {
    entry_count = 0;
    selected_index = -1;
    strncpy(current_path, path, sizeof(current_path) - 1);

    DIR *d = opendir(current_path);
    if (!d) return;

    struct dirent *de;
    while ((de = readdir(d)) != NULL && entry_count < MAX_ITEMS) {
        if (strcmp(de->d_name, ".") == 0) continue;

        struct file_entry *e = &entries[entry_count++];
        strncpy(e->name, de->d_name, sizeof(e->name) - 1);

        char full_path[512];
        if (strcmp(current_path, "/") == 0) {
            snprintf(full_path, sizeof(full_path), "/%s", de->d_name);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", current_path, de->d_name);
        }

        struct stat st;
        if (stat(full_path, &st) == 0) {
            e->is_dir = (st.st_mode & 0040000) != 0;
            e->is_exec = (st.st_mode & 0111) != 0;
            e->size = (size_t)st.st_size;
        } else {
            e->is_dir = false;
            e->is_exec = false;
            e->size = 0;
        }
    }
    closedir(d);
}

static void draw_pcmanfm(Display *dpy, Window win, GC gc_bg, GC gc_hdr, GC gc_btn, GC gc_text, GC gc_dir, GC gc_exec, GC gc_sel) {
    /* 1. Background */
    XFillRectangle(dpy, win, gc_bg, 0, 0, WIN_W, WIN_H);

    /* 2. Header Titlebar Tab */
    XFillRectangle(dpy, win, gc_hdr, 0, 0, WIN_W, HEADER_H);
    XSetForeground(dpy, gc_text, 0x00FFFFFF);
    const char *title = "pcmanfm - LXDE File Manager";
    XDrawString(dpy, win, gc_text, 10, 4, title, (int)strlen(title));

    /* 3. Toolbar */
    int tool_y = HEADER_H + 3;

    /* [^ Up] */
    XFillRectangle(dpy, win, gc_btn, 8, tool_y, 48, TOOL_H - 6);
    XSetForeground(dpy, gc_text, 0x002C3E50);
    XDrawString(dpy, win, gc_text, 16, tool_y + 4, "^ Up", 4);

    /* [~ Root] */
    XFillRectangle(dpy, win, gc_btn, 62, tool_y, 56, TOOL_H - 6);
    XDrawString(dpy, win, gc_text, 68, tool_y + 4, "/ Root", 6);

    /* [/bin] */
    XFillRectangle(dpy, win, gc_btn, 124, tool_y, 50, TOOL_H - 6);
    XDrawString(dpy, win, gc_text, 130, tool_y + 4, "/bin", 4);

    /* [/etc] */
    XFillRectangle(dpy, win, gc_btn, 180, tool_y, 50, TOOL_H - 6);
    XDrawString(dpy, win, gc_text, 186, tool_y + 4, "/etc", 4);

    /* Location Bar */
    XFillRectangle(dpy, win, gc_btn, 236, tool_y, WIN_W - 244, TOOL_H - 6);
    char path_bar[128];
    snprintf(path_bar, sizeof(path_bar), "Path: %s", current_path);
    XDrawString(dpy, win, gc_text, 244, tool_y + 4, path_bar, (int)strlen(path_bar));

    /* 4. Column Headers */
    int col_y = tool_y + TOOL_H;
    XSetForeground(dpy, gc_text, 0x007F8C8D);
    XDrawLine(dpy, win, gc_text, 8, col_y, WIN_W - 8, col_y);
    XDrawString(dpy, win, gc_text, 12, col_y + 2, "Type", 4);
    XDrawString(dpy, win, gc_text, 64, col_y + 2, "Name", 4);
    XDrawString(dpy, win, gc_text, 360, col_y + 2, "Size", 4);
    XDrawLine(dpy, win, gc_text, 8, col_y + 16, WIN_W - 8, col_y + 16);

    /* 5. Directory List Items */
    int start_y = col_y + 20;
    for (int i = 0; i < entry_count; i++) {
        int item_y = start_y + i * ITEM_H;
        if (item_y + ITEM_H > WIN_H - 8) break;

        if (i == selected_index) {
            XFillRectangle(dpy, win, gc_sel, 8, item_y - 2, WIN_W - 16, ITEM_H);
            XSetForeground(dpy, gc_text, 0x00FFFFFF);
        } else {
            XSetForeground(dpy, gc_text, 0x002C3E50);
        }

        /* Type Icon */
        if (entries[i].is_dir) {
            XSetForeground(dpy, gc_dir, (i == selected_index) ? 0x00FFFFFF : 0x002980B9);
            XDrawString(dpy, win, gc_dir, 12, item_y, "[DIR ]", 6);
        } else if (entries[i].is_exec) {
            XSetForeground(dpy, gc_exec, (i == selected_index) ? 0x00FFFFFF : 0x0027AE60);
            XDrawString(dpy, win, gc_exec, 12, item_y, "[EXEC]", 6);
        } else {
            XSetForeground(dpy, gc_text, (i == selected_index) ? 0x00FFFFFF : 0x007F8C8D);
            XDrawString(dpy, win, gc_text, 12, item_y, "[FILE]", 6);
        }

        /* Name */
        XSetForeground(dpy, gc_text, (i == selected_index) ? 0x00FFFFFF : 0x002C3E50);
        XDrawString(dpy, win, gc_text, 64, item_y, entries[i].name, (int)strlen(entries[i].name));

        /* Size */
        char size_buf[32];
        if (entries[i].is_dir) {
            strcpy(size_buf, "<DIR>");
        } else {
            snprintf(size_buf, sizeof(size_buf), "%lu B", (unsigned long)entries[i].size);
        }
        XDrawString(dpy, win, gc_text, 360, item_y, size_buf, (int)strlen(size_buf));
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "pcmanfm: cannot connect to X11 display server\n");
        return 1;
    }

    Window root = XDefaultRootWindow(dpy);
    Window win = XCreateSimpleWindow(dpy, root, 160, 100, WIN_W, WIN_H, 2, 0x002C3E50, 0x00FAFAFA);
    XStoreName(dpy, win, "pcmanfm");

    GC gc_bg = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_bg, 0x00F8F9F9);

    GC gc_hdr = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_hdr, 0x002C3E50); /* Dark Slate Openbox header */

    GC gc_btn = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_btn, 0x00EAEDED);

    GC gc_text = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_text, 0x002C3E50);

    GC gc_dir = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_dir, 0x002980B9);

    GC gc_exec = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_exec, 0x0027AE60);

    GC gc_sel = XCreateGC(dpy, win, 0, NULL);
    XSetForeground(dpy, gc_sel, 0x003498DB);

    XSelectInput(dpy, win, ExposureMask | ButtonPressMask | KeyPressMask);
    XMapWindow(dpy, win);

    load_directory("/bin");
    draw_pcmanfm(dpy, win, gc_bg, gc_hdr, gc_btn, gc_text, gc_dir, gc_exec, gc_sel);

    XEvent ev;
    while (1) {
        XNextEvent(dpy, &ev);

        if (ev.type == Expose) {
            draw_pcmanfm(dpy, win, gc_bg, gc_hdr, gc_btn, gc_text, gc_dir, gc_exec, gc_sel);
        } else if (ev.type == ButtonPress) {
            int mx = ev.xbutton.x;
            int my = ev.xbutton.y;

            /* Check Toolbar Buttons (y: HEADER_H+3 .. HEADER_H+3+TOOL_H) */
            if (my >= HEADER_H + 3 && my <= HEADER_H + 3 + TOOL_H) {
                if (mx >= 8 && mx <= 56) {
                    /* Up directory */
                    char *slash = strrchr(current_path, '/');
                    if (slash && slash != current_path) {
                        *slash = '\0';
                    } else {
                        strcpy(current_path, "/");
                    }
                    load_directory(current_path);
                    draw_pcmanfm(dpy, win, gc_bg, gc_hdr, gc_btn, gc_text, gc_dir, gc_exec, gc_sel);
                } else if (mx >= 62 && mx <= 118) {
                    /* Root directory */
                    load_directory("/");
                    draw_pcmanfm(dpy, win, gc_bg, gc_hdr, gc_btn, gc_text, gc_dir, gc_exec, gc_sel);
                } else if (mx >= 124 && mx <= 174) {
                    /* /bin */
                    load_directory("/bin");
                    draw_pcmanfm(dpy, win, gc_bg, gc_hdr, gc_btn, gc_text, gc_dir, gc_exec, gc_sel);
                } else if (mx >= 180 && mx <= 230) {
                    /* /etc */
                    load_directory("/etc");
                    draw_pcmanfm(dpy, win, gc_bg, gc_hdr, gc_btn, gc_text, gc_dir, gc_exec, gc_sel);
                }
            } else if (my >= HEADER_H + TOOL_H + 20) {
                /* Check item clicked in directory list */
                int list_start_y = HEADER_H + TOOL_H + 20;
                int idx = (my - list_start_y) / ITEM_H;
                if (idx >= 0 && idx < entry_count) {
                    if (idx == selected_index) {
                        /* Double-click / activate item */
                        if (entries[idx].is_dir) {
                            char next_path[512];
                            if (strcmp(current_path, "/") == 0) {
                                snprintf(next_path, sizeof(next_path), "/%s", entries[idx].name);
                            } else {
                                snprintf(next_path, sizeof(next_path), "%s/%s", current_path, entries[idx].name);
                            }
                            load_directory(next_path);
                        } else if (entries[idx].is_exec) {
                            /* Launch application */
                            char exec_path[512];
                            snprintf(exec_path, sizeof(exec_path), "%s/%s", current_path, entries[idx].name);
                            pid_t pid = fork();
                            if (pid == 0) {
                                char *args[] = { exec_path, NULL };
                                execve(exec_path, args, NULL);
                                exit(1);
                            }
                        }
                    } else {
                        selected_index = idx;
                    }
                    draw_pcmanfm(dpy, win, gc_bg, gc_hdr, gc_btn, gc_text, gc_dir, gc_exec, gc_sel);
                }
            }
        }
    }

    XFreeGC(dpy, gc_bg);
    XFreeGC(dpy, gc_hdr);
    XFreeGC(dpy, gc_btn);
    XFreeGC(dpy, gc_text);
    XFreeGC(dpy, gc_dir);
    XFreeGC(dpy, gc_exec);
    XFreeGC(dpy, gc_sel);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}
