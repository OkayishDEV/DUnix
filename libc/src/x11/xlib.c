#include <X11/Xlib.h>
#include <X11/xproto.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

Display *XOpenDisplay(const char *display_name) {
    (void)display_name;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return NULL;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(X11_TCP_PORT);
    saddr.sin_addr.s_addr = htonl(0x7F000001); /* 127.0.0.1 */

    if (connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        close(sockfd);
        return NULL;
    }

    /* Send connect handshake */
    struct x11_connect_req req;
    memset(&req, 0, sizeof(req));
    req.hdr.opcode = X_OP_CONNECT;
    req.hdr.length = sizeof(req) / 4;
    req.major_version = 11;
    req.minor_version = 0;

    if (send(sockfd, &req, sizeof(req), 0) != (ssize_t)sizeof(req)) {
        close(sockfd);
        return NULL;
    }

    struct x11_connect_reply reply;
    if (recv(sockfd, &reply, sizeof(reply), 0) != (ssize_t)sizeof(reply)) {
        close(sockfd);
        return NULL;
    }

    if (reply.status != 1) {
        close(sockfd);
        return NULL;
    }

    Display *dpy = (Display *)malloc(sizeof(Display));
    if (!dpy) {
        close(sockfd);
        return NULL;
    }

    memset(dpy, 0, sizeof(Display));
    dpy->fd = sockfd;
    dpy->default_screen = 0;
    dpy->resource_id_base = reply.resource_id_base;
    dpy->next_resource_id = reply.resource_id_base + 1;
    dpy->root_window = reply.root_window;
    strncpy(dpy->display_name, ":0.0", sizeof(dpy->display_name) - 1);

    dpy->screens[0].width = (int)reply.width;
    dpy->screens[0].height = (int)reply.height;
    dpy->screens[0].root = reply.root_window;
    dpy->screens[0].white_pixel = reply.white_pixel;
    dpy->screens[0].black_pixel = reply.black_pixel;
    dpy->screens[0].root_depth = (int)reply.depth;

    return dpy;
}

int XCloseDisplay(Display *dpy) {
    if (!dpy) return 0;
    if (dpy->fd >= 0) {
        close(dpy->fd);
    }
    free(dpy);
    return 0;
}

int XFlush(Display *dpy) {
    (void)dpy;
    return 0;
}

int XSync(Display *dpy, Bool discard) {
    if (!dpy) return 0;
    (void)discard;
    struct x11_req_header hdr;
    hdr.opcode = X_OP_SYNC;
    hdr.pad = 0;
    hdr.length = sizeof(hdr) / 4;
    send(dpy->fd, &hdr, sizeof(hdr), 0);
    uint32_t resp = 0;
    recv(dpy->fd, &resp, sizeof(resp), 0);
    return 0;
}

Window XDefaultRootWindow(Display *dpy) {
    return dpy ? dpy->root_window : 0;
}

int XDefaultScreen(Display *dpy) {
    return dpy ? dpy->default_screen : 0;
}

int XDisplayWidth(Display *dpy, int screen_number) {
    (void)screen_number;
    return dpy ? dpy->screens[0].width : 1024;
}

int XDisplayHeight(Display *dpy, int screen_number) {
    (void)screen_number;
    return dpy ? dpy->screens[0].height : 768;
}

unsigned long XBlackPixel(Display *dpy, int screen_number) {
    (void)screen_number;
    return dpy ? dpy->screens[0].black_pixel : 0x00000000;
}

unsigned long XWhitePixel(Display *dpy, int screen_number) {
    (void)screen_number;
    return dpy ? dpy->screens[0].white_pixel : 0x00FFFFFF;
}

