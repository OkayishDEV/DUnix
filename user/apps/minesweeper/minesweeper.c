/*
 * DUnix Minesweeper - Dual TUI and GUI Classic Logic Puzzle
 * Supports both full-color interactive Terminal (TUI) and DWS Window (GUI)
 * in the exact same binary.
 *
 * Usage:
 *   minesweeper          (Auto-detects DWS GUI, falls back to TUI if in terminal)
 *   minesweeper --tui    (Forces text-mode terminal interface)
 *   minesweeper --gui    (Forces DWS graphical window interface)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <signal.h>
#include <poll.h>
#include <dui/dui.h>
#include <dui/protocol.h>

#define ROWS 9
#define COLS 9
#define TOTAL_MINES 10

#define STATE_READY   0
#define STATE_PLAYING 1
#define STATE_WON     2
#define STATE_LOST    3

typedef struct {
    int  is_mine;
    int  is_revealed;
    int  is_flagged;
    int  neighbors;
} Cell;

static Cell g_board[ROWS][COLS];
static int  g_state = STATE_READY;
static int  g_flags_count = 0;
static int  g_cursor_r = 4;
static int  g_cursor_c = 4;
static time_t g_start_time = 0;
static int  g_elapsed_sec = 0;
static int  g_exploded_r = -1;
static int  g_exploded_c = -1;

static void place_mines(int first_r, int first_c) {
    int placed = 0;
    while (placed < TOTAL_MINES) {
        int r = rand() % ROWS;
        int c = rand() % COLS;
        /* First clicked cell and its 8 neighbors are kept safe */
        if (abs(r - first_r) <= 1 && abs(c - first_c) <= 1) continue;
        if (!g_board[r][c].is_mine) {
            g_board[r][c].is_mine = 1;
            placed++;
        }
    }

    /* Compute neighbor counts */
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (g_board[r][c].is_mine) continue;
            int count = 0;
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    int nr = r + dr, nc = c + dc;
                    if (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS) {
                        if (g_board[nr][nc].is_mine) count++;
                    }
                }
            }
            g_board[r][c].neighbors = count;
        }
    }
}

static void init_game(void) {
    memset(g_board, 0, sizeof(g_board));
    g_state = STATE_READY;
    g_flags_count = 0;
    g_start_time = 0;
    g_elapsed_sec = 0;
    g_exploded_r = -1;
    g_exploded_c = -1;
}

static void check_win(void) {
    int unrevealed = 0;
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (!g_board[r][c].is_revealed) unrevealed++;
        }
    }
    if (unrevealed == TOTAL_MINES) {
        g_state = STATE_WON;
        /* Auto-flag all mines */
        for (int r = 0; r < ROWS; r++) {
            for (int c = 0; c < COLS; c++) {
                if (g_board[r][c].is_mine) g_board[r][c].is_flagged = 1;
            }
        }
        g_flags_count = TOTAL_MINES;
    }
}

static void reveal_cell(int r, int c) {
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) return;
    if (g_board[r][c].is_revealed || g_board[r][c].is_flagged) return;

    if (g_state == STATE_READY) {
        place_mines(r, c);
        g_state = STATE_PLAYING;
        g_start_time = time(NULL);
    }

    if (g_state != STATE_PLAYING) return;

    if (g_board[r][c].is_mine) {
        /* Game Over: Mine Hit */
        g_state = STATE_LOST;
        g_exploded_r = r;
        g_exploded_c = c;
        /* Reveal all mines */
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                if (g_board[row][col].is_mine) g_board[row][col].is_revealed = 1;
            }
        }
        return;
    }

    g_board[r][c].is_revealed = 1;

    /* Flood fill if empty (0 neighbors) */
    if (g_board[r][c].neighbors == 0) {
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                reveal_cell(r + dr, c + dc);
            }
        }
    }

    check_win();
}

static void toggle_flag(int r, int c) {
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) return;
    if (g_state == STATE_WON || g_state == STATE_LOST) return;
    if (g_board[r][c].is_revealed) return;

    if (g_board[r][c].is_flagged) {
        g_board[r][c].is_flagged = 0;
        g_flags_count--;
    } else {
        if (g_flags_count < TOTAL_MINES) {
            g_board[r][c].is_flagged = 1;
            g_flags_count++;
        }
    }
}

