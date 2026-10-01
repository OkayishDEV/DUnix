#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/wait.h>
#include <dui/dui.h>
#include <dui/protocol.h>

#define BG_COLOR 0x00F8F9F9
#define HEADER_BG 0x002C3E50
#define HEADER_FG 0x00ECF0F1
#define TEXT_COLOR 0x002C3E50
#define HIGHLIGHT_COLOR 0x003498DB
#define ITEM_HEIGHT 20
#define MAX_ITEMS 100

typedef struct {
    char name[256];
    int is_dir;
    int is_exec;
    off_t size;
} FileItem;

FileItem items[MAX_ITEMS];
int num_items = 0;
int selected_item = -1;
char current_path[1024] = "/";

void load_dir(const char *path) {
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
            items[num_items].is_dir = S_ISDIR(st.st_mode);
            items[num_items].is_exec = (st.st_mode & S_IXUSR) && !S_ISDIR(st.st_mode);
            items[num_items].size = st.st_size;
            num_items++;
        }
    }
    closedir(d);
}

void redraw(DuiConnection *conn, DuiWindow win) {
    dui_clear(conn, win, BG_COLOR);
    // Header
    dui_fill_rect(conn, win, 0, 0, 560, 30, HEADER_BG);
    char title_buf[1024];
    snprintf(title_buf, sizeof(title_buf), "Path: %s", current_path);
    dui_draw_text(conn, win, 10, 8, title_buf, HEADER_FG);
    
    // Toolbar buttons (simplified)
    dui_fill_rect(conn, win, 0, 30, 560, 25, 0x00E0E0E0);
    dui_draw_text(conn, win, 10, 35, "[^ Up]  [/ Root]  [/bin]  [/etc]", TEXT_COLOR);

    // List
    int y_start = 60;
    dui_draw_text(conn, win, 10, y_start, "Type | Name | Size", TEXT_COLOR);
    dui_draw_line(conn, win, 10, y_start + 16, 550, y_start + 16, TEXT_COLOR);
    y_start += 20;

    for (int i = 0; i < num_items; i++) {
        int y = y_start + i * ITEM_HEIGHT;
        if (i == selected_item) {
            dui_fill_rect(conn, win, 5, y, 550, ITEM_HEIGHT, HIGHLIGHT_COLOR);
        }
        
        char type_str[10] = "[FILE]";
        if (items[i].is_dir) strcpy(type_str, "[DIR ]");
        else if (items[i].is_exec) strcpy(type_str, "[EXEC]");
        
        char line_buf[256];
        snprintf(line_buf, sizeof(line_buf), "%s  %-30s %8ld", type_str, items[i].name, (long)items[i].size);
        
        dui_draw_text(conn, win, 10, y + 2, line_buf, i == selected_item ? HEADER_FG : TEXT_COLOR);
    }
    dui_flush(conn, win);
}

void handle_click(int y) {
    if (y >= 60 + 20 && y < 60 + 20 + num_items * ITEM_HEIGHT) {
        int idx = (y - 80) / ITEM_HEIGHT;
        if (idx == selected_item) {
            // double click
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
    } else if (y >= 30 && y < 55) {
        // Poor man's toolbar click detection
        // "[^ Up]  [/ Root]  [/bin]  [/etc]"
        // Just rough x approximation. We don't have x passed here, need to fix signature
    }
}

void handle_mouse_click(int x, int y) {
    if (y >= 30 && y < 55) {
        if (x >= 10 && x < 60) {
            // up
            char *last_slash = strrchr(current_path, '/');
            if (last_slash && last_slash != current_path) {
                *last_slash = '\0';
            } else {
                strcpy(current_path, "/");
            }
        } else if (x >= 70 && x < 130) {
            strcpy(current_path, "/");
        } else if (x >= 140 && x < 190) {
            strcpy(current_path, "/bin");
        } else if (x >= 200 && x < 250) {
            strcpy(current_path, "/etc");
        }
        load_dir(current_path);
    } else {
        handle_click(y);
    }
}

int main(void) {
    DuiConnection *conn = dui_connect();
    if (!conn) return 1;

    DuiWindow win = dui_create_window(conn, 150, 150, 560, 420, "DUnix Files", BG_COLOR, DWS_WIN_DECORATED);
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
