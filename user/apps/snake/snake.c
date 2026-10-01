/*
 * DUnix Snake - Dual TUI and GUI Classic Arcade Game
 * Supports both full-color interactive Terminal (TUI) and DWS Window (GUI)
 * in the exact same binary.
 *
 * Usage:
 *   snake          (Auto-detects DWS GUI, falls back to TUI if in terminal)
 *   snake --tui    (Forces text-mode terminal interface)
 *   snake --gui    (Forces DWS graphical window interface)
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

#define GRID_W 20
#define GRID_H 20
#define MAX_SNAKE (GRID_W * GRID_H)

#define DIR_UP    0
#define DIR_RIGHT 1
#define DIR_DOWN  2
#define DIR_LEFT  3

typedef struct {
    int x;
    int y;
} Point;

/* Game State */
static Point g_snake[MAX_SNAKE];
static int   g_snake_len = 3;
static int   g_dir = DIR_RIGHT;
static int   g_next_dir = DIR_RIGHT;
static Point g_food;
static int   g_score = 0;
static int   g_high_score = 0;
static int   g_game_over = 0;
static int   g_paused = 0;
static int   g_speed_ms = 140;

static void spawn_food(void) {
    int empty_found = 0;
    while (!empty_found) {
        g_food.x = rand() % GRID_W;
        g_food.y = rand() % GRID_H;
        empty_found = 1;
        for (int i = 0; i < g_snake_len; i++) {
            if (g_snake[i].x == g_food.x && g_snake[i].y == g_food.y) {
                empty_found = 0;
                break;
            }
        }
    }
}

static void init_game(void) {
    g_snake_len = 3;
    g_dir = DIR_RIGHT;
    g_next_dir = DIR_RIGHT;
    g_score = 0;
    g_game_over = 0;
    g_paused = 0;
    g_speed_ms = 140;

    int start_x = GRID_W / 2;
    int start_y = GRID_H / 2;
    for (int i = 0; i < g_snake_len; i++) {
        g_snake[i].x = start_x - i;
        g_snake[i].y = start_y;
    }
    spawn_food();
}

static void change_dir(int new_dir) {
    if ((g_dir == DIR_UP && new_dir == DIR_DOWN) ||
        (g_dir == DIR_DOWN && new_dir == DIR_UP) ||
        (g_dir == DIR_LEFT && new_dir == DIR_RIGHT) ||
        (g_dir == DIR_RIGHT && new_dir == DIR_LEFT)) {
        return; /* No immediate 180-degree turn */
    }
    g_next_dir = new_dir;
}

static void update_game(void) {
    if (g_game_over || g_paused) return;

    g_dir = g_next_dir;
    Point head = g_snake[0];

    if (g_dir == DIR_UP)    head.y--;
    if (g_dir == DIR_DOWN)  head.y++;
    if (g_dir == DIR_LEFT)  head.x--;
    if (g_dir == DIR_RIGHT) head.x++;

    /* Wall collision */
    if (head.x < 0 || head.x >= GRID_W || head.y < 0 || head.y >= GRID_H) {
        g_game_over = 1;
        if (g_score > g_high_score) g_high_score = g_score;
        return;
    }

    /* Self collision */
    for (int i = 0; i < g_snake_len - 1; i++) {
        if (g_snake[i].x == head.x && g_snake[i].y == head.y) {
            g_game_over = 1;
            if (g_score > g_high_score) g_high_score = g_score;
            return;
        }
    }

    /* Check food eating */
    int ate = (head.x == g_food.x && head.y == g_food.y);
    if (ate) {
        g_score += 10;
        if (g_score > g_high_score) g_high_score = g_score;
        if (g_snake_len < MAX_SNAKE) g_snake_len++;
        if (g_speed_ms > 60) g_speed_ms -= 3;
        spawn_food();
    }

    /* Shift snake body */
    for (int i = g_snake_len - 1; i > 0; i--) {
        g_snake[i] = g_snake[i - 1];
    }
    g_snake[0] = head;
}

/* ========================================================================= */
/* TUI (Terminal User Interface) Engine                                      */
/* ========================================================================= */

static struct termios g_orig_termios;
static int g_tui_active = 0;

static void tui_restore_terminal(void) {
    if (g_tui_active) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
        printf("\033[?25h\033[0m\n"); /* Show cursor, reset colors */
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
    printf("\033[?25l"); /* Hide cursor */
    fflush(stdout);
}

