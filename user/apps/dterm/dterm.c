#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <dui/dui.h>
#include <dui/protocol.h>

#define BG_COLOR        0x0010141E
#define HEADER_BG       0x00171D2B
#define HEADER_BORDER   0x00242E42
#define HEADER_FG       0x0094A3B8
#define FG_COLOR        0x00CDD6F4
#define CURSOR_COLOR    0x0038BDF8

#define HEADER_H        24
#define MAX_LINES       24
#define MAX_COLS        78
#define FONT_W          8
#define FONT_H          16

static char term_buf[MAX_LINES][MAX_COLS + 2];
static int cursor_x = 0;
static int cursor_y = 0;
static int esc_state = 0;

static void scroll_up(void) {
    for (int i = 0; i < MAX_LINES - 1; i++) {
        memcpy(term_buf[i], term_buf[i + 1], MAX_COLS + 2);
    }
    memset(term_buf[MAX_LINES - 1], 0, MAX_COLS + 2);
    if (cursor_y > 0) cursor_y--;
}

static void print_char(char c) {
    if (esc_state == 1) {
        if (c == '[') {
            esc_state = 2;
            return;
        }
        esc_state = 0;
    } else if (esc_state == 2) {
        if ((c >= '0' && c <= '9') || c == ';' || c == '?' || c == ' ' || c == '(' || c == ')') {
            return;
        }
        esc_state = 0;
        return;
    }

    if (c == '\033') {
        esc_state = 1;
        return;
    }
    if (c == '\r') return;
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= MAX_LINES) {
            scroll_up();
        }
    } else if (c == '\b' || c == 127) {
        if (cursor_x > 0) {
            cursor_x--;
            term_buf[cursor_y][cursor_x] = '\0';
        }
    } else if (c == '\t') {
        for (int i = 0; i < 4; i++) print_char(' ');
    } else if ((unsigned char)c >= 32 && (unsigned char)c < 127) {
        if (cursor_x >= MAX_COLS) {
            cursor_x = 0;
            cursor_y++;
            if (cursor_y >= MAX_LINES) {
                scroll_up();
            }
        }
        term_buf[cursor_y][cursor_x++] = c;
        term_buf[cursor_y][cursor_x] = '\0';
    }
}

static void print_str(const char *s) {
    if (!s) return;
    while (*s) {
        print_char(*s++);
    }
}

static void redraw(DuiConnection *conn, DuiWindow win) {
    dui_clear(conn, win, BG_COLOR);

    /* Sub-header tab strip */
    dui_fill_rect(conn, win, 0, 0, 640, HEADER_H, HEADER_BG);
    dui_draw_line(conn, win, 0, HEADER_H - 1, 640, HEADER_H - 1, HEADER_BORDER);

    /* Indicator dots */
    dui_fill_circle(conn, win, 12, 12, 4, 0x00EF4444);
    dui_fill_circle(conn, win, 24, 12, 4, 0x00F59E0B);
    dui_fill_circle(conn, win, 36, 12, 4, 0x0010B981);

    dui_draw_text(conn, win, 52, 4, "dunix@workstation: ~ (/bin/sh)", HEADER_FG);

    /* Terminal buffer */
    for (int i = 0; i < MAX_LINES; i++) {
        if (term_buf[i][0] != '\0') {
            dui_draw_text(conn, win, 8, HEADER_H + 4 + i * FONT_H, term_buf[i], FG_COLOR);
        }
    }

    /* Cursor */
    dui_fill_rect(conn, win, 8 + cursor_x * FONT_W, HEADER_H + 4 + cursor_y * FONT_H, FONT_W, FONT_H, CURSOR_COLOR);
    dui_flush(conn, win);
}

static void execute_cmd(const char *cmd) {
    if (!cmd || *cmd == '\0') return;

    if (strcmp(cmd, "clear") == 0) {
        memset(term_buf, 0, sizeof(term_buf));
        cursor_x = 0;
        cursor_y = 0;
        return;
    }

    if (strcmp(cmd, "exit") == 0) {
        exit(0);
    }

    int pfd[2];
    if (pipe(pfd) < 0) {
        print_str("Error creating pipe\n");
        return;
    }
    pid_t pid = fork();
    if (pid == 0) {
        close(pfd[0]);
        dup2(pfd[1], STDOUT_FILENO);
        dup2(pfd[1], STDERR_FILENO);
        close(pfd[1]);
        int null_fd = open("/dev/null", O_RDONLY);
        if (null_fd >= 0) {
            dup2(null_fd, STDIN_FILENO);
            close(null_fd);
        }
        char *argv[] = {"/bin/sh", "-c", (char*)cmd, NULL};
        execve("/bin/sh", argv, NULL);
        exit(1);
    } else if (pid > 0) {
        close(pfd[1]);
        char buf[128];
        ssize_t n;
        while ((n = read(pfd[0], buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            print_str(buf);
        }
        close(pfd[0]);
        waitpid(pid, NULL, 0);
    } else {
        print_str("Error forking\n");
    }
}

int main(void) {
    DuiConnection *conn = dui_connect();
    if (!conn) {
        fprintf(stderr, "Failed to connect to display server\n");
        return 1;
    }

    DuiWindow win = dui_create_window(conn, 30, 44, 640, 420, "Terminal Emulator", BG_COLOR, DWS_WIN_DECORATED);
    dui_show(conn, win);

    memset(term_buf, 0, sizeof(term_buf));
    print_str("DUnix 64-Bit Workstation Terminal\n");
    print_str("Type 'help' for built-ins or launch GUI apps\n\n");
    print_str("dunix$ ");
    redraw(conn, win);

    char cmd_buf[256];
    int cmd_len = 0;

    DuiEvent ev;
    while (1) {
        if (dui_next_event(conn, &ev) > 0) {
            if (ev.type == DWS_EV_CLOSE_REQ && ev.window == win) {
                break;
            } else if (ev.type == DWS_EV_EXPOSE && ev.window == win) {
                redraw(conn, win);
            } else if (ev.type == DWS_EV_KEY_DOWN && ev.window == win) {
                char ch = ev.key.ch;
                if (ch == '\n' || ch == '\r') {
                    print_char('\n');
                    cmd_buf[cmd_len] = '\0';
                    execute_cmd(cmd_buf);
                    cmd_len = 0;
                    print_str("dunix$ ");
                } else if (ch == '\b' || ch == 127) {
                    if (cmd_len > 0) {
                        cmd_len--;
                        print_char('\b');
                    }
                } else if ((unsigned char)ch >= 32 && (unsigned char)ch < 127) {
                    if (cmd_len < (int)sizeof(cmd_buf) - 1) {
                        cmd_buf[cmd_len++] = ch;
                        print_char(ch);
                    }
                }
                redraw(conn, win);
            }
        }
    }

    dui_destroy_window(conn, win);
    dui_disconnect(conn);
    return 0;
}
