/*
 * DWeb - High Performance Graphical Web Browser for DUnix
 * Featuring WebKit-Style HTML Tokenizer, DOM & Layout Engine,
 * BSD Socket HTTP Client, and DUI Desktop Integration.
 *
 * Copyright (c) 2026 DUnix Project. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <dui/dui.h>
#include <dui/protocol.h>

#include "net.h"
#include "html_engine.h"

/* Unity build: include implementations for single-compilation-unit Makefile */
#include "net.c"
#include "html_engine.c"

#define WIN_W           740
#define WIN_H           500
#define TOOLBAR_H       42
#define STATUSBAR_H     24
#define VIEW_Y          TOOLBAR_H
#define VIEW_H          (WIN_H - TOOLBAR_H - STATUSBAR_H)
#define SCROLLBAR_W     14
#define VIEW_W          (WIN_W - SCROLLBAR_W)

#define COLOR_CHROME_BG 0x001E222A  /* Dark slate toolbar */
#define COLOR_CHROME_BORDER 0x00333B48
#define COLOR_BTN_BG    0x002B313D
#define COLOR_BTN_FG    0x00E2E8F0
#define COLOR_BTN_DIM   0x00556070
#define COLOR_URL_BG    0x0012151B
#define COLOR_URL_BORDER 0x003A4354
#define COLOR_URL_FOCUS 0x002563EB  /* Blue focus outline */
#define COLOR_URL_TEXT  0x00F8FAFC
#define COLOR_VIEW_BG   0x00FFFFFF  /* Page viewport white */
#define COLOR_SCROLL_TRACK 0x00F1F5F9
#define COLOR_SCROLL_THUMB 0x0094A3B8
#define COLOR_STATUS_FG 0x0094A3B8

/* History Stack */
#define MAX_HISTORY 64
static char g_history[MAX_HISTORY][256];
static int  g_hist_count = 0;
static int  g_hist_pos = -1;

/* Browser State */
static char g_current_url[256] = "about:home";
static char g_input_url[256]   = "about:home";
static bool g_input_focused    = false;
static int  g_scroll_y         = 0;
static int  g_hovered_link     = -1;
static char g_status_msg[128]  = "Ready";
static char g_http_code[64]    = "200 OK";
static struct layout_document *g_doc = NULL;
static bool g_dragging_scroll  = false;
static int  g_drag_start_y     = 0;
static int  g_scroll_start_pos = 0;

