#include <dui/dui.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ─── Internal Helpers ─── */

static void dui_send(DuiConnection *conn, struct dws_message *msg) {
    if (conn && conn->fd >= 0) {
        size_t total = sizeof(struct dws_message);
        size_t sent = 0;
        const uint8_t *p = (const uint8_t *)msg;
        while (sent < total) {
            ssize_t s = send(conn->fd, p + sent, total - sent, 0);
            if (s <= 0) break;
            sent += s;
        }
    }
}

static int dui_recv_blocking(DuiConnection *conn, struct dws_message *msg) {
    if (!conn || conn->fd < 0) return -1;
    /* Temporarily clear O_NONBLOCK for blocking recv */
    int flags = fcntl(conn->fd, F_GETFL, 0);
    fcntl(conn->fd, F_SETFL, flags & ~O_NONBLOCK);
    ssize_t n = recv(conn->fd, msg, sizeof(struct dws_message), 0);
    fcntl(conn->fd, F_SETFL, flags);
    return (n == (ssize_t)sizeof(struct dws_message)) ? 0 : -1;
}

static int dui_recv_nonblock(DuiConnection *conn, struct dws_message *msg) {
    if (!conn || conn->fd < 0) return -1;
    ssize_t n = recv(conn->fd, msg, sizeof(struct dws_message), MSG_DONTWAIT);
    return (n == (ssize_t)sizeof(struct dws_message)) ? 0 : -1;
}

static void dui_msg_to_event(struct dws_message *msg, DuiEvent *ev) {
    ev->type = msg->type;
    ev->window = msg->window_id;
    switch (msg->type) {
        case DWS_EV_MOUSE_DOWN:
        case DWS_EV_MOUSE_UP:
        case DWS_EV_MOUSE_MOVE:
            ev->mouse.x = msg->mouse.mx;
            ev->mouse.y = msg->mouse.my;
            ev->mouse.button = msg->mouse.button;
            ev->mouse.root_x = msg->mouse.root_x;
            ev->mouse.root_y = msg->mouse.root_y;
            break;
        case DWS_EV_KEY_DOWN:
        case DWS_EV_KEY_UP:
            ev->key.keycode = msg->key.keycode;
            ev->key.ch = msg->key.ch;
            break;
        case DWS_EV_EXPOSE:
        case DWS_EV_RESIZED:
            ev->resize.width = msg->expose.ev_width;
            ev->resize.height = msg->expose.ev_height;
            break;
        default:
            break;
    }
}

/* ─── Connection ─── */

DuiConnection *dui_connect(void) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return NULL;

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(DWS_PORT);
    saddr.sin_addr.s_addr = htonl(0x7F000001); /* 127.0.0.1 */

    if (connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        close(sockfd);
        return NULL;
    }

    /* Send connect request */
    struct dws_message req;
    memset(&req, 0, sizeof(req));
    req.type = DWS_REQ_CONNECT;
    send(sockfd, &req, sizeof(req), 0);

    /* Receive connect reply */
    struct dws_message reply;
    ssize_t n = recv(sockfd, &reply, sizeof(reply), 0);
    if (n != (ssize_t)sizeof(reply) || reply.type != DWS_EV_CONNECTED) {
        close(sockfd);
        return NULL;
    }

    DuiConnection *conn = (DuiConnection *)malloc(sizeof(DuiConnection));
    if (!conn) {
        close(sockfd);
        return NULL;
    }

    conn->fd = sockfd;
    conn->client_id = reply.connected.client_id;
    conn->screen_width = reply.connected.screen_width;
    conn->screen_height = reply.connected.screen_height;
    conn->next_wid = conn->client_id * 1000 + 1;

    return conn;
}

void dui_disconnect(DuiConnection *conn) {
    if (!conn) return;
    if (conn->fd >= 0) close(conn->fd);
    free(conn);
}

uint32_t dui_screen_width(DuiConnection *conn) {
    return conn ? conn->screen_width : 1024;
}

uint32_t dui_screen_height(DuiConnection *conn) {
    return conn ? conn->screen_height : 768;
}

/* ─── Window Management ─── */

DuiWindow dui_create_window(DuiConnection *conn, int x, int y, int w, int h,
                            const char *title, uint32_t bg_color, uint32_t flags) {
    if (!conn) return 0;
    DuiWindow wid = conn->next_wid++;

    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_CREATE_WINDOW;
    msg.window_id = wid;
    msg.create.x = x;
    msg.create.y = y;
    msg.create.width = (uint32_t)w;
    msg.create.height = (uint32_t)h;
    msg.create.bg_color = bg_color;
    msg.create.flags = flags;
    if (title) strncpy(msg.create.title, title, sizeof(msg.create.title) - 1);
    dui_send(conn, &msg);
    return wid;
}

void dui_destroy_window(DuiConnection *conn, DuiWindow win) {
    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_DESTROY_WINDOW;
    msg.window_id = win;
    dui_send(conn, &msg);
}

void dui_show(DuiConnection *conn, DuiWindow win) {
    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_SHOW;
    msg.window_id = win;
    dui_send(conn, &msg);
}

void dui_hide(DuiConnection *conn, DuiWindow win) {
    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_HIDE;
    msg.window_id = win;
    dui_send(conn, &msg);
}

void dui_move(DuiConnection *conn, DuiWindow win, int x, int y) {
    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_MOVE;
    msg.window_id = win;
    msg.move.x = x;
    msg.move.y = y;
    dui_send(conn, &msg);
}

void dui_resize(DuiConnection *conn, DuiWindow win, int w, int h) {
    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_RESIZE;
    msg.window_id = win;
    msg.resize.width = (uint32_t)w;
    msg.resize.height = (uint32_t)h;
    dui_send(conn, &msg);
}