/* ========================================================================= */
/* TUI (Terminal User Interface) Engine                                      */
/* ========================================================================= */

static struct termios g_orig_termios;
static int g_tui_active = 0;

static void tui_restore_terminal(void) {
    if (g_tui_active) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
        printf("\033[?25h\033[0m\n");
        fflush(stdout);
        g_tui_active = 0;
    }
}

static void tui_sigint_handler(int sig) {
    (void)sig;
    tui_restore_terminal();
    exit(0);
}

static void tui_init_terminal(void) {
    if (tcgetattr(STDIN_FILENO, &g_orig_termios) == 0) {
        struct termios raw = g_orig_termios;
        raw.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        g_tui_active = 1;
        atexit(tui_restore_terminal);
        signal(SIGINT, tui_sigint_handler);
        signal(SIGTERM, tui_sigint_handler);
    }
    printf("\033[?25l");
    fflush(stdout);
}

static const char *num_ansi_colors[] = {
    "",
    "\033[1;34m", /* 1: Blue */
    "\033[1;32m", /* 2: Green */
    "\033[1;31m", /* 3: Red */
    "\033[0;34m", /* 4: Navy */
    "\033[0;31m", /* 5: Dark Red */
    "\033[1;36m", /* 6: Cyan */
    "\033[1;35m", /* 7: Magenta */
    "\033[1;37m"  /* 8: White */
};

static void tui_render(void) {
    if (g_state == STATE_PLAYING && g_start_time > 0) {
        g_elapsed_sec = (int)(time(NULL) - g_start_time);
    }

    printf("\033[2J\033[H");

    /* Header */
    printf("\033[1;36m+-------------------------------------------+\033[0m\n");
    printf("\033[1;36m|\033[1;37m   DUnix Minesweeper (TUI)                 \033[1;36m|\033[0m\n");

    const char *face = ":-)";
    if (g_state == STATE_WON)  face = "B-)";
    if (g_state == STATE_LOST) face = "X-P";

    printf("\033[1;36m|\033[0m Mines: \033[1;31m%02d\033[0m   [Face: \033[1;33m%s\033[0m]   Time: \033[1;32m%03ds\033[0m   \033[1;36m|\033[0m\n",
           TOTAL_MINES - g_flags_count, face, g_elapsed_sec);
    printf("\033[1;36m+-------------------------------------------+\033[0m\n");

    /* Column coordinates */
    printf("     ");
    for (int c = 0; c < COLS; c++) printf(" %d ", c + 1);
    printf("\n");

    printf("    +");
    for (int c = 0; c < COLS; c++) printf("---");
    printf("-+\n");

    /* Rows */
    for (int r = 0; r < ROWS; r++) {
        printf("  %d |", r + 1);
        for (int c = 0; c < COLS; c++) {
            int is_cursor = (r == g_cursor_r && c == g_cursor_c);
            char left_delim  = is_cursor ? '[' : ' ';
            char right_delim = is_cursor ? ']' : ' ';

            if (g_board[r][c].is_flagged) {
                printf("%c\033[1;33mP\033[0m%c", left_delim, right_delim);
            } else if (!g_board[r][c].is_revealed) {
                printf("%c\033[0;37m.\033[0m%c", left_delim, right_delim);
            } else if (g_board[r][c].is_mine) {
                if (r == g_exploded_r && c == g_exploded_c) {
                    printf("%c\033[1;41;37m*\033[0m%c", left_delim, right_delim);
                } else {
                    printf("%c\033[1;31m*\033[0m%c", left_delim, right_delim);
                }
            } else if (g_board[r][c].neighbors > 0) {
                int n = g_board[r][c].neighbors;
                printf("%c%s%d\033[0m%c", left_delim, num_ansi_colors[n], n, right_delim);
            } else {
                printf("%c \033[0m%c", left_delim, right_delim);
            }
        }
        printf(" |\n");
    }

    printf("    +");
    for (int c = 0; c < COLS; c++) printf("---");
    printf("-+\n");

    /* Status banner */
    if (g_state == STATE_WON) {
        printf("\033[1;32m  *** VICTORY! ALL MINES CLEARED! ***      \033[0m\n");
        printf("\033[1;33m  Press [R] to Play Again  |  [Q] to Quit   \033[0m\n");
    } else if (g_state == STATE_LOST) {
        printf("\033[1;31m  *** BOOM! GAME OVER! ***                  \033[0m\n");
        printf("\033[1;33m  Press [R] to Play Again  |  [Q] to Quit   \033[0m\n");
    } else {
        printf("  [WASD / Arrows] Move Cursor               \n");
        printf("  [Space] Reveal   [F] Flag   [R] New Game  \n");
        printf("  [Q] Quit                                  \n");
    }
    fflush(stdout);
}