Window XCreateSimpleWindow(Display *dpy, Window parent, int x, int y,
                           unsigned int width, unsigned int height,
                           unsigned int border_width, unsigned long border,
                           unsigned long background) {
    if (!dpy) return 0;

    Window wid = dpy->next_resource_id++;

    struct x11_create_window_req req;
    memset(&req, 0, sizeof(req));
    req.hdr.opcode = X_OP_CREATE_WINDOW;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)wid;
    req.parent = (uint32_t)parent;
    req.x = x;
    req.y = y;
    req.width = width;
    req.height = height;
    req.border_width = border_width;
    req.border_pixel = (uint32_t)border;
    req.background_pixel = (uint32_t)background;

    send(dpy->fd, &req, sizeof(req), 0);
    return wid;
}

Window XCreateWindow(Display *dpy, Window parent, int x, int y,
                     unsigned int width, unsigned int height,
                     unsigned int border_width, int depth, unsigned int class,
                     Visual *visual, unsigned long valuemask,
                     XSetWindowAttributes *attributes) {
    (void)depth; (void)class; (void)visual;
    unsigned long bg = 0xFFFFFF;
    unsigned long border = 0x000000;
    if (attributes) {
        if (valuemask & CWBackPixel) bg = attributes->background_pixel;
        if (valuemask & CWBorderPixel) border = attributes->border_pixel;
    }
    return XCreateSimpleWindow(dpy, parent, x, y, width, height, border_width, border, bg);
}

int XDestroyWindow(Display *dpy, Window w) {
    if (!dpy) return 0;
    struct x11_destroy_window_req req;
    req.hdr.opcode = X_OP_DESTROY_WINDOW;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)w;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XMapWindow(Display *dpy, Window w) {
    if (!dpy) return 0;
    struct x11_map_window_req req;
    req.hdr.opcode = X_OP_MAP_WINDOW;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)w;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XMapRaised(Display *dpy, Window w) {
    if (!dpy) return 0;
    struct x11_map_window_req req;
    req.hdr.opcode = X_OP_MAP_RAISED;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)w;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XUnmapWindow(Display *dpy, Window w) {
    if (!dpy) return 0;
    struct x11_unmap_window_req req;
    req.hdr.opcode = X_OP_UNMAP_WINDOW;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)w;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XMoveWindow(Display *dpy, Window w, int x, int y) {
    if (!dpy) return 0;
    struct x11_move_resize_req req;
    req.hdr.opcode = X_OP_MOVE_WINDOW;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)w;
    req.x = x;
    req.y = y;
    req.width = 0;
    req.height = 0;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XResizeWindow(Display *dpy, Window w, unsigned int width, unsigned int height) {
    if (!dpy) return 0;
    struct x11_move_resize_req req;
    req.hdr.opcode = X_OP_RESIZE_WINDOW;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)w;
    req.x = 0;
    req.y = 0;
    req.width = width;
    req.height = height;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XMoveResizeWindow(Display *dpy, Window w, int x, int y, unsigned int width, unsigned int height) {
    if (!dpy) return 0;
    struct x11_move_resize_req req;
    req.hdr.opcode = X_OP_MOVE_RESIZE_WINDOW;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)w;
    req.x = x;
    req.y = y;
    req.width = width;
    req.height = height;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XSetWindowBorderWidth(Display *dpy, Window w, unsigned int width) {
    if (!dpy) return 0;
    struct {
        struct x11_req_header hdr;
        uint32_t wid;
        uint32_t border_width;
    } req;
    req.hdr.opcode = X_OP_SET_WIN_BORDER_WIDTH;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)w;
    req.border_width = width;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XSetWindowBackground(Display *dpy, Window w, unsigned long background_pixel) {
    if (!dpy) return 0;
    struct {
        struct x11_req_header hdr;
        uint32_t wid;
        uint32_t bg;
    } req;
    req.hdr.opcode = X_OP_SET_WIN_BG;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)w;
    req.bg = (uint32_t)background_pixel;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

