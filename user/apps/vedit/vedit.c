#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdbool.h>

#define MAX_ROWS 1024
#define MAX_COLS 256
#define SCREEN_ROWS 22
#define SCREEN_COLS 80

static char lines[MAX_ROWS][MAX_COLS];
static int num_lines = 0;
static int cursor_row = 0;
static int cursor_col = 0;
static int view_top_row = 0;
static char current_filename[128] = "untitled.txt";
static bool modified = false;

static void clear_screen(void) {
    printf("\033[2J\033[H");
    fflush(stdout);
}

static void move_cursor(int row, int col) {
    printf("\033[%d;%dH", row + 2, col + 1);
    fflush(stdout);
}

static void render_screen(void) {
    printf("\033[H");
    /* Title bar */
    printf("\033[7m[ DUnix Visual Editor: %-30s %s ]\033[0m\r\n",
           current_filename, modified ? "[Modified]" : "          ");

    /* Text lines */
    for (int i = 0; i < SCREEN_ROWS; i++) {
        int line_idx = view_top_row + i;
        if (line_idx < num_lines) {
            printf("%-79s\r\n", lines[line_idx]);
        } else {
            printf("~\033[K\r\n");
        }
    }

    /* Status bar */
    printf("\033[7m ^O Save    ^X Exit    ^W WhereIs | Row: %3d, Col: %3d | Lines: %3d\033[0m",
           cursor_row + 1, cursor_col + 1, num_lines);
    fflush(stdout);

    /* Move cursor to current position */
    int screen_y = cursor_row - view_top_row;
    if (screen_y >= 0 && screen_y < SCREEN_ROWS) {
        move_cursor(screen_y, cursor_col);
    }
}

static void load_file(const char *filename) {
    strncpy(current_filename, filename, sizeof(current_filename) - 1);
    num_lines = 0;
    cursor_row = 0;
    cursor_col = 0;
    view_top_row = 0;
    modified = false;

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        /* New file */
        num_lines = 1;
        lines[0][0] = '\0';
        return;
    }

    char buf[MAX_COLS];
    while (num_lines < MAX_ROWS && fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
        strncpy(lines[num_lines++], buf, MAX_COLS - 1);
    }
    fclose(fp);

    if (num_lines == 0) {
        num_lines = 1;
        lines[0][0] = '\0';
    }
}

static void save_file(void) {
    FILE *fp = fopen(current_filename, "w");
    if (!fp) return;

    for (int i = 0; i < num_lines; i++) {
        fputs(lines[i], fp);
        fputc('\n', fp);
    }
    fclose(fp);
    modified = false;
}

static void insert_char(char c) {
    if (cursor_row >= num_lines) return;
    size_t len = strlen(lines[cursor_row]);
    if (len >= MAX_COLS - 2) return;

    if (cursor_col > (int)len) cursor_col = (int)len;

    memmove(&lines[cursor_row][cursor_col + 1],
            &lines[cursor_row][cursor_col],
            len - cursor_col + 1);
    lines[cursor_row][cursor_col] = c;
    cursor_col++;
    modified = true;
}

static void insert_newline(void) {
    if (num_lines >= MAX_ROWS - 1) return;

    size_t len = strlen(lines[cursor_row]);
    if (cursor_col > (int)len) cursor_col = (int)len;

    /* Shift lines down */
    for (int i = num_lines; i > cursor_row + 1; i--) {
        strcpy(lines[i], lines[i - 1]);
    }

    /* Split line */
    strcpy(lines[cursor_row + 1], &lines[cursor_row][cursor_col]);
    lines[cursor_row][cursor_col] = '\0';

    num_lines++;
    cursor_row++;
    cursor_col = 0;
    if (cursor_row >= view_top_row + SCREEN_ROWS) {
        view_top_row = cursor_row - SCREEN_ROWS + 1;
    }
    modified = true;
}

static void delete_char(void) {
    if (cursor_col > 0) {
        size_t len = strlen(lines[cursor_row]);
        memmove(&lines[cursor_row][cursor_col - 1],
                &lines[cursor_row][cursor_col],
                len - cursor_col + 1);
        cursor_col--;
        modified = true;
    } else if (cursor_row > 0) {
        /* Join with previous line */
        size_t prev_len = strlen(lines[cursor_row - 1]);
        size_t curr_len = strlen(lines[cursor_row]);
        if (prev_len + curr_len < MAX_COLS - 1) {
            strcat(lines[cursor_row - 1], lines[cursor_row]);
            for (int i = cursor_row; i < num_lines - 1; i++) {
                strcpy(lines[i], lines[i + 1]);
            }
            num_lines--;
            cursor_row--;
            cursor_col = (int)prev_len;
            if (cursor_row < view_top_row) {
                view_top_row = cursor_row;
            }
            modified = true;
        }
    }
}

int main(int argc, char **argv) {
    const char *file = (argc > 1) ? argv[1] : "scratch.txt";
    load_file(file);

    clear_screen();

    for (;;) {
        render_screen();

        char c;
        if (read(STDIN_FILENO, &c, 1) <= 0) break;

        /* Ctrl+X = Exit */
        if (c == 0x18) {
            clear_screen();
            break;
        }
        /* Ctrl+O = Save */
        else if (c == 0x0F) {
            save_file();
        }
        /* Enter / Newline */
        else if (c == '\r' || c == '\n') {
            insert_newline();
        }
        /* Backspace */
        else if (c == 0x7F || c == 0x08) {
            delete_char();
        }
        /* ANSI Escape Sequence (e.g. Arrow keys) */
        else if (c == 0x1B) {
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
                if (seq[0] == '[') {
                    if (seq[1] == 'A') { /* Up */
                        if (cursor_row > 0) {
                            cursor_row--;
                            if (cursor_row < view_top_row) view_top_row = cursor_row;
                        }
                    } else if (seq[1] == 'B') { /* Down */
                        if (cursor_row < num_lines - 1) {
                            cursor_row++;
                            if (cursor_row >= view_top_row + SCREEN_ROWS) view_top_row++;
                        }
                    } else if (seq[1] == 'C') { /* Right */
                        size_t len = strlen(lines[cursor_row]);
                        if (cursor_col < (int)len) cursor_col++;
                    } else if (seq[1] == 'D') { /* Left */
                        if (cursor_col > 0) cursor_col--;
                    }
                }
            }
        }
        /* Printable characters */
        else if (c >= 32 && c <= 126) {
            insert_char(c);
        }
    }

    return 0;
}