static void tui_render(void) {
    /* Clear screen and move cursor home */
    printf("\033[2J\033[H");

    /* Header banner */
    printf("\033[1;36m+------------------------------------------+\033[0m\n");
    printf("\033[1;36m|\033[1;37m   DUnix Snake (TUI)                      \033[1;36m|\033[0m\n");
    printf("\033[1;36m|\033[0m  Score: \033[1;32m%-4d\033[0m  High: \033[1;33m%-4d\033[0m  Len: \033[1;35m%-3d\033[0m  \033[1;36m|\033[0m\n",
           g_score, g_high_score, g_snake_len);
    printf("\033[1;36m+------------------------------------------+\033[0m\n");

    /* Top board border */
    printf("\033[1;34m+-");
    for (int x = 0; x < GRID_W; x++) printf("--");
    printf("-+\033[0m\n");

    /* Board rows */
    for (int y = 0; y < GRID_H; y++) {
        printf("\033[1;34m| \033[0m");
        for (int x = 0; x < GRID_W; x++) {
            if (x == g_snake[0].x && y == g_snake[0].y) {
                printf("\033[1;33m@@\033[0m"); /* Head */
            } else if (x == g_food.x && y == g_food.y) {
                printf("\033[1;31m<>\033[0m"); /* Food */
            } else {
                int is_body = 0;
                for (int i = 1; i < g_snake_len; i++) {
                    if (g_snake[i].x == x && g_snake[i].y == y) {
                        is_body = 1;
                        break;
                    }
                }
                if (is_body) {
                    printf("\033[1;32m[]\033[0m"); /* Body segment */
                } else {
                    printf("  "); /* Empty cell */
                }
            }
        }
        printf("\033[1;34m |\033[0m\n");
    }

    /* Bottom board border */
    printf("\033[1;34m+-");
    for (int x = 0; x < GRID_W; x++) printf("--");
    printf("-+\033[0m\n");

    /* Controls and status */
    if (g_game_over) {
        printf("\033[1;31m  *** GAME OVER! *** (Score: %d)            \033[0m\n", g_score);
        printf("\033[1;33m  Press [R] to Restart  |  [Q] to Quit      \033[0m\n");
    } else if (g_paused) {
        printf("\033[1;33m  *** PAUSED *** Press [P] to Resume       \033[0m\n");
        printf("  [W/A/S/D / Arrows] Move  [Q] Quit        \n");
    } else {
        printf("  [W/A/S/D / Arrows] Move  [P] Pause       \n");
        printf("  [R] Restart              [Q] Quit        \n");
    }
    fflush(stdout);
}

static void run_tui(void) {
    tui_init_terminal();
    init_game();

    while (1) {
        tui_render();

        /* Poll stdin for key input with game tick timeout */
        struct pollfd pfd;
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;
        pfd.revents = 0;

        int poll_res = poll(&pfd, 1, g_speed_ms);
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
                                if (seq[1] == 'A') change_dir(DIR_UP);
                                else if (seq[1] == 'B') change_dir(DIR_DOWN);
                                else if (seq[1] == 'C') change_dir(DIR_RIGHT);
                                else if (seq[1] == 'D') change_dir(DIR_LEFT);
                            }
                        }
                    } else {
                        break; /* Standalone ESC quits */
                    }
                } else if (ch == 'w' || ch == 'W') {
                    change_dir(DIR_UP);
                } else if (ch == 's' || ch == 'S') {
                    change_dir(DIR_DOWN);
                } else if (ch == 'a' || ch == 'A') {
                    change_dir(DIR_LEFT);
                } else if (ch == 'd' || ch == 'D') {
                    change_dir(DIR_RIGHT);
                } else if (ch == 'p' || ch == 'P') {
                    g_paused = !g_paused;
                } else if (ch == 'r' || ch == 'R') {
                    init_game();
                } else if (ch == 'q' || ch == 'Q') {
                    break;
                }
            }
        }

        update_game();
    }

    tui_restore_terminal();
}

/* ========================================================================= */
/* GUI (DWS Desktop Window Interface) Engine                                 */
/* ========================================================================= */

#define WIN_W 440
#define WIN_H 490
#define CELL_SZ 20
#define BOARD_OFFSET_X 20
#define BOARD_OFFSET_Y 60