static void run_tui(void) {
    tui_init_terminal();
    init_game();

    while (1) {
        tui_render();

        struct pollfd pfd;
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int poll_res = poll(&pfd, 1, 500);
        if (poll_res > 0 && (pfd.revents & POLLIN)) {
            char ch = 0;
            if (read(STDIN_FILENO, &ch, 1) > 0) {
                if (ch == '\033') {
                    struct pollfd pseq = { .fd = STDIN_FILENO, .events = POLLIN, .revents = 0 };
                    if (poll(&pseq, 1, 50) > 0 && (pseq.revents & POLLIN)) {
                        char seq[2];
                        if (read(STDIN_FILENO, &seq[0], 1) > 0 &&
                            poll(&pseq, 1, 50) > 0 && (pseq.revents & POLLIN) &&
                            read(STDIN_FILENO, &seq[1], 1) > 0) {
                            if (seq[0] == '[') {
                                if (seq[1] == 'A' && g_cursor_r > 0) g_cursor_r--;
                                else if (seq[1] == 'B' && g_cursor_r < ROWS - 1) g_cursor_r++;
                                else if (seq[1] == 'C' && g_cursor_c < COLS - 1) g_cursor_c++;
                                else if (seq[1] == 'D' && g_cursor_c > 0) g_cursor_c--;
                            }
                        }
                    } else {
                        break; /* Standalone ESC quits */
                    }
                } else if ((ch == 'w' || ch == 'W') && g_cursor_r > 0) {
                    g_cursor_r--;
                } else if ((ch == 's' || ch == 'S') && g_cursor_r < ROWS - 1) {
                    g_cursor_r++;
                } else if ((ch == 'a' || ch == 'A') && g_cursor_c > 0) {
                    g_cursor_c--;
                } else if ((ch == 'd' || ch == 'D') && g_cursor_c < COLS - 1) {
                    g_cursor_c++;
                } else if (ch == ' ' || ch == '\n' || ch == '\r') {
                    reveal_cell(g_cursor_r, g_cursor_c);
                } else if (ch == 'f' || ch == 'F') {
                    toggle_flag(g_cursor_r, g_cursor_c);
                } else if (ch == 'r' || ch == 'R') {
                    init_game();
                } else if (ch == 'q' || ch == 'Q') {
                    break;
                }
            }
        }
    }

    tui_restore_terminal();
}

/* ========================================================================= */
/* GUI (DWS Desktop Window Interface) Engine                                 */
/* ========================================================================= */

#define WIN_W 320
#define WIN_H 390
#define CELL_SZ 30
#define BOARD_OFFSET_X 25
#define BOARD_OFFSET_Y 70

#define COLOR_FRAME_BG     0x00C0C0C0
#define COLOR_BEVEL_LIGHT  0x00FFFFFF
#define COLOR_BEVEL_DARK   0x00808080
#define COLOR_BEVEL_BLACK  0x00000000
#define COLOR_FACE_YELLOW  0x00F1C40F
#define COLOR_RED_DIGIT    0x00E74C3C
#define COLOR_DIGIT_BG     0x00000000

static const uint32_t num_gui_colors[] = {
    0x00000000,
    0x000000FF, /* 1: Blue */
    0x00008000, /* 2: Green */
    0x00FF0000, /* 3: Red */
    0x00000080, /* 4: Dark Blue */
    0x00800000, /* 5: Maroon */
    0x00008080, /* 6: Cyan */
    0x00000000, /* 7: Black */
    0x00808080  /* 8: Grey */
};