static void draw_browser_chrome(DuiConnection *conn, DuiWindow win) {
    /* 1. Toolbar Background */
    dui_fill_rect(conn, win, 0, 0, WIN_W, TOOLBAR_H, COLOR_CHROME_BG);
    dui_draw_line(conn, win, 0, TOOLBAR_H - 1, WIN_W, TOOLBAR_H - 1, COLOR_CHROME_BORDER);

    /* Back Button: [ < ] */
    bool can_back = (g_hist_pos > 0);
    dui_fill_rect(conn, win, 8, 7, 28, 28, COLOR_BTN_BG);
    dui_draw_rect(conn, win, 8, 7, 28, 28, COLOR_CHROME_BORDER);
    dui_draw_text(conn, win, 17, 13, "<", can_back ? COLOR_BTN_FG : COLOR_BTN_DIM);

    /* Forward Button: [ > ] */
    bool can_fwd = (g_hist_pos < g_hist_count - 1);
    dui_fill_rect(conn, win, 40, 7, 28, 28, COLOR_BTN_BG);
    dui_draw_rect(conn, win, 40, 7, 28, 28, COLOR_CHROME_BORDER);
    dui_draw_text(conn, win, 49, 13, ">", can_fwd ? COLOR_BTN_FG : COLOR_BTN_DIM);

    /* Reload Button: [ R ] */
    dui_fill_rect(conn, win, 72, 7, 28, 28, COLOR_BTN_BG);
    dui_draw_rect(conn, win, 72, 7, 28, 28, COLOR_CHROME_BORDER);
    dui_draw_text(conn, win, 81, 13, "R", COLOR_BTN_FG);

    /* Home Button: [ H ] */
    dui_fill_rect(conn, win, 104, 7, 28, 28, COLOR_BTN_BG);
    dui_draw_rect(conn, win, 104, 7, 28, 28, COLOR_CHROME_BORDER);
    dui_draw_text(conn, win, 113, 13, "H", COLOR_BTN_FG);

    /* Address Bar */
    int url_x = 138;
    int url_w = WIN_W - url_x - 52;
    dui_fill_rect(conn, win, url_x, 7, url_w, 28, COLOR_URL_BG);
    dui_draw_rect(conn, win, url_x, 7, url_w, 28, g_input_focused ? COLOR_URL_FOCUS : COLOR_URL_BORDER);

    /* URL Text */
    char disp_url[64];
    int ulen = (int)strlen(g_input_url);
    int max_chars = (url_w - 16) / 8;
    if (ulen > max_chars) {
        strncpy(disp_url, g_input_url + (ulen - max_chars), sizeof(disp_url) - 1);
        disp_url[sizeof(disp_url) - 1] = '\0';
    } else {
        strncpy(disp_url, g_input_url, sizeof(disp_url) - 1);
        disp_url[sizeof(disp_url) - 1] = '\0';
    }
    dui_draw_text(conn, win, url_x + 8, 13, disp_url, COLOR_URL_TEXT);

    /* Cursor when editing */
    if (g_input_focused) {
        int cx = url_x + 8 + (int)strlen(disp_url) * 8;
        if (cx < url_x + url_w - 6) {
            dui_draw_line(conn, win, cx, 11, cx, 30, COLOR_URL_TEXT);
        }
    }

    /* Go Button: [ Go ] */
    int go_x = WIN_W - 46;
    dui_fill_rect(conn, win, go_x, 7, 38, 28, COLOR_BTN_BG);
    dui_draw_rect(conn, win, go_x, 7, 38, 28, COLOR_CHROME_BORDER);
    dui_draw_text(conn, win, go_x + 10, 13, "Go", COLOR_BTN_FG);

    /* 2. Status Bar Background */
    int status_y = WIN_H - STATUSBAR_H;
    dui_fill_rect(conn, win, 0, status_y, WIN_W, STATUSBAR_H, COLOR_CHROME_BG);
    dui_draw_line(conn, win, 0, status_y, WIN_W, status_y, COLOR_CHROME_BORDER);

    /* Left status message */
    char s_msg[80];
    if (g_hovered_link >= 0 && g_hovered_link < g_doc->link_count) {
        snprintf(s_msg, sizeof(s_msg), "-> %s", g_doc->links[g_hovered_link].url);
    } else {
        snprintf(s_msg, sizeof(s_msg), "%s", g_status_msg);
    }
    dui_draw_text(conn, win, 10, status_y + 5, s_msg, COLOR_STATUS_FG);

    /* Right status code & engine */
    char r_msg[64];
    snprintf(r_msg, sizeof(r_msg), "%s | DWeb Engine", g_http_code);
    int rw = (int)strlen(r_msg) * 8;
    dui_draw_text(conn, win, WIN_W - rw - 12, status_y + 5, r_msg, COLOR_STATUS_FG);

    /* 3. Scrollbar on right edge */
    int sb_x = WIN_W - SCROLLBAR_W;
    dui_fill_rect(conn, win, sb_x, VIEW_Y, SCROLLBAR_W, VIEW_H, COLOR_SCROLL_TRACK);
    dui_draw_line(conn, win, sb_x, VIEW_Y, sb_x, VIEW_Y + VIEW_H, COLOR_CHROME_BORDER);

    if (g_doc && g_doc->content_height > VIEW_H) {
        int max_scroll = g_doc->content_height - VIEW_H;
        int thumb_h = (VIEW_H * VIEW_H) / g_doc->content_height;
        if (thumb_h < 24) thumb_h = 24;

        int thumb_y = VIEW_Y + (g_scroll_y * (VIEW_H - thumb_h)) / max_scroll;
        dui_fill_rect(conn, win, sb_x + 2, thumb_y, SCROLLBAR_W - 4, thumb_h, COLOR_SCROLL_THUMB);
        dui_draw_rect(conn, win, sb_x + 2, thumb_y, SCROLLBAR_W - 4, thumb_h, COLOR_CHROME_BORDER);
    }
}