#define GUI_BG_COLOR       0x001B2631
#define GUI_PANEL_BG       0x002C3E50
#define GUI_GRID_BG        0x00151E28
#define GUI_GRID_BORDER    0x0034495E
#define GUI_SNAKE_HEAD     0x0082E0AA
#define GUI_SNAKE_BODY     0x002ECC71
#define GUI_SNAKE_BORDER   0x0027AE60
#define GUI_FOOD_APPLE     0x00E74C3C
#define GUI_FOOD_LEAF      0x002ECC71
#define GUI_TEXT_WHITE     0x00ECF0F1
#define GUI_TEXT_SCORE     0x002ECC71
#define GUI_TEXT_HIGH      0x00F1C40F
#define GUI_TEXT_RED       0x00E74C3C

static void gui_render(DuiConnection *conn, DuiWindow win) {
    dui_clear(conn, win, GUI_BG_COLOR);

    /* Top score panel */
    dui_fill_rect(conn, win, 10, 10, WIN_W - 20, 40, GUI_PANEL_BG);
    dui_draw_rect(conn, win, 10, 10, WIN_W - 20, 40, GUI_GRID_BORDER);

    char score_str[64];
    snprintf(score_str, sizeof(score_str), "SCORE: %d", g_score);
    dui_draw_text(conn, win, 20, 24, score_str, GUI_TEXT_SCORE);

    char high_str[64];
    snprintf(high_str, sizeof(high_str), "HIGH: %d", g_high_score);
    dui_draw_text(conn, win, 170, 24, high_str, GUI_TEXT_HIGH);

    if (g_paused) {
        dui_draw_text(conn, win, 320, 24, "[PAUSED]", 0x00E67E22);
    } else {
        char len_str[32];
        snprintf(len_str, sizeof(len_str), "LEN: %d", g_snake_len);
        dui_draw_text(conn, win, 320, 24, len_str, GUI_TEXT_WHITE);
    }

    /* Board Background & Outer Border */
    dui_fill_rect(conn, win, BOARD_OFFSET_X, BOARD_OFFSET_Y, GRID_W * CELL_SZ, GRID_H * CELL_SZ, GUI_GRID_BG);
    dui_draw_rect(conn, win, BOARD_OFFSET_X - 1, BOARD_OFFSET_Y - 1,
                  GRID_W * CELL_SZ + 2, GRID_H * CELL_SZ + 2, GUI_GRID_BORDER);

    /* Draw Food (Apple) */
    int fx = BOARD_OFFSET_X + g_food.x * CELL_SZ;
    int fy = BOARD_OFFSET_Y + g_food.y * CELL_SZ;
    dui_fill_circle(conn, win, fx + CELL_SZ / 2, fy + CELL_SZ / 2 + 1, CELL_SZ / 2 - 2, GUI_FOOD_APPLE);
    /* Leaf */
    dui_draw_line(conn, win, fx + CELL_SZ / 2, fy + 2, fx + CELL_SZ / 2 + 3, fy - 1, GUI_FOOD_LEAF);

    /* Draw Snake Body */
    for (int i = g_snake_len - 1; i > 0; i--) {
        int bx = BOARD_OFFSET_X + g_snake[i].x * CELL_SZ;
        int by = BOARD_OFFSET_Y + g_snake[i].y * CELL_SZ;
        dui_fill_rect(conn, win, bx + 1, by + 1, CELL_SZ - 2, CELL_SZ - 2, GUI_SNAKE_BODY);
        dui_draw_rect(conn, win, bx + 1, by + 1, CELL_SZ - 2, CELL_SZ - 2, GUI_SNAKE_BORDER);
    }

    /* Draw Snake Head */
    int hx = BOARD_OFFSET_X + g_snake[0].x * CELL_SZ;
    int hy = BOARD_OFFSET_Y + g_snake[0].y * CELL_SZ;
    dui_fill_rect(conn, win, hx + 1, hy + 1, CELL_SZ - 2, CELL_SZ - 2, GUI_SNAKE_HEAD);
    dui_draw_rect(conn, win, hx + 1, hy + 1, CELL_SZ - 2, CELL_SZ - 2, GUI_SNAKE_BORDER);

    /* Snake Eyes based on direction */
    int e1x = hx + 5, e1y = hy + 5, e2x = hx + 13, e2y = hy + 5;
    if (g_dir == DIR_DOWN)  { e1y = hy + 13; e2y = hy + 13; }
    if (g_dir == DIR_LEFT)  { e1x = hx + 5; e1y = hy + 5; e2x = hx + 5; e2y = hy + 13; }
    if (g_dir == DIR_RIGHT) { e1x = hx + 13; e1y = hy + 5; e2x = hx + 13; e2y = hy + 13; }
    dui_fill_rect(conn, win, e1x, e1y, 3, 3, 0x001B2631);
    dui_fill_rect(conn, win, e2x, e2y, 3, 3, 0x001B2631);

    /* Bottom status hint */
    dui_draw_text(conn, win, 20, 468, "[WASD/Arrows] Move  [P] Pause  [R] Restart", 0x0095A5A6);

    /* Game Over Modal Overlay */
    if (g_game_over) {
        int modal_w = 280, modal_h = 130;
        int mx = (WIN_W - modal_w) / 2;
        int my = (WIN_H - modal_h) / 2;

        dui_fill_rect(conn, win, mx, my, modal_w, modal_h, GUI_PANEL_BG);
        dui_draw_rect(conn, win, mx, my, modal_w, modal_h, GUI_TEXT_RED);
        dui_draw_rect(conn, win, mx + 2, my + 2, modal_w - 4, modal_h - 4, GUI_GRID_BORDER);

        dui_draw_text(conn, win, mx + 70, my + 25, "G A M E   O V E R", GUI_TEXT_RED);

        char final_score[64];
        snprintf(final_score, sizeof(final_score), "Final Score: %d", g_score);
        dui_draw_text(conn, win, mx + 85, my + 55, final_score, GUI_TEXT_WHITE);

        dui_draw_text(conn, win, mx + 28, my + 90, "Click or Press [R] to Play Again", 0x00F39C12);
    }

    dui_flush(conn, win);
}