/* Draws standard 3D raised border */
static void draw_raised_box(DuiConnection *conn, DuiWindow win, int x, int y, int w, int h) {
    dui_fill_rect(conn, win, x, y, w, h, COLOR_FRAME_BG);
    dui_draw_line(conn, win, x, y, x + w - 1, y, COLOR_BEVEL_LIGHT);
    dui_draw_line(conn, win, x, y, x, y + h - 1, COLOR_BEVEL_LIGHT);
    dui_draw_line(conn, win, x, y + h - 1, x + w - 1, y + h - 1, COLOR_BEVEL_DARK);
    dui_draw_line(conn, win, x + w - 1, y, x + w - 1, y + h - 1, COLOR_BEVEL_DARK);
}

/* Draws standard 3D sunken border */
static void draw_sunken_box(DuiConnection *conn, DuiWindow win, int x, int y, int w, int h) {
    dui_fill_rect(conn, win, x, y, w, h, COLOR_FRAME_BG);
    dui_draw_line(conn, win, x, y, x + w - 1, y, COLOR_BEVEL_DARK);
    dui_draw_line(conn, win, x, y, x, y + h - 1, COLOR_BEVEL_DARK);
    dui_draw_line(conn, win, x, y + h - 1, x + w - 1, y + h - 1, COLOR_BEVEL_LIGHT);
    dui_draw_line(conn, win, x + w - 1, y, x + w - 1, y + h - 1, COLOR_BEVEL_LIGHT);
}

