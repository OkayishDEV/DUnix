#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/wait.h>
#include <dui/dui.h>
#include <dui/protocol.h>

#define BG_COLOR        0x00131722
#define HEADER_BG       0x001B2232
#define HEADER_BORDER   0x002A354C
#define HEADER_FG       0x00F8FAFC
#define SIDEBAR_BG      0x00111520
#define SIDEBAR_FG      0x0094A3B8
#define SIDEBAR_HI      0x001E283C
#define TEXT_COLOR      0x00E2E8F0
#define TEXT_MUTED      0x0064748B
#define HIGHLIGHT_COLOR 0x001E2A40
#define HIGHLIGHT_BAR   0x0038BDF8

#define BADGE_DIR_BG    0x001D4ED8
#define BADGE_EXEC_BG   0x00047857
#define BADGE_FILE_BG   0x00334155
#define BADGE_FG        0x00FFFFFF

#define ITEM_HEIGHT     22
#define MAX_ITEMS       100

typedef struct {
    char name[256];
    int is_dir;
    int is_exec;
    off_t size;
} FileItem;

static FileItem items[MAX_ITEMS];
static int num_items = 0;
static int selected_item = -1;
static char current_path[1024] = "/";

static void load_dir(const char *path) {
    DIR *d = opendir(path);
    if (!d) return;
    num_items = 0;
    selected_item = -1;
    struct dirent *ent;
    char fullpath[1024];
    while ((ent = readdir(d)) != NULL && num_items < MAX_ITEMS) {
        if (strcmp(ent->d_name, ".") == 0) continue;
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, ent->d_name);
        struct stat st;
        if (stat(fullpath, &st) == 0) {
            strncpy(items[num_items].name, ent->d_name, 255);
            items[num_items].name[255] = '\0';
            items[num_items].is_dir = S_ISDIR(st.st_mode);
            items[num_items].is_exec = (st.st_mode & S_IXUSR) && !S_ISDIR(st.st_mode);
            items[num_items].size = st.st_size;
            num_items++;
        }
    }
    closedir(d);
}

static void redraw(DuiConnection *conn, DuiWindow win) {
    dui_clear(conn, win, BG_COLOR);

    /* 1. Top Header & Breadcrumb Bar */
    dui_fill_rect(conn, win, 0, 0, 580, 32, HEADER_BG);
    dui_draw_line(conn, win, 0, 31, 580, 31, HEADER_BORDER);

    char title_buf[1024];
    snprintf(title_buf, sizeof(title_buf), "Location: %s", current_path);
    dui_draw_text(conn, win, 12, 8, title_buf, HEADER_FG);

    /* 2. Action Toolbar */
    dui_fill_rect(conn, win, 0, 32, 580, 26, 0x00161C2A);
    dui_draw_line(conn, win, 0, 57, 580, 57, HEADER_BORDER);

    struct { const char *lbl; int x, w; } tb[] = {
        { "[^] Up",    10, 56 },
        { "[/] Root",  72, 60 },
        { "Bin",      138, 44 },
        { "Etc",      188, 44 },
        { "Dev",      238, 44 },
        { "Mnt",      288, 44 }
    };
    for (size_t i = 0; i < sizeof(tb)/sizeof(tb[0]); i++) {
        dui_fill_rect(conn, win, tb[i].x, 34, tb[i].w, 20, 0x00222B3E);
        dui_draw_rect(conn, win, tb[i].x, 34, tb[i].w, 20, 0x0033405C);
        dui_draw_text(conn, win, tb[i].x + 6, 36, tb[i].lbl, TEXT_COLOR);
    }

    /* 3. Left Sidebar: Places */
    int sb_w = 120;
    dui_fill_rect(conn, win, 0, 58, sb_w, 420 - 58, SIDEBAR_BG);
    dui_draw_line(conn, win, sb_w, 58, sb_w, 420, HEADER_BORDER);

    dui_draw_text(conn, win, 12, 66, "PLACES", TEXT_MUTED);
    dui_draw_line(conn, win, 12, 82, sb_w - 12, 82, 0x001E2638);

    const char *places[] = {
        "/ Root",
        "  Apps (/bin)",
        "  Cfg  (/etc)",
        "  Dev  (/dev)",
        "  Disk (/mnt)"
    };
    for (int i = 0; i < 5; i++) {
        dui_draw_text(conn, win, 10, 92 + i * 24, places[i], SIDEBAR_FG);
    }

    /* 4. Main File List Header */
    int list_x = sb_w + 12;
    dui_draw_text(conn, win, list_x, 66, "Name", TEXT_MUTED);
    dui_draw_text(conn, win, list_x + 240, 66, "Type", TEXT_MUTED);
    dui_draw_text(conn, win, list_x + 310, 66, "Size", TEXT_MUTED);
    dui_draw_line(conn, win, list_x, 82, 570, 82, HEADER_BORDER);

    /* 5. File Items */
    int y_start = 88;
    for (int i = 0; i < num_items; i++) {
        int y = y_start + i * ITEM_HEIGHT;
        if (y + ITEM_HEIGHT > 420) break;

        bool sel = (i == selected_item);
        if (sel) {
            dui_fill_rect(conn, win, list_x - 6, y - 2, 580 - list_x, ITEM_HEIGHT, HIGHLIGHT_COLOR);
            dui_fill_rect(conn, win, list_x - 6, y - 2, 3, ITEM_HEIGHT, HIGHLIGHT_BAR);
        }

        /* Name */
        char name_buf[32];
        strncpy(name_buf, items[i].name, 28);
        name_buf[28] = '\0';
        dui_draw_text(conn, win, list_x, y + 1, name_buf, sel ? 0x00FFFFFF : TEXT_COLOR);

        /* Type badge */
        const char *t_str = "FILE";
        uint32_t t_bg = BADGE_FILE_BG;
        if (items[i].is_dir) { t_str = "DIR"; t_bg = BADGE_DIR_BG; }
        else if (items[i].is_exec) { t_str = "BIN"; t_bg = BADGE_EXEC_BG; }

        dui_fill_rect(conn, win, list_x + 240, y, 42, 16, t_bg);
        dui_draw_text(conn, win, list_x + 246, y, t_str, BADGE_FG);

        /* Size */
        char sz_buf[32];
        if (items[i].is_dir) {
            strcpy(sz_buf, "<DIR>");
        } else if (items[i].size >= 1024 * 1024) {
            snprintf(sz_buf, sizeof(sz_buf), "%ld M", (long)(items[i].size / (1024 * 1024)));
        } else if (items[i].size >= 1024) {
            snprintf(sz_buf, sizeof(sz_buf), "%ld K", (long)(items[i].size / 1024));
        } else {
            snprintf(sz_buf, sizeof(sz_buf), "%ld B", (long)items[i].size);
        }
        dui_draw_text(conn, win, list_x + 310, y + 1, sz_buf, TEXT_MUTED);
    }

    dui_flush(conn, win);
}