static void run_gui(DuiConnection *conn) {
    DuiWindow win = dui_create_window(conn, 25, 55, WIN_W, WIN_H,
                                      "DUnix Snake", GUI_BG_COLOR, DWS_WIN_DECORATED);
    dui_show(conn, win);
    init_game();
    gui_render(conn, win);

    DuiEvent ev;

    while (1) {
        /* Check events */
        while (dui_poll_event(conn, &ev) > 0) {
            if (ev.type == DWS_EV_CLOSE_REQ && ev.window == win) {
                dui_destroy_window(conn, win);
                dui_disconnect(conn);
                return;
            } else if (ev.type == DWS_EV_EXPOSE && ev.window == win) {
                gui_render(conn, win);
            } else if (ev.type == DWS_EV_MOUSE_DOWN && ev.window == win) {
                if (g_game_over) {
                    init_game();
                    gui_render(conn, win);
                }
            } else if (ev.type == DWS_EV_KEY_DOWN && ev.window == win) {
                char c = ev.key.ch;
                uint32_t k = ev.key.keycode;

                if (c == 'w' || c == 'W' || k == 0x11 /* Up */)    change_dir(DIR_UP);
                else if (c == 's' || c == 'S' || k == 0x12 /* Down */)  change_dir(DIR_DOWN);
                else if (c == 'a' || c == 'A' || k == 0x13 /* Left */)  change_dir(DIR_LEFT);
                else if (c == 'd' || c == 'D' || k == 0x14 /* Right */) change_dir(DIR_RIGHT);
                else if (c == 'p' || c == 'P') g_paused = !g_paused;
                else if (c == 'r' || c == 'R') { init_game(); }
                else if (c == 'q' || c == 'Q') {
                    dui_destroy_window(conn, win);
                    dui_disconnect(conn);
                    return;
                }
                gui_render(conn, win);
            }
        }

        /* Game step tick */
        update_game();
        gui_render(conn, win);

        usleep(g_speed_ms * 1000);
    }
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
            printf("Usage: snake [OPTIONS]\n");
            printf("Classic Snake game with both Terminal (TUI) and DWS Window (GUI) interfaces.\n\n");
            printf("Options:\n");
            printf("  -t, --tui    Force text-mode terminal interface\n");
            printf("  -g, --gui    Force graphical DWS window interface\n");
            printf("  -h, --help   Display this help message and exit\n\n");
            printf("By default, snake detects if DWS desktop is running and opens the GUI window,\n");
            printf("otherwise falling back seamlessly to full-color interactive TUI mode.\n");
            return 0;
        }
    }

    if (force_tui) {
        run_tui();
        return 0;
    }

    /* Attempt to connect to DWS graphical server */
    DuiConnection *conn = dui_connect();
    if (conn) {
        if (!force_tui) {
            run_gui(conn);
            return 0;
        }
        dui_disconnect(conn);
    }

    if (force_gui) {
        fprintf(stderr, "snake: cannot connect to DWS graphical display server.\n");
        fprintf(stderr, "-> Start DWS with '/bin/dws' or run in text mode: 'snake -t'\n");
        return 1;
    }

    /* Fallback to TUI mode */
    run_tui();
    return 0;
}