static void gui_render(DuiConnection *conn, DuiWindow win) {
    if (g_state == STATE_PLAYING && g_start_time > 0) {
        g_elapsed_sec = (int)(time(NULL) - g_start_time);
    }

    dui_clear(conn, win, COLOR_FRAME_BG);

    /* Outer Window Bevel Border */
    dui_draw_line(conn, win, 0, 0, WIN_W - 1, 0, COLOR_BEVEL_LIGHT);
    dui_draw_line(conn, win, 0, 0, 0, WIN_H - 1, COLOR_BEVEL_LIGHT);
    dui_draw_line(conn, win, 0, WIN_H - 1, WIN_W - 1, WIN_H - 1, COLOR_BEVEL_DARK);
    dui_draw_line(conn, win, WIN_W - 1, 0, WIN_W - 1, WIN_H - 1, COLOR_BEVEL_DARK);

    /* Top Status Bar: Sunken Box */
    draw_sunken_box(conn, win, 15, 12, WIN_W - 30, 44);

    /* Mine Counter Display (Left) */
    dui_fill_rect(conn, win, 25, 18, 54, 30, COLOR_DIGIT_BG);
    char mines_str[16];
    int remaining = TOTAL_MINES - g_flags_count;
    if (remaining < 0) remaining = 0;
    snprintf(mines_str, sizeof(mines_str), "%03d", remaining);
    dui_draw_text(conn, win, 34, 25, mines_str, COLOR_RED_DIGIT);

    /* Timer Display (Right) */
    dui_fill_rect(conn, win, WIN_W - 25 - 54, 18, 54, 30, COLOR_DIGIT_BG);
    char time_str[16];
    int t = g_elapsed_sec > 999 ? 999 : g_elapsed_sec;
    snprintf(time_str, sizeof(time_str), "%03d", t);
    dui_draw_text(conn, win, WIN_W - 25 - 54 + 9, 25, time_str, COLOR_RED_DIGIT);

    /* Smiley Face Button in Center (32x32 at x=144, y=17) */
    draw_raised_box(conn, win, 144, 17, 32, 32);
    dui_fill_circle(conn, win, 160, 33, 11, COLOR_FACE_YELLOW);
    dui_draw_circle(conn, win, 160, 33, 11, COLOR_BEVEL_BLACK);

    if (g_state == STATE_LOST) {
        /* X eyes */
        dui_draw_text(conn, win, 154, 26, "X", COLOR_BEVEL_BLACK);
        dui_draw_text(conn, win, 162, 26, "X", COLOR_BEVEL_BLACK);
        /* Sad mouth */
        dui_draw_line(conn, win, 155, 38, 165, 38, COLOR_BEVEL_BLACK);
    } else if (g_state == STATE_WON) {
        /* Sunglasses */
        dui_fill_rect(conn, win, 153, 28, 6, 4, COLOR_BEVEL_BLACK);
        dui_fill_rect(conn, win, 161, 28, 6, 4, COLOR_BEVEL_BLACK);
        dui_draw_line(conn, win, 159, 29, 161, 29, COLOR_BEVEL_BLACK);
        /* Big smile */
        dui_draw_line(conn, win, 155, 37, 165, 37, COLOR_BEVEL_BLACK);
        dui_draw_line(conn, win, 157, 38, 163, 38, COLOR_BEVEL_BLACK);
    } else {
        /* Normal Eyes */
        dui_fill_rect(conn, win, 155, 27, 2, 3, COLOR_BEVEL_BLACK);
        dui_fill_rect(conn, win, 163, 27, 2, 3, COLOR_BEVEL_BLACK);
        /* Happy smile */
        dui_draw_line(conn, win, 156, 37, 164, 37, COLOR_BEVEL_BLACK);
        dui_set_pixel(conn, win, 155, 36, COLOR_BEVEL_BLACK);
        dui_set_pixel(conn, win, 165, 36, COLOR_BEVEL_BLACK);
    }

    /* Playing Field: Sunken Enclosure */
    draw_sunken_box(conn, win, BOARD_OFFSET_X - 3, BOARD_OFFSET_Y - 3,
                    COLS * CELL_SZ + 6, ROWS * CELL_SZ + 6);

    /* Fill board background */
    dui_fill_rect(conn, win, BOARD_OFFSET_X, BOARD_OFFSET_Y, COLS * CELL_SZ, ROWS * CELL_SZ, COLOR_FRAME_BG);

    /* Grid lines */
    for (int c = 0; c <= COLS; c++) {
        int gx = BOARD_OFFSET_X + c * CELL_SZ;
        dui_draw_line(conn, win, gx, BOARD_OFFSET_Y, gx, BOARD_OFFSET_Y + ROWS * CELL_SZ, COLOR_BEVEL_DARK);
    }
    for (int r = 0; r <= ROWS; r++) {
        int gy = BOARD_OFFSET_Y + r * CELL_SZ;
        dui_draw_line(conn, win, BOARD_OFFSET_X, gy, BOARD_OFFSET_X + COLS * CELL_SZ, gy, COLOR_BEVEL_DARK);
    }

    /* Render Cells */
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            int cx = BOARD_OFFSET_X + c * CELL_SZ;
            int cy = BOARD_OFFSET_Y + r * CELL_SZ;

            if (!g_board[r][c].is_revealed) {
                /* 3D bevel highlight on unrevealed tile */
                dui_draw_line(conn, win, cx + 1, cy + 1, cx + CELL_SZ - 2, cy + 1, COLOR_BEVEL_LIGHT);
                dui_draw_line(conn, win, cx + 1, cy + 1, cx + 1, cy + CELL_SZ - 2, COLOR_BEVEL_LIGHT);

                if (g_board[r][c].is_flagged) {
                    /* Red Flag */
                    dui_fill_rect(conn, win, cx + 10, cy + 8, 8, 7, 0x00FF0000);
                    dui_draw_line(conn, win, cx + 18, cy + 8, cx + 18, cy + 22, COLOR_BEVEL_BLACK);
                    dui_draw_line(conn, win, cx + 14, cy + 22, cx + 22, cy + 22, COLOR_BEVEL_BLACK);
                }
            } else if (g_board[r][c].is_mine) {
                if (r == g_exploded_r && c == g_exploded_c) {
                    dui_fill_rect(conn, win, cx + 1, cy + 1, CELL_SZ - 1, CELL_SZ - 1, 0x00E74C3C);
                }
                dui_fill_circle(conn, win, cx + CELL_SZ / 2, cy + CELL_SZ / 2, 7, COLOR_BEVEL_BLACK);
                dui_draw_line(conn, win, cx + 4, cy + CELL_SZ / 2, cx + CELL_SZ - 5, cy + CELL_SZ / 2, COLOR_BEVEL_BLACK);
                dui_draw_line(conn, win, cx + CELL_SZ / 2, cy + 4, cx + CELL_SZ / 2, cy + CELL_SZ - 5, COLOR_BEVEL_BLACK);
            } else {
                int n = g_board[r][c].neighbors;
                if (n > 0) {
                    char digit_str[4];
                    snprintf(digit_str, sizeof(digit_str), "%d", n);
                    dui_draw_text(conn, win, cx + 11, cy + 7, digit_str, num_gui_colors[n]);
                }
            }
        }
    }

    dui_flush(conn, win);
}