static void render_browser(DuiConnection *conn, DuiWindow win) {
    /* 1. Clear viewport to clean page background */
    dui_fill_rect(conn, win, 0, VIEW_Y, VIEW_W, VIEW_H, COLOR_VIEW_BG);

    /* 2. Render page elements */
    if (g_doc) {
        html_render_dui(conn, win, g_doc, 0, VIEW_Y, VIEW_W, VIEW_H, g_scroll_y, g_hovered_link);
    }

    /* 3. Render browser chrome (toolbar, status bar, scrollbar) */
    draw_browser_chrome(conn, win);
    dui_flush(conn, win);
}

static void navigate_to(DuiConnection *conn, DuiWindow win, const char *target, bool add_to_history) {
    if (!target || target[0] == '\0') return;

    strncpy(g_current_url, target, sizeof(g_current_url) - 1);
    g_current_url[sizeof(g_current_url) - 1] = '\0';
    strncpy(g_input_url, target, sizeof(g_input_url) - 1);
    g_input_url[sizeof(g_input_url) - 1] = '\0';

    snprintf(g_status_msg, sizeof(g_status_msg), "Loading %s...", target);
    render_browser(conn, win);

    char *body = NULL;
    size_t body_len = 0;
    int status = 0;
    char page_title[64] = "";

    net_fetch(target, &body, &body_len, &status, g_http_code, sizeof(g_http_code), page_title, sizeof(page_title));

    if (!body) {
        body = strdup("<!DOCTYPE html><html><body><h1>Network Error</h1><p>Failed to load URL</p></body></html>");
        body_len = strlen(body);
    }

    if (!g_doc) g_doc = layout_doc_create();
    html_layout(g_doc, body, VIEW_W);
    free(body);

    g_scroll_y = 0;
    g_hovered_link = -1;
    snprintf(g_status_msg, sizeof(g_status_msg), "Done (%lu bytes)", (unsigned long)body_len);

    /* Update window title */
    char win_title[128];
    if (g_doc->title[0]) {
        snprintf(win_title, sizeof(win_title), "DWeb - %s", g_doc->title);
    } else {
        snprintf(win_title, sizeof(win_title), "DWeb - %s", target);
    }
    dui_set_title(conn, win, win_title);

    /* Add to history */
    if (add_to_history) {
        if (g_hist_pos < g_hist_count - 1) {
            g_hist_count = g_hist_pos + 1;
        }
        if (g_hist_count < MAX_HISTORY) {
            strncpy(g_history[g_hist_count], target, sizeof(g_history[0]) - 1);
            g_history[g_hist_count][sizeof(g_history[0]) - 1] = '\0';
            g_hist_pos = g_hist_count;
            g_hist_count++;
        }
    }

    render_browser(conn, win);
}