static void handle_click(int y) {
    int y_start = 88;
    if (y >= y_start && y < y_start + num_items * ITEM_HEIGHT) {
        int idx = (y - y_start) / ITEM_HEIGHT;
        if (idx == selected_item) {
            /* Double click */
            if (items[idx].is_dir) {
                if (strcmp(items[idx].name, "..") == 0) {
                    char *last_slash = strrchr(current_path, '/');
                    if (last_slash && last_slash != current_path) {
                        *last_slash = '\0';
                    } else {
                        strcpy(current_path, "/");
                    }
                } else {
                    if (strcmp(current_path, "/") != 0) strcat(current_path, "/");
                    strcat(current_path, items[idx].name);
                }
                load_dir(current_path);
            } else if (items[idx].is_exec) {
                pid_t pid = fork();
                if (pid == 0) {
                    char fullpath[1024];
                    if (strcmp(current_path, "/") == 0) snprintf(fullpath, sizeof(fullpath), "/%s", items[idx].name);
                    else snprintf(fullpath, sizeof(fullpath), "%s/%s", current_path, items[idx].name);
                    char *argv[] = {fullpath, NULL};
                    execve(fullpath, argv, NULL);
                    exit(1);
                }
            }
        } else {
            selected_item = idx;
        }
    }
}

static void handle_mouse_click(int x, int y) {
    /* Toolbar Clicks */
    if (y >= 32 && y < 58) {
        if (x >= 10 && x < 66) {
            char *last_slash = strrchr(current_path, '/');
            if (last_slash && last_slash != current_path) {
                *last_slash = '\0';
            } else {
                strcpy(current_path, "/");
            }
        } else if (x >= 72 && x < 132) {
            strcpy(current_path, "/");
        } else if (x >= 138 && x < 182) {
            strcpy(current_path, "/bin");
        } else if (x >= 188 && x < 232) {
            strcpy(current_path, "/etc");
        } else if (x >= 238 && x < 282) {
            strcpy(current_path, "/dev");
        } else if (x >= 288 && x < 332) {
            strcpy(current_path, "/mnt");
        }
        load_dir(current_path);
    } else if (x < 120 && y >= 88) {
        /* Sidebar Places Clicks */
        int s_idx = (y - 88) / 24;
        if (s_idx == 0) strcpy(current_path, "/");
        else if (s_idx == 1) strcpy(current_path, "/bin");
        else if (s_idx == 2) strcpy(current_path, "/etc");
        else if (s_idx == 3) strcpy(current_path, "/dev");
        else if (s_idx == 4) strcpy(current_path, "/mnt");
        load_dir(current_path);
    } else {
        handle_click(y);
    }
}

int main(void) {
    DuiConnection *conn = dui_connect();
    if (!conn) return 1;

    DuiWindow win = dui_create_window(conn, 140, 120, 580, 420, "File Manager", BG_COLOR, DWS_WIN_DECORATED);
    dui_show(conn, win);

    load_dir(current_path);
    redraw(conn, win);

    DuiEvent ev;
    while (1) {
        if (dui_next_event(conn, &ev) > 0) {
            if (ev.type == DWS_EV_CLOSE_REQ && ev.window == win) {
                break;
            } else if (ev.type == DWS_EV_EXPOSE && ev.window == win) {
                redraw(conn, win);
            } else if (ev.type == DWS_EV_MOUSE_DOWN && ev.window == win) {
                handle_mouse_click(ev.mouse.x, ev.mouse.y);
                redraw(conn, win);
            }
        }
    }

    dui_destroy_window(conn, win);
    dui_disconnect(conn);
    return 0;
}