GC XCreateGC(Display *dpy, Drawable d, unsigned long valuemask, XGCValues *values) {
    if (!dpy) return NULL;

    GC gc = (GC)malloc(sizeof(struct _XGC));
    if (!gc) return NULL;

    gc->display = dpy;
    gc->gid = dpy->next_resource_id++;
    memset(&gc->values, 0, sizeof(XGCValues));
    gc->values.foreground = 0x000000;
    gc->values.background = 0xFFFFFF;

    if (values) {
        if (valuemask & GCForeground) gc->values.foreground = values->foreground;
        if (valuemask & GCBackground) gc->values.background = values->background;
        if (valuemask & GCLineWidth)  gc->values.line_width = values->line_width;
    }

    struct x11_create_gc_req req;
    req.hdr.opcode = X_OP_CREATE_GC;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.gcontext = (uint32_t)gc->gid;
    req.drawable = (uint32_t)d;
    req.foreground = (uint32_t)gc->values.foreground;
    req.background = (uint32_t)gc->values.background;

    send(dpy->fd, &req, sizeof(req), 0);
    return gc;
}

int XFreeGC(Display *dpy, GC gc) {
    if (!dpy || !gc) return 0;
    struct x11_free_gc_req req;
    req.hdr.opcode = X_OP_FREE_GC;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.gcontext = (uint32_t)gc->gid;
    send(dpy->fd, &req, sizeof(req), 0);
    free(gc);
    return 0;
}

