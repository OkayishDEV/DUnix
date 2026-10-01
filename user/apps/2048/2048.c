/*
 * DUnix 2048 - Dual TUI and GUI Classic Sliding Tile Game
 * Supports both full-color interactive Terminal (TUI) and DWS Window (GUI)
 * in the exact same binary.
 *
 * Usage:
 *   2048          (Auto-detects DWS GUI, falls back to TUI if in terminal)
 *   2048 --tui    (Forces text-mode terminal interface)
 *   2048 --gui    (Forces DWS graphical window interface)
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

#define SIZE 4

#define MOVE_UP    0
#define MOVE_RIGHT 1
#define MOVE_DOWN  2
#define MOVE_LEFT  3

static int g_board[SIZE][SIZE];
static int g_score = 0;
static int g_best_score = 0;
static int g_game_over = 0;
static int g_won = 0;

static void spawn_tile(void) {
    int empty[SIZE * SIZE][2];
    int count = 0;

    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (g_board[r][c] == 0) {
                empty[count][0] = r;
                empty[count][1] = c;
                count++;
            }
        }
    }

    if (count > 0) {
        int idx = rand() % count;
        int val = (rand() % 10 == 0) ? 4 : 2;
        g_board[empty[idx][0]][empty[idx][1]] = val;
    }
}

static void init_game(void) {
    memset(g_board, 0, sizeof(g_board));
    g_score = 0;
    g_game_over = 0;
    g_won = 0;
    spawn_tile();
    spawn_tile();
}

static int can_move(void) {
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (g_board[r][c] == 0) return 1;
            if (r + 1 < SIZE && g_board[r][c] == g_board[r + 1][c]) return 1;
            if (c + 1 < SIZE && g_board[r][c] == g_board[r][c + 1]) return 1;
        }
    }
    return 0;
}

static void rotate_board(void) {
    int temp[SIZE][SIZE];
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            temp[c][SIZE - 1 - r] = g_board[r][c];
        }
    }
    memcpy(g_board, temp, sizeof(g_board));
}

static int slide_left_row(int row[SIZE]) {
    int changed = 0;
    int target[SIZE];
    int idx = 0;
    memset(target, 0, sizeof(target));

    /* Compact non-zero tiles */
    for (int i = 0; i < SIZE; i++) {
        if (row[i] != 0) {
            target[idx++] = row[i];
        }
    }

    /* Merge adjacent equal tiles */
    for (int i = 0; i < SIZE - 1; i++) {
        if (target[i] != 0 && target[i] == target[i + 1]) {
            target[i] *= 2;
            g_score += target[i];
            if (target[i] == 2048) g_won = 1;
            target[i + 1] = 0;
            i++;
        }
    }

    /* Compact again after merging */
    int final_row[SIZE];
    memset(final_row, 0, sizeof(final_row));
    idx = 0;
    for (int i = 0; i < SIZE; i++) {
        if (target[i] != 0) {
            final_row[idx++] = target[i];
        }
    }

    /* Check if anything changed */
    for (int i = 0; i < SIZE; i++) {
        if (row[i] != final_row[i]) {
            changed = 1;
            row[i] = final_row[i];
        }
    }

    return changed;
}

