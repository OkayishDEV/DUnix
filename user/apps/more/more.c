#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdbool.h>

#define DEFAULT_ROWS 24
#define DEFAULT_COLS 80

static struct termios g_orig_term;
static bool g_raw_active = false;
static int g_tty_fd = -1;

static void enable_raw_mode(void) {
    if (g_tty_fd < 0) return;
    if (tcgetattr(g_tty_fd, &g_orig_term) == 0) {
        struct termios raw = g_orig_term;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(g_tty_fd, TCSANOW, &raw);
        g_raw_active = true;
    }
}

static void disable_raw_mode(void) {
    if (g_raw_active && g_tty_fd >= 0) {
        tcsetattr(g_tty_fd, TCSANOW, &g_orig_term);
        g_raw_active = false;
    }
}

static int get_screen_rows(void) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 4) {
        return ws.ws_row;
    }
    return DEFAULT_ROWS;
}

static int read_key(void) {
    if (g_tty_fd < 0) return ' ';
    char c = 0;
    if (read(g_tty_fd, &c, 1) > 0) {
        return (unsigned char)c;
    }
    return 'q';
}

static void page_stream(FILE *fp, long file_size) {
    int max_rows = get_screen_rows() - 1;
    char line[1024];
    int line_count = 0;
    long bytes_read = 0;

    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        bytes_read += len;
        fputs(line, stdout);
        line_count++;

        if (line_count >= max_rows) {
            /* Display --More-- prompt */
            if (file_size > 0) {
                int pct = (int)((bytes_read * 100) / file_size);
                if (pct > 100) pct = 100;
                printf("\033[7m--More (%d%%)--\033[0m", pct);
            } else {
                printf("\033[7m--More--\033[0m");
            }
            fflush(stdout);

            int key = read_key();

            /* Clear prompt */
            printf("\r\033[K");
            fflush(stdout);

            if (key == 'q' || key == 'Q' || key == 3 /* Ctrl+C */) {
                break;
            } else if (key == '\n' || key == '\r') {
                /* Advance single line */
                line_count = max_rows - 1;
            } else {
                /* Advance full page */
                line_count = 0;
            }
        }
    }
}

int main(int argc, char **argv) {
    if (isatty(STDIN_FILENO)) {
        g_tty_fd = STDIN_FILENO;
    } else {
        g_tty_fd = open("/dev/tty", O_RDWR);
        if (g_tty_fd < 0) {
            g_tty_fd = open("/dev/console", O_RDWR);
        }
    }

    if (g_tty_fd >= 0) {
        enable_raw_mode();
    }

    if (argc <= 1) {
        page_stream(stdin, -1);
    } else {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "--help") == 0) {
                printf("Usage: %s [FILE]...\n", argv[0]);
                printf("A file perusal filter for crt viewing.\n");
                break;
            }

            FILE *fp = fopen(argv[i], "r");
            if (!fp) {
                fprintf(stderr, "%s: cannot open '%s'\n", argv[0], argv[i]);
                continue;
            }

            fseek(fp, 0, SEEK_END);
            long fsize = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            if (argc > 2) {
                printf("\033[1m::::::::::::::\n%s\n::::::::::::::\033[0m\n", argv[i]);
            }

            page_stream(fp, fsize);
            fclose(fp);
        }
    }

    disable_raw_mode();
    if (g_tty_fd >= 0 && g_tty_fd != STDIN_FILENO) {
        close(g_tty_fd);
    }

    return 0;
}