int main(int argc, char **argv) {
    const char *initial_url = (argc > 1) ? argv[1] : "about:home";

    /* Connect to DWS Display Server */
    DuiConnection *conn = dui_connect();
    if (!conn) {
        /* Terminal fallback mode: print formatted page to console */
        char *body = NULL;
        size_t len = 0;
        int status = 0;
        char status_txt[64];
        char title[64];

        printf("dweb: running in console text mode (no display server)\n");
        printf("Fetching: %s\n", initial_url);

        net_fetch(initial_url, &body, &len, &status, status_txt, sizeof(status_txt), title, sizeof(title));
        if (body) {
            html_render_terminal(body);
            free(body);
        } else {
            fprintf(stderr, "dweb: failed to load %s (%s)\n", initial_url, status_txt);
        }
        return 0;
    }

    /* Create GUI browser window */
    DuiWindow win = dui_create_window(conn, 60, 40, WIN_W, WIN_H, "DWeb - Web Browser", COLOR_VIEW_BG, DWS_WIN_DECORATED | DWS_WIN_RESIZABLE);
    dui_show(conn, win);

    g_doc = layout_doc_create();
    navigate_to(conn, win, initial_url, true);

    DuiEvent ev;
    while (1) {
        if (dui_next_event(conn, &ev) > 0) {
            if (ev.type == DWS_EV_CLOSE_REQ && ev.window == win) {
                break;
            } else if (ev.type == DWS_EV_EXPOSE && ev.window == win) {
                render_browser(conn, win);
            } else if (ev.type == DWS_EV_MOUSE_MOVE && ev.window == win) {
                int mx = ev.mouse.x;
                int my = ev.mouse.y;

                if (g_dragging_scroll && g_doc && g_doc->content_height > VIEW_H) {
                    int dy = my - g_drag_start_y;
                    int max_scroll = g_doc->content_height - VIEW_H;
                    int thumb_h = (VIEW_H * VIEW_H) / g_doc->content_height;
                    if (thumb_h < 24) thumb_h = 24;
                    int scroll_range = VIEW_H - thumb_h;
                    if (scroll_range > 0) {
                        int delta_scroll = (dy * max_scroll) / scroll_range;
                        g_scroll_y = g_scroll_start_pos + delta_scroll;
                        if (g_scroll_y < 0) g_scroll_y = 0;
                        if (g_scroll_y > max_scroll) g_scroll_y = max_scroll;
                        render_browser(conn, win);
                    }
                    continue;
                }

                /* Check for link hover in viewport */
                if (my >= VIEW_Y && my < VIEW_Y + VIEW_H && mx < VIEW_W) {
                    int hit = html_find_link_at(g_doc, mx, my - VIEW_Y, g_scroll_y);
                    if (hit != g_hovered_link) {
                        g_hovered_link = hit;
                        render_browser(conn, win);
                    }
                } else if (g_hovered_link != -1) {
                    g_hovered_link = -1;
                    render_browser(conn, win);
                }
            } else if (ev.type == DWS_EV_MOUSE_UP && ev.window == win) {
                if (g_dragging_scroll) {
                    g_dragging_scroll = false;
                }
            } else if (ev.type == DWS_EV_MOUSE_DOWN && ev.window == win) {
                int mx = ev.mouse.x;
                int my = ev.mouse.y;

                /* 1. Toolbar Interactions */
                if (my >= 0 && my < TOOLBAR_H) {
                    g_input_focused = false;

                    /* Back Button [ < ] */
                    if (mx >= 8 && mx < 36 && g_hist_pos > 0) {
                        g_hist_pos--;
                        navigate_to(conn, win, g_history[g_hist_pos], false);
                    }
                    /* Forward Button [ > ] */
                    else if (mx >= 40 && mx < 68 && g_hist_pos < g_hist_count - 1) {
                        g_hist_pos++;
                        navigate_to(conn, win, g_history[g_hist_pos], false);
                    }
                    /* Reload Button [ R ] */
                    else if (mx >= 72 && mx < 100) {
                        navigate_to(conn, win, g_current_url, false);
                    }
                    /* Home Button [ H ] */
                    else if (mx >= 104 && mx < 132) {
                        navigate_to(conn, win, "about:home", true);
                    }
                    /* Address Bar */
                    else if (mx >= 138 && mx < WIN_W - 52) {
                        g_input_focused = true;
                        render_browser(conn, win);
                    }
                    /* Go Button */
                    else if (mx >= WIN_W - 46 && mx < WIN_W - 8) {
                        navigate_to(conn, win, g_input_url, true);
                    }
                }
                /* 2. Scrollbar Interactions */
                else if (mx >= VIEW_W && mx < WIN_W && my >= VIEW_Y && my < VIEW_Y + VIEW_H) {
                    g_input_focused = false;
                    if (g_doc && g_doc->content_height > VIEW_H) {
                        int max_scroll = g_doc->content_height - VIEW_H;
                        int thumb_h = (VIEW_H * VIEW_H) / g_doc->content_height;
                        if (thumb_h < 24) thumb_h = 24;

                        int thumb_y = VIEW_Y + (g_scroll_y * (VIEW_H - thumb_h)) / max_scroll;
                        if (my >= thumb_y && my <= thumb_y + thumb_h) {
                            g_dragging_scroll = true;
                            g_drag_start_y = my;
                            g_scroll_start_pos = g_scroll_y;
                        } else if (my < thumb_y) {
                            g_scroll_y -= VIEW_H / 2;
                            if (g_scroll_y < 0) g_scroll_y = 0;
                            render_browser(conn, win);
                        } else {
                            g_scroll_y += VIEW_H / 2;
                            if (g_scroll_y > max_scroll) g_scroll_y = max_scroll;
                            render_browser(conn, win);
                        }
                    }
                }
                /* 3. Viewport Link Click */
                else if (my >= VIEW_Y && my < VIEW_Y + VIEW_H && mx < VIEW_W) {
                    g_input_focused = false;
                    int hit = html_find_link_at(g_doc, mx, my - VIEW_Y, g_scroll_y);
                    if (hit >= 0 && hit < g_doc->link_count) {
                        char resolved[256];
                        url_resolve(g_current_url, g_doc->links[hit].url, resolved, sizeof(resolved));
                        navigate_to(conn, win, resolved, true);
                    }
                }
            } else if (ev.type == DWS_EV_KEY_DOWN && ev.window == win) {
                char ch = ev.key.ch;

                if (g_input_focused) {
                    if (ch == '\r' || ch == '\n') {
                        g_input_focused = false;
                        navigate_to(conn, win, g_input_url, true);
                    } else if (ch == 0x1B) { /* Escape */
                        g_input_focused = false;
                        strncpy(g_input_url, g_current_url, sizeof(g_input_url) - 1);
                        render_browser(conn, win);
                    } else if (ch == 0x7F || ch == 0x08) { /* Backspace */
                        size_t l = strlen(g_input_url);
                        if (l > 0) {
                            g_input_url[l - 1] = '\0';
                            render_browser(conn, win);
                        }
                    } else if (ch >= 32 && ch <= 126) {
                        size_t l = strlen(g_input_url);
                        if (l < sizeof(g_input_url) - 2) {
                            g_input_url[l] = ch;
                            g_input_url[l + 1] = '\0';
                            render_browser(conn, win);
                        }
                    }
                } else {
                    /* Scrolling & shortcuts when not editing URL */
                    int max_scroll = (g_doc && g_doc->content_height > VIEW_H) ? (g_doc->content_height - VIEW_H) : 0;

                    if (ch == 'j' || ch == 'J' || ch == ' ') { /* Scroll Down */
                        g_scroll_y += (ch == ' ') ? (VIEW_H - 40) : 40;
                        if (g_scroll_y > max_scroll) g_scroll_y = max_scroll;
                        render_browser(conn, win);
                    } else if (ch == 'k' || ch == 'K') { /* Scroll Up */
                        g_scroll_y -= 40;
                        if (g_scroll_y < 0) g_scroll_y = 0;
                        render_browser(conn, win);
                    } else if (ch == 'h' || ch == 'H') { /* Back */
                        if (g_hist_pos > 0) {
                            g_hist_pos--;
                            navigate_to(conn, win, g_history[g_hist_pos], false);
                        }
                    } else if (ch == 'l' || ch == 'L') { /* Forward */
                        if (g_hist_pos < g_hist_count - 1) {
                            g_hist_pos++;
                            navigate_to(conn, win, g_history[g_hist_pos], false);
                        }
                    } else if (ch == 'r' || ch == 'R') { /* Reload */
                        navigate_to(conn, win, g_current_url, false);
                    } else if (ch == '/' || ch == 'o' || ch == 'O') { /* Focus URL bar */
                        g_input_focused = true;
                        render_browser(conn, win);
                    }
                }
            }
        }
    }

    if (g_doc) layout_doc_free(g_doc);
    dui_destroy_window(conn, win);
    dui_disconnect(conn);
    return 0;
}