static int move(int dir) {
    if (g_game_over) return 0;

    int rotations = 0;
    if (dir == MOVE_UP)    rotations = 3;
    if (dir == MOVE_RIGHT) rotations = 2;
    if (dir == MOVE_DOWN)  rotations = 1;

    for (int i = 0; i < rotations; i++) rotate_board();

    int moved = 0;
    for (int r = 0; r < SIZE; r++) {
        if (slide_left_row(g_board[r])) moved = 1;
    }

    int restore = (4 - rotations) % 4;
    for (int i = 0; i < restore; i++) rotate_board();

    if (moved) {
        if (g_score > g_best_score) g_best_score = g_score;
        spawn_tile();
        if (!can_move()) {
            g_game_over = 1;
        }
    }

    return moved;
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

static const char *get_tui_color(int val) {
    switch (val) {
        case 2:    return "\033[1;30;47m"; /* Black on White */
        case 4:    return "\033[1;30;43m"; /* Black on Yellow */
        case 8:    return "\033[1;37;41m"; /* White on Red */
        case 16:   return "\033[1;37;45m"; /* White on Magenta */
        case 32:   return "\033[1;37;44m"; /* White on Blue */
        case 64:   return "\033[1;37;46m"; /* White on Cyan */
        case 128:  return "\033[1;33;42m"; /* Yellow on Green */
        case 256:  return "\033[1;37;42m"; /* White on Green */
        case 512:  return "\033[1;37;43m"; /* White on Yellow */
        case 1024: return "\033[1;31;47m"; /* Red on White */
        case 2048: return "\033[1;33;45m"; /* Yellow on Magenta */
        default:   return "\033[1;37;40m"; /* White on Grey */
    }
}

static void tui_render(void) {
    printf("\033[2J\033[H");

    /* Header */
    printf("\033[1;36m+-------------------------------------------+\033[0m\n");
    printf("\033[1;36m|\033[1;37m   DUnix 2048 Puzzle (TUI)                 \033[1;36m|\033[0m\n");
    printf("\033[1;36m|\033[0m  Score: \033[1;32m%-6d\033[0m      Best: \033[1;33m%-6d\033[0m       \033[1;36m|\033[0m\n",
           g_score, g_best_score);
    printf("\033[1;36m+-------------------------------------------+\033[0m\n\n");

    /* Board */
    printf("   +-------+-------+-------+-------+\n");
    for (int r = 0; r < SIZE; r++) {
        /* Upper padding line */
        printf("   |");
        for (int c = 0; c < SIZE; c++) {
            if (g_board[r][c] == 0) {
                printf("       |");
            } else {
                printf("%s       \033[0m|", get_tui_color(g_board[r][c]));
            }
        }
        printf("\n");

        /* Center number line */
        printf("   |");
        for (int c = 0; c < SIZE; c++) {
            if (g_board[r][c] == 0) {
                printf("   .   |");
            } else {
                printf("%s %5d \033[0m|", get_tui_color(g_board[r][c]), g_board[r][c]);
            }
        }
        printf("\n");

        /* Lower padding line */
        printf("   |");
        for (int c = 0; c < SIZE; c++) {
            if (g_board[r][c] == 0) {
                printf("       |");
            } else {
                printf("%s       \033[0m|", get_tui_color(g_board[r][c]));
            }
        }
        printf("\n");
        printf("   +-------+-------+-------+-------+\n");
    }

    printf("\n");
    if (g_game_over) {
        printf("\033[1;31m   *** GAME OVER! Final Score: %d ***      \033[0m\n", g_score);
        printf("\033[1;33m   Press [R] to Restart  |  [Q] to Quit     \033[0m\n");
    } else if (g_won) {
        printf("\033[1;32m   *** YOU WIN! (Reached 2048!) ***         \033[0m\n");
        printf("   [WASD / Arrows] Keep Playing  [R] Restart\n");
    } else {
        printf("   [W/A/S/D / Arrows] Slide  [R] Restart    \n");
        printf("   [Q] Quit                                 \n");
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

        if (poll(&pfd, 1, -1) > 0 && (pfd.revents & POLLIN)) {
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
                                if (seq[1] == 'A') move(MOVE_UP);
                                else if (seq[1] == 'B') move(MOVE_DOWN);
                                else if (seq[1] == 'C') move(MOVE_RIGHT);
                                else if (seq[1] == 'D') move(MOVE_LEFT);
                            }
                        }
                    } else {
                        break; /* Standalone ESC quits */
                    }
                } else if (ch == 'w' || ch == 'W') {
                    move(MOVE_UP);
                } else if (ch == 's' || ch == 'S') {
                    move(MOVE_DOWN);
                } else if (ch == 'a' || ch == 'A') {
                    move(MOVE_LEFT);
                } else if (ch == 'd' || ch == 'D') {
                    move(MOVE_RIGHT);
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

#define WIN_W 380
#define WIN_H 480
#define BOARD_X 20
#define BOARD_Y 110
#define BOARD_SIZE 340
#define TILE_SZ 72
#define TILE_GAP 10

#define GUI_BG           0x00FAF8EF
#define GUI_BOARD_BG     0x00BBADA0
#define GUI_EMPTY_TILE   0x00CDC1B4
#define GUI_TEXT_DARK    0x00776E65
#define GUI_TEXT_LIGHT   0x00F9F6F2
#define GUI_SCORE_BOX    0x00BBADA0
#define GUI_BUTTON_BG    0x008F7A66

static uint32_t get_tile_bg(int val) {
    switch (val) {
        case 2:    return 0x00EEE4DA;
        case 4:    return 0x00EDE0C8;
        case 8:    return 0x00F2B179;
        case 16:   return 0x00F59563;
        case 32:   return 0x00F67C5F;
        case 64:   return 0x00F65E3B;
        case 128:  return 0x00EDCF72;
        case 256:  return 0x00EDCC61;
        case 512:  return 0x00EDC850;
        case 1024: return 0x00EDC53F;
        case 2048: return 0x00EDC22E;
        default:   return 0x003C3A32;
    }
}

static uint32_t get_tile_fg(int val) {
    if (val <= 4) return GUI_TEXT_DARK;
    return GUI_TEXT_LIGHT;
}

static void gui_render(DuiConnection *conn, DuiWindow win) {
    dui_clear(conn, win, GUI_BG);

    /* Logo "2048" */
    dui_draw_text(conn, win, 20, 25, "2048", GUI_TEXT_DARK);
    dui_draw_text(conn, win, 21, 25, "2048", GUI_TEXT_DARK); /* Bold effect */

    /* Score Box */
    dui_fill_rect(conn, win, 180, 15, 80, 48, GUI_SCORE_BOX);
    dui_draw_text(conn, win, 198, 20, "SCORE", 0x00EEE4DA);
    char s_str[16];
    snprintf(s_str, sizeof(s_str), "%d", g_score);
    int sw = (int)strlen(s_str) * 8;
    dui_draw_text(conn, win, 180 + (80 - sw) / 2, 38, s_str, GUI_TEXT_LIGHT);

    /* Best Box */
    dui_fill_rect(conn, win, 275, 15, 80, 48, GUI_SCORE_BOX);
    dui_draw_text(conn, win, 298, 20, "BEST", 0x00EEE4DA);
    char b_str[16];
    snprintf(b_str, sizeof(b_str), "%d", g_best_score);
    int bw = (int)strlen(b_str) * 8;
    dui_draw_text(conn, win, 275 + (80 - bw) / 2, 38, b_str, GUI_TEXT_LIGHT);

    /* Subtitle & New Game Button */
    dui_draw_text(conn, win, 20, 78, "Join numbers to get 2048!", GUI_TEXT_DARK);

    dui_fill_rect(conn, win, 260, 72, 95, 28, GUI_BUTTON_BG);
    dui_draw_text(conn, win, 274, 78, "New Game", GUI_TEXT_LIGHT);

    /* Board Background Container */
    dui_fill_rect(conn, win, BOARD_X, BOARD_Y, BOARD_SIZE, BOARD_SIZE, GUI_BOARD_BG);

    /* Render 4x4 Tiles */
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            int tx = BOARD_X + TILE_GAP + c * (TILE_SZ + TILE_GAP);
            int ty = BOARD_Y + TILE_GAP + r * (TILE_SZ + TILE_GAP);

            int val = g_board[r][c];
            if (val == 0) {
                dui_fill_rect(conn, win, tx, ty, TILE_SZ, TILE_SZ, GUI_EMPTY_TILE);
            } else {
                dui_fill_rect(conn, win, tx, ty, TILE_SZ, TILE_SZ, get_tile_bg(val));
                char num_str[16];
                snprintf(num_str, sizeof(num_str), "%d", val);
                int nw = (int)strlen(num_str) * 8;
                dui_draw_text(conn, win, tx + (TILE_SZ - nw) / 2, ty + (TILE_SZ - 16) / 2,
                              num_str, get_tile_fg(val));
            }
        }
    }

    /* Game Over Overlay */
    if (g_game_over) {
        dui_fill_rect(conn, win, BOARD_X + 20, BOARD_Y + 100, BOARD_SIZE - 40, 140, 0x00EEE4DA);
        dui_draw_rect(conn, win, BOARD_X + 20, BOARD_Y + 100, BOARD_SIZE - 40, 140, GUI_TEXT_DARK);

        dui_draw_text(conn, win, BOARD_X + 110, BOARD_Y + 130, "Game Over!", 0x00E74C3C);
        char fin_str[32];
        snprintf(fin_str, sizeof(fin_str), "Score: %d", g_score);
        dui_draw_text(conn, win, BOARD_X + 120, BOARD_Y + 160, fin_str, GUI_TEXT_DARK);

        dui_draw_text(conn, win, BOARD_X + 65, BOARD_Y + 200, "Press R or Click New Game", GUI_BUTTON_BG);
    }

    dui_flush(conn, win);
}