int XSetForeground(Display *dpy, GC gc, unsigned long foreground) {
    if (!dpy || !gc) return 0;
    gc->values.foreground = foreground;
    struct x11_set_gc_color_req req;
    req.hdr.opcode = X_OP_SET_FG;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.gcontext = (uint32_t)gc->gid;
    req.color = (uint32_t)foreground;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XSetBackground(Display *dpy, GC gc, unsigned long background) {
    if (!dpy || !gc) return 0;
    gc->values.background = background;
    struct x11_set_gc_color_req req;
    req.hdr.opcode = X_OP_SET_BG;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.gcontext = (uint32_t)gc->gid;
    req.color = (uint32_t)background;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XSetLineAttributes(Display *dpy, GC gc, unsigned int line_width,
                       int line_style, int cap_style, int join_style) {
    (void)line_style; (void)cap_style; (void)join_style;
    if (!dpy || !gc) return 0;
    gc->values.line_width = line_width;
    struct {
        struct x11_req_header hdr;
        uint32_t gcontext;
        uint32_t line_width;
    } req;
    req.hdr.opcode = X_OP_SET_LINE_WIDTH;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.gcontext = (uint32_t)gc->gid;
    req.line_width = line_width;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XDrawPoint(Display *dpy, Drawable d, GC gc, int x, int y) {
    if (!dpy || !gc) return 0;
    struct x11_draw_point_req req;
    req.hdr.opcode = X_OP_DRAW_POINT;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.drawable = (uint32_t)d;
    req.gcontext = (uint32_t)gc->gid;
    req.x = x;
    req.y = y;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XDrawLine(Display *dpy, Drawable d, GC gc, int x1, int y1, int x2, int y2) {
    if (!dpy || !gc) return 0;
    struct x11_draw_line_req req;
    req.hdr.opcode = X_OP_DRAW_LINE;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.drawable = (uint32_t)d;
    req.gcontext = (uint32_t)gc->gid;
    req.x1 = x1;
    req.y1 = y1;
    req.x2 = x2;
    req.y2 = y2;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XDrawRectangle(Display *dpy, Drawable d, GC gc, int x, int y, unsigned int width, unsigned int height) {
    if (!dpy || !gc) return 0;
    struct x11_draw_rect_req req;
    req.hdr.opcode = X_OP_DRAW_RECT;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.drawable = (uint32_t)d;
    req.gcontext = (uint32_t)gc->gid;
    req.x = x;
    req.y = y;
    req.width = width;
    req.height = height;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XFillRectangle(Display *dpy, Drawable d, GC gc, int x, int y, unsigned int width, unsigned int height) {
    if (!dpy || !gc) return 0;
    struct x11_draw_rect_req req;
    req.hdr.opcode = X_OP_FILL_RECT;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.drawable = (uint32_t)d;
    req.gcontext = (uint32_t)gc->gid;
    req.x = x;
    req.y = y;
    req.width = width;
    req.height = height;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XDrawArc(Display *dpy, Drawable d, GC gc, int x, int y, unsigned int width, unsigned int height, int angle1, int angle2) {
    (void)angle1; (void)angle2;
    if (!dpy || !gc) return 0;
    struct x11_draw_rect_req req;
    req.hdr.opcode = X_OP_DRAW_ARC;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.drawable = (uint32_t)d;
    req.gcontext = (uint32_t)gc->gid;
    req.x = x;
    req.y = y;
    req.width = width;
    req.height = height;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XFillArc(Display *dpy, Drawable d, GC gc, int x, int y, unsigned int width, unsigned int height, int angle1, int angle2) {
    (void)angle1; (void)angle2;
    if (!dpy || !gc) return 0;
    struct x11_draw_rect_req req;
    req.hdr.opcode = X_OP_FILL_ARC;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.drawable = (uint32_t)d;
    req.gcontext = (uint32_t)gc->gid;
    req.x = x;
    req.y = y;
    req.width = width;
    req.height = height;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XDrawString(Display *dpy, Drawable d, GC gc, int x, int y, const char *string, int length) {
    if (!dpy || !gc || !string || length <= 0) return 0;
    struct x11_draw_string_req req;
    memset(&req, 0, sizeof(req));
    req.hdr.opcode = X_OP_DRAW_STRING;
    req.hdr.length = sizeof(req) / 4;
    req.drawable = (uint32_t)d;
    req.gcontext = (uint32_t)gc->gid;
    req.x = x;
    req.y = y;
    req.str_len = (length < 127) ? (uint32_t)length : 127;
    strncpy(req.text, string, req.str_len);
    req.text[req.str_len] = '\0';
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XDrawImageString(Display *dpy, Drawable d, GC gc, int x, int y, const char *string, int length) {
    return XDrawString(dpy, d, gc, x, y, string, length);
}

int XClearWindow(Display *dpy, Window w) {
    if (!dpy) return 0;
    struct {
        struct x11_req_header hdr;
        uint32_t wid;
    } req;
    req.hdr.opcode = X_OP_CLEAR_WINDOW;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)w;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XClearArea(Display *dpy, Window w, int x, int y, unsigned int width, unsigned int height, Bool exposures) {
    (void)exposures;
    if (!dpy) return 0;
    struct x11_draw_rect_req req;
    req.hdr.opcode = X_OP_CLEAR_AREA;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.drawable = (uint32_t)w;
    req.gcontext = 0;
    req.x = x;
    req.y = y;
    req.width = width;
    req.height = height;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XSelectInput(Display *dpy, Window w, long event_mask) {
    if (!dpy) return 0;
    struct x11_select_input_req req;
    req.hdr.opcode = X_OP_SELECT_INPUT;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)w;
    req.event_mask = (uint32_t)event_mask;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XNextEvent(Display *dpy, XEvent *event_return) {
    if (!dpy || !event_return) return 0;
    ssize_t n = recv(dpy->fd, event_return, sizeof(XEvent), 0);
    if (n != (ssize_t)sizeof(XEvent)) {
        memset(event_return, 0, sizeof(XEvent));
        return 0;
    }
    event_return->xany.display = dpy;
    return 0;
}

int XPending(Display *dpy) {
    if (!dpy) return 0;
    /* Try non-blocking peek on socket */
    XEvent ev;
    ssize_t n = recv(dpy->fd, &ev, sizeof(XEvent), MSG_PEEK | MSG_DONTWAIT);
    return (n >= (ssize_t)sizeof(XEvent)) ? 1 : 0;
}

Bool XCheckWindowEvent(Display *dpy, Window w, long event_mask, XEvent *event_return) {
    (void)event_mask;
    if (!XPending(dpy)) return False;
    XNextEvent(dpy, event_return);
    if (event_return->xany.window == w) return True;
    return False;
}

Bool XCheckTypedEvent(Display *dpy, int event_type, XEvent *event_return) {
    if (!XPending(dpy)) return False;
    XNextEvent(dpy, event_return);
    if (event_return->type == event_type) return True;
    return False;
}

Status XSendEvent(Display *dpy, Window w, Bool propagate, long event_mask, XEvent *event_send) {
    (void)propagate;
    if (!dpy || !event_send) return 0;
    struct x11_send_event_req req;
    req.hdr.opcode = X_OP_SEND_EVENT;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)w;
    req.event_mask = (uint32_t)event_mask;
    memcpy(&req.event, event_send, sizeof(XEvent));
    send(dpy->fd, &req, sizeof(req), 0);
    return 1;
}

int XStoreName(Display *dpy, Window w, const char *window_name) {
    if (!dpy || !window_name) return 0;
    struct x11_store_name_req req;
    memset(&req, 0, sizeof(req));
    req.hdr.opcode = X_OP_STORE_NAME;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)w;
    strncpy(req.name, window_name, sizeof(req.name) - 1);
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XFetchName(Display *dpy, Window w, char **window_name_return) {
    (void)dpy; (void)w;
    if (window_name_return) *window_name_return = NULL;
    return 0;
}

Atom XInternAtom(Display *dpy, const char *atom_name, Bool only_if_exists) {
    (void)dpy; (void)only_if_exists;
    if (!atom_name) return None;
    if (strcmp(atom_name, "WM_PROTOCOLS") == 0) return 100;
    if (strcmp(atom_name, "WM_DELETE_WINDOW") == 0) return 101;
    if (strcmp(atom_name, "WM_NAME") == 0) return XA_WM_NAME;
    if (strcmp(atom_name, "WM_CLASS") == 0) return XA_WM_CLASS;
    return 102;
}

Status XSetWMProtocols(Display *display, Window w, Atom *protocols, int count) {
    (void)display; (void)w; (void)protocols; (void)count;
    return 1;
}

void XSetWMNormalHints(Display *display, Window w, XSizeHints *hints) {
    (void)display; (void)w; (void)hints;
}

int XGrabButton(Display *dpy, unsigned int button, unsigned int modifiers,
                Window grab_window, Bool owner_events, unsigned int event_mask,
                int pointer_mode, int keyboard_mode, Window confine_to, Cursor cursor) {
    (void)modifiers; (void)owner_events; (void)pointer_mode; (void)keyboard_mode; (void)confine_to; (void)cursor;
    if (!dpy) return 0;
    struct {
        struct x11_req_header hdr;
        uint32_t wid;
        uint32_t button;
        uint32_t event_mask;
    } req;
    req.hdr.opcode = X_OP_GRAB_BUTTON;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)grab_window;
    req.button = button;
    req.event_mask = event_mask;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XUngrabButton(Display *dpy, unsigned int button, unsigned int modifiers, Window grab_window) {
    (void)modifiers;
    if (!dpy) return 0;
    struct {
        struct x11_req_header hdr;
        uint32_t wid;
        uint32_t button;
    } req;
    req.hdr.opcode = X_OP_UNGRAB_BUTTON;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)grab_window;
    req.button = button;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XGrabPointer(Display *dpy, Window grab_window, Bool owner_events,
                 unsigned int event_mask, int pointer_mode, int keyboard_mode,
                 Window confine_to, Cursor cursor, Time time) {
    (void)owner_events; (void)pointer_mode; (void)keyboard_mode; (void)confine_to; (void)cursor; (void)time;
    if (!dpy) return 0;
    struct {
        struct x11_req_header hdr;
        uint32_t wid;
        uint32_t event_mask;
    } req;
    req.hdr.opcode = X_OP_GRAB_POINTER;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)grab_window;
    req.event_mask = event_mask;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

int XUngrabPointer(Display *dpy, Time time) {
    (void)time;
    if (!dpy) return 0;
    struct x11_req_header req;
    req.opcode = X_OP_UNGRAB_POINTER;
    req.pad = 0;
    req.length = sizeof(req) / 4;
    send(dpy->fd, &req, sizeof(req), 0);
    return 0;
}

Bool XQueryPointer(Display *dpy, Window w, Window *root_return, Window *child_return,
                   int *root_x_return, int *root_y_return, int *win_x_return, int *win_y_return,
                   unsigned int *mask_return) {
    if (!dpy) return False;
    struct x11_query_pointer_req req;
    req.hdr.opcode = X_OP_QUERY_POINTER;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)w;
    send(dpy->fd, &req, sizeof(req), 0);

    struct x11_query_pointer_reply reply;
    if (recv(dpy->fd, &reply, sizeof(reply), 0) != (ssize_t)sizeof(reply)) return False;

    if (root_return) *root_return = reply.root;
    if (child_return) *child_return = reply.child;
    if (root_x_return) *root_x_return = reply.root_x;
    if (root_y_return) *root_y_return = reply.root_y;
    if (win_x_return) *win_x_return = reply.win_x;
    if (win_y_return) *win_y_return = reply.win_y;
    if (mask_return) *mask_return = reply.mask;
    return True;
}

Status XGetWindowAttributes(Display *display, Window w, XWindowAttributes *attr) {
    (void)w;
    if (!display || !attr) return 0;
    memset(attr, 0, sizeof(XWindowAttributes));
    attr->root = display->root_window;
    attr->depth = 32;
    attr->map_state = 1;
    return 1;
}

Status XGetGeometry(Display *dpy, Drawable d, Window *root_return,
                    int *x_return, int *y_return, unsigned int *width_return,
                    unsigned int *height_return, unsigned int *border_width_return,
                    unsigned int *depth_return) {
    if (!dpy) return 0;
    struct x11_get_geometry_req req;
    req.hdr.opcode = X_OP_GET_GEOMETRY;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.drawable = (uint32_t)d;
    send(dpy->fd, &req, sizeof(req), 0);

    struct x11_get_geometry_reply reply;
    if (recv(dpy->fd, &reply, sizeof(reply), 0) != (ssize_t)sizeof(reply)) return 0;

    if (root_return) *root_return = reply.root;
    if (x_return) *x_return = reply.x;
    if (y_return) *y_return = reply.y;
    if (width_return) *width_return = reply.width;
    if (height_return) *height_return = reply.height;
    if (border_width_return) *border_width_return = reply.border_width;
    if (depth_return) *depth_return = reply.depth;
    return 1;
}

Status XQueryTree(Display *dpy, Window w, Window *root_return, Window *parent_return,
                  Window **children_return, unsigned int *nchildren_return) {
    if (!dpy) return 0;
    struct x11_query_tree_req req;
    req.hdr.opcode = X_OP_QUERY_TREE;
    req.hdr.pad = 0;
    req.hdr.length = sizeof(req) / 4;
    req.wid = (uint32_t)w;
    send(dpy->fd, &req, sizeof(req), 0);

    struct x11_query_tree_reply reply;
    if (recv(dpy->fd, &reply, sizeof(reply), 0) != (ssize_t)sizeof(reply)) return 0;

    if (root_return) *root_return = reply.root;
    if (parent_return) *parent_return = reply.parent;
    if (nchildren_return) *nchildren_return = reply.nchildren;
    if (children_return && reply.nchildren > 0) {
        Window *list = (Window *)malloc(sizeof(Window) * reply.nchildren);
        for (unsigned int i = 0; i < reply.nchildren; i++) {
            list[i] = reply.children[i];
        }
        *children_return = list;
    }
    return 1;
}

int XLookupString(XKeyEvent *event_struct, char *buffer_return, int bytes_buffer,
                  KeySym *keysym_return, void *status_in_out) {
    (void)status_in_out;
    if (!event_struct || !buffer_return || bytes_buffer <= 0) return 0;
    char c = (char)(event_struct->keycode & 0xFF);
    buffer_return[0] = c;
    if (bytes_buffer > 1) buffer_return[1] = '\0';
    if (keysym_return) *keysym_return = (KeySym)c;
    return 1;
}