static void handle_click(int mx, int my, int button) {
    /* Check Smiley face click (x: 144..176, y: 17..49) */
    if (mx >= 144 && mx <= 176 && my >= 17 && my <= 49) {
        init_game();
        return;
    }

    /* Check Board click */
    if (mx >= BOARD_OFFSET_X && mx < BOARD_OFFSET_X + COLS * CELL_SZ &&
        my >= BOARD_OFFSET_Y && my < BOARD_OFFSET_Y + ROWS * CELL_SZ) {
        int c = (mx - BOARD_OFFSET_X) / CELL_SZ;
        int r = (my - BOARD_OFFSET_Y) / CELL_SZ;

        if (button == 1) {
            /* Left Click: Reveal */
            reveal_cell(r, c);
        } else if (button == 3) {
            /* Right Click: Flag */
            toggle_flag(r, c);
        }
    }
}

static void run_gui(DuiConnection *conn) {
    printf("[minesweeper] Entered run_gui, creating window...\n");
    DuiWindow win = dui_create_window(conn, 480, 55, WIN_W, WIN_H,
                                      "DUnix Minesweeper", COLOR_FRAME_BG, DWS_WIN_DECORATED);
    printf("[minesweeper] win=%u, showing window...\n", (unsigned int)win);
    dui_show(conn, win);
    printf("[minesweeper] init_game...\n");
    init_game();
    printf("[minesweeper] gui_render...\n");
    gui_render(conn, win);
    printf("[minesweeper] entering event loop...\n");

    DuiEvent ev;
    while (1) {
        if (dui_next_event(conn, &ev) > 0) {
            if (ev.type == DWS_EV_CLOSE_REQ && ev.window == win) {
                break;
            } else if (ev.type == DWS_EV_EXPOSE && ev.window == win) {
                gui_render(conn, win);
            } else if (ev.type == DWS_EV_MOUSE_DOWN && ev.window == win) {
                handle_click(ev.mouse.x, ev.mouse.y, ev.mouse.button);
                gui_render(conn, win);
            } else if (ev.type == DWS_EV_KEY_DOWN && ev.window == win) {
                char ch = ev.key.ch;
                if (ch == 'r' || ch == 'R') {
                    init_game();
                    gui_render(conn, win);
                } else if (ch == 'q' || ch == 'Q') {
                    break;
                }
            }
        }
    }

    dui_destroy_window(conn, win);
    dui_disconnect(conn);
}

/* ========================================================================= */
/* Entry Point & Mode Auto-Detection                                         */
/* ========================================================================= */

int main(int argc, char **argv) {
    srand((unsigned int)time(NULL));

    int force_tui = 0;
    int force_gui = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tui") == 0 || strcmp(argv[i], "-t") == 0) {
            force_tui = 1;
        } else if (strcmp(argv[i], "--gui") == 0 || strcmp(argv[i], "-g") == 0) {
            force_gui = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: minesweeper [OPTIONS]\n");
            printf("Classic Minesweeper puzzle game with both Terminal (TUI) and DWS Window (GUI) interfaces.\n\n");
            printf("Options:\n");
            printf("  -t, --tui    Force text-mode terminal interface\n");
            printf("  -g, --gui    Force graphical DWS window interface\n");
            printf("  -h, --help   Display this help message and exit\n\n");
            printf("By default, minesweeper detects if DWS desktop is running and opens the GUI window,\n");
            printf("otherwise falling back seamlessly to interactive TUI mode.\n");
            return 0;
        }
    }

    if (force_tui) {
        run_tui();
        return 0;
    }

    printf("[minesweeper] Calling dui_connect()...\n");
    DuiConnection *conn = dui_connect();
    printf("[minesweeper] conn = %p\n", conn);
    if (conn) {
        if (!force_tui) {
            run_gui(conn);
            return 0;
        }
        dui_disconnect(conn);
    }

    if (force_gui) {
        fprintf(stderr, "minesweeper: cannot connect to DWS graphical display server.\n");
        fprintf(stderr, "-> Start DWS with '/bin/dws' or run in text mode: 'minesweeper -t'\n");
        return 1;
    }

    run_tui();
    return 0;
}