static void run_gui(DuiConnection *conn) {
    DuiWindow win = dui_create_window(conn, 620, 240, WIN_W, WIN_H,
                                      "DUnix 2048", GUI_BG, DWS_WIN_DECORATED);
    dui_show(conn, win);
    init_game();
    gui_render(conn, win);

    DuiEvent ev;
    while (1) {
        if (dui_next_event(conn, &ev) > 0) {
            if (ev.type == DWS_EV_CLOSE_REQ && ev.window == win) {
                break;
            } else if (ev.type == DWS_EV_EXPOSE && ev.window == win) {
                gui_render(conn, win);
            } else if (ev.type == DWS_EV_MOUSE_DOWN && ev.window == win) {
                int mx = ev.mouse.x;
                int my = ev.mouse.y;
                /* Check New Game button (x: 260..355, y: 72..100) */
                if (mx >= 260 && mx <= 355 && my >= 72 && my <= 100) {
                    init_game();
                    gui_render(conn, win);
                }
            } else if (ev.type == DWS_EV_KEY_DOWN && ev.window == win) {
                char ch = ev.key.ch;
                uint32_t k = ev.key.keycode;

                if (ch == 'w' || ch == 'W' || k == 0x11 /* Up */)    move(MOVE_UP);
                else if (ch == 's' || ch == 'S' || k == 0x12 /* Down */)  move(MOVE_DOWN);
                else if (ch == 'a' || ch == 'A' || k == 0x13 /* Left */)  move(MOVE_LEFT);
                else if (ch == 'd' || ch == 'D' || k == 0x14 /* Right */) move(MOVE_RIGHT);
                else if (ch == 'r' || ch == 'R') { init_game(); }
                else if (ch == 'q' || ch == 'Q') { break; }

                gui_render(conn, win);
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
            printf("Usage: 2048 [OPTIONS]\n");
            printf("Classic 2048 sliding tile puzzle with both Terminal (TUI) and DWS Window (GUI) interfaces.\n\n");
            printf("Options:\n");
            printf("  -t, --tui    Force text-mode terminal interface\n");
            printf("  -g, --gui    Force graphical DWS window interface\n");
            printf("  -h, --help   Display this help message and exit\n\n");
            printf("By default, 2048 detects if DWS desktop is running and opens the GUI window,\n");
            printf("otherwise falling back seamlessly to interactive TUI mode.\n");
            return 0;
        }
    }

    if (force_tui) {
        run_tui();
        return 0;
    }

    DuiConnection *conn = dui_connect();
    if (conn) {
        if (!force_tui) {
            run_gui(conn);
            return 0;
        }
        dui_disconnect(conn);
    }

    if (force_gui) {
        fprintf(stderr, "2048: cannot connect to DWS graphical display server.\n");
        fprintf(stderr, "-> Start DWS with '/bin/dws' or run in text mode: '2048 -t'\n");
        return 1;
    }

    run_tui();
    return 0;
}