void dui_set_title(DuiConnection *conn, DuiWindow win, const char *title) {
    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_SET_TITLE;
    msg.window_id = win;
    if (title) strncpy(msg.set_title.title, title, sizeof(msg.set_title.title) - 1);
    dui_send(conn, &msg);
}

void dui_raise(DuiConnection *conn, DuiWindow win) {
    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_RAISE;
    msg.window_id = win;
    dui_send(conn, &msg);
}

/* ─── Drawing ─── */

void dui_fill_rect(DuiConnection *conn, DuiWindow win, int x, int y, int w, int h, uint32_t color) {
    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_FILL_RECT;
    msg.window_id = win;
    msg.rect.x = x;
    msg.rect.y = y;
    msg.rect.width = (uint32_t)w;
    msg.rect.height = (uint32_t)h;
    msg.rect.color = color;
    dui_send(conn, &msg);
}

void dui_draw_rect(DuiConnection *conn, DuiWindow win, int x, int y, int w, int h, uint32_t color) {
    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_DRAW_RECT;
    msg.window_id = win;
    msg.rect.x = x;
    msg.rect.y = y;
    msg.rect.width = (uint32_t)w;
    msg.rect.height = (uint32_t)h;
    msg.rect.color = color;
    dui_send(conn, &msg);
}

void dui_draw_line(DuiConnection *conn, DuiWindow win, int x1, int y1, int x2, int y2, uint32_t color) {
    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_DRAW_LINE;
    msg.window_id = win;
    msg.line.x1 = x1;
    msg.line.y1 = y1;
    msg.line.x2 = x2;
    msg.line.y2 = y2;
    msg.line.color = color;
    dui_send(conn, &msg);
}

void dui_draw_text(DuiConnection *conn, DuiWindow win, int x, int y, const char *text, uint32_t color) {
    if (!text) return;
    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_DRAW_TEXT;
    msg.window_id = win;
    msg.text.x = x;
    msg.text.y = y;
    msg.text.color = color;
    msg.text.len = (uint32_t)strlen(text);
    if (msg.text.len > 127) msg.text.len = 127;
    strncpy(msg.text.text, text, msg.text.len);
    dui_send(conn, &msg);
}

void dui_set_pixel(DuiConnection *conn, DuiWindow win, int x, int y, uint32_t color) {
    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_SET_PIXEL;
    msg.window_id = win;
    msg.pixel.x = x;
    msg.pixel.y = y;
    msg.pixel.color = color;
    dui_send(conn, &msg);
}

void dui_clear(DuiConnection *conn, DuiWindow win, uint32_t color) {
    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_CLEAR;
    msg.window_id = win;
    msg.clear.color = color;
    dui_send(conn, &msg);
}

void dui_flush(DuiConnection *conn, DuiWindow win) {
    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_FLUSH;
    msg.window_id = win;
    dui_send(conn, &msg);
}

void dui_fill_circle(DuiConnection *conn, DuiWindow win, int cx, int cy, int r, uint32_t color) {
    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_FILL_CIRCLE;
    msg.window_id = win;
    msg.circle.cx = cx;
    msg.circle.cy = cy;
    msg.circle.radius = (uint32_t)r;
    msg.circle.color = color;
    dui_send(conn, &msg);
}

void dui_draw_circle(DuiConnection *conn, DuiWindow win, int cx, int cy, int r, uint32_t color) {
    struct dws_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = DWS_REQ_DRAW_CIRCLE;
    msg.window_id = win;
    msg.circle.cx = cx;
    msg.circle.cy = cy;
    msg.circle.radius = (uint32_t)r;
    msg.circle.color = color;
    dui_send(conn, &msg);
}

/* ─── Events ─── */

int dui_next_event(DuiConnection *conn, DuiEvent *ev) {
    if (!conn || !ev) return 0;
    struct dws_message msg;
    if (dui_recv_blocking(conn, &msg) != 0) return 0;
    dui_msg_to_event(&msg, ev);
    return 1;
}

int dui_poll_event(DuiConnection *conn, DuiEvent *ev) {
    if (!conn || !ev) return 0;
    struct dws_message msg;
    if (dui_recv_nonblock(conn, &msg) == 0) {
        dui_msg_to_event(&msg, ev);
        return 1;
    }
    return 0;
}

int dui_has_event(DuiConnection *conn) {
    if (!conn) return 0;
    struct dws_message msg;
    ssize_t n = recv(conn->fd, &msg, sizeof(msg), MSG_PEEK | MSG_DONTWAIT);
    return (n >= (ssize_t)sizeof(msg)) ? 1 : 0;
}

void dui_blit_buffer(DuiConnection *conn, DuiWindow win, const uint32_t *pixels, int w, int h) {
    if (!conn || !pixels || w <= 0 || h <= 0) return;

    /* Stream pixel buffer in scanline strips to prevent socket ring buffer saturation */
    int strip_h = 8;
    for (int y = 0; y < h; y += strip_h) {
        int cur_h = strip_h;
        if (y + cur_h > h) cur_h = h - y;

        struct dws_message msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = DWS_REQ_BLIT_BUFFER;
        msg.window_id = win;
        msg.rect.x = 0;
        msg.rect.y = y;
        msg.rect.width = (uint32_t)w;
        msg.rect.height = (uint32_t)cur_h;
        dui_send(conn, &msg);

        size_t total = (size_t)w * cur_h * sizeof(uint32_t);
        size_t sent = 0;
        const uint8_t *src = (const uint8_t *)(pixels + y * w);
        while (sent < total) {
            ssize_t s = send(conn->fd, src + sent, total - sent, 0);
            if (s <= 0) break;
            sent += s;
        }
    }
}
