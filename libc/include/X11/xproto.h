#ifndef _X11_XPROTO_H
#define _X11_XPROTO_H

#include <stdint.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#define X11_TCP_PORT 6000
#define X11_UNIX_PATH "/tmp/.X11-unix/X0"

#define X_OP_CONNECT               1
#define X_OP_CREATE_WINDOW         2
#define X_OP_DESTROY_WINDOW        3
#define X_OP_MAP_WINDOW            4
#define X_OP_UNMAP_WINDOW          5
#define X_OP_MOVE_WINDOW           6
#define X_OP_RESIZE_WINDOW         7
#define X_OP_MOVE_RESIZE_WINDOW    8
#define X_OP_CREATE_GC             9
#define X_OP_FREE_GC               10
#define X_OP_SET_FG                11
#define X_OP_SET_BG                12
#define X_OP_SET_LINE_WIDTH        13
#define X_OP_DRAW_POINT            14
#define X_OP_DRAW_LINE             15
#define X_OP_DRAW_RECT             16
#define X_OP_FILL_RECT             17
#define X_OP_DRAW_ARC              18
#define X_OP_FILL_ARC              19
#define X_OP_DRAW_STRING           20
#define X_OP_CLEAR_WINDOW          21
#define X_OP_CLEAR_AREA            22
#define X_OP_SELECT_INPUT          23
#define X_OP_GET_EVENT             24
#define X_OP_STORE_NAME            25
#define X_OP_FETCH_NAME            26
#define X_OP_GET_GEOMETRY          27
#define X_OP_GET_ATTRIBUTES        28
#define X_OP_QUERY_POINTER         29
#define X_OP_QUERY_TREE            30
#define X_OP_SEND_EVENT            31
#define X_OP_MAP_RAISED            32
#define X_OP_SET_WIN_BORDER_WIDTH  33
#define X_OP_SET_WIN_BG            34
#define X_OP_GRAB_BUTTON           35
#define X_OP_UNGRAB_BUTTON         36
#define X_OP_GRAB_POINTER          37
#define X_OP_UNGRAB_POINTER        38
#define X_OP_SYNC                  39

struct __attribute__((packed)) x11_req_header {
    uint8_t opcode;
    uint8_t pad;
    uint16_t length; /* Total request length in 4-byte units */
};

struct __attribute__((packed)) x11_connect_req {
    struct x11_req_header hdr;
    uint16_t major_version;
    uint16_t minor_version;
};

struct __attribute__((packed)) x11_connect_reply {
    uint8_t status; /* 1 = Success */
    uint8_t pad;
    uint16_t major_version;
    uint16_t minor_version;
    uint32_t root_window;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t white_pixel;
    uint32_t black_pixel;
    uint32_t resource_id_base;
};

struct __attribute__((packed)) x11_create_window_req {
    struct x11_req_header hdr;
    uint32_t wid;
    uint32_t parent;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t border_width;
    uint32_t border_pixel;
    uint32_t background_pixel;
};

struct __attribute__((packed)) x11_map_window_req {
    struct x11_req_header hdr;
    uint32_t wid;
};

struct __attribute__((packed)) x11_unmap_window_req {
    struct x11_req_header hdr;
    uint32_t wid;
};

struct __attribute__((packed)) x11_destroy_window_req {
    struct x11_req_header hdr;
    uint32_t wid;
};

struct __attribute__((packed)) x11_move_resize_req {
    struct x11_req_header hdr;
    uint32_t wid;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
};

struct __attribute__((packed)) x11_create_gc_req {
    struct x11_req_header hdr;
    uint32_t gcontext;
    uint32_t drawable;
    uint32_t foreground;
    uint32_t background;
};

struct __attribute__((packed)) x11_free_gc_req {
    struct x11_req_header hdr;
    uint32_t gcontext;
};

struct __attribute__((packed)) x11_set_gc_color_req {
    struct x11_req_header hdr;
    uint32_t gcontext;
    uint32_t color;
};

struct __attribute__((packed)) x11_draw_point_req {
    struct x11_req_header hdr;
    uint32_t drawable;
    uint32_t gcontext;
    int32_t x;
    int32_t y;
};

struct __attribute__((packed)) x11_draw_line_req {
    struct x11_req_header hdr;
    uint32_t drawable;
    uint32_t gcontext;
    int32_t x1;
    int32_t y1;
    int32_t x2;
    int32_t y2;
};

struct __attribute__((packed)) x11_draw_rect_req {
    struct x11_req_header hdr;
    uint32_t drawable;
    uint32_t gcontext;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
};

struct __attribute__((packed)) x11_draw_string_req {
    struct x11_req_header hdr;
    uint32_t drawable;
    uint32_t gcontext;
    int32_t x;
    int32_t y;
    uint32_t str_len;
    char text[128];
};

struct __attribute__((packed)) x11_select_input_req {
    struct x11_req_header hdr;
    uint32_t wid;
    uint32_t event_mask;
};

struct __attribute__((packed)) x11_store_name_req {
    struct x11_req_header hdr;
    uint32_t wid;
    char name[64];
};

struct __attribute__((packed)) x11_get_geometry_req {
    struct x11_req_header hdr;
    uint32_t drawable;
};

struct __attribute__((packed)) x11_get_geometry_reply {
    uint32_t root;
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t border_width;
    uint32_t depth;
};

struct __attribute__((packed)) x11_query_pointer_req {
    struct x11_req_header hdr;
    uint32_t wid;
};

struct __attribute__((packed)) x11_query_pointer_reply {
    uint32_t root;
    uint32_t child;
    int32_t root_x;
    int32_t root_y;
    int32_t win_x;
    int32_t win_y;
    uint32_t mask;
};

struct __attribute__((packed)) x11_query_tree_req {
    struct x11_req_header hdr;
    uint32_t wid;
};

struct __attribute__((packed)) x11_query_tree_reply {
    uint32_t root;
    uint32_t parent;
    uint32_t nchildren;
    uint32_t children[32];
};

struct __attribute__((packed)) x11_send_event_req {
    struct x11_req_header hdr;
    uint32_t wid;
    uint32_t event_mask;
    XEvent event;
};

#endif /* _X11_XPROTO_H */
