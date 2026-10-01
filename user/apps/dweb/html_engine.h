#ifndef _DWEB_HTML_ENGINE_H
#define _DWEB_HTML_ENGINE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <dui/dui.h>

struct link_entry {
    int x1, y1, x2, y2;
    char url[256];
};

struct text_box {
    int x, y;
    char text[128];
    uint32_t color;
    bool is_bold;
    bool is_link;
    bool is_code;
    int link_index; /* -1 if not a link */
};

struct line_box {
    int x1, y1, x2, y2;
    uint32_t color;
};

struct rect_box {
    int x, y, w, h;
    uint32_t fill_color;
    uint32_t border_color;
};

struct layout_document {
    int content_height;
    char title[64];

    struct text_box *texts;
    int text_count;
    int text_cap;

    struct line_box *lines;
    int line_count;
    int line_cap;

    struct rect_box *rects;
    int rect_count;
    int rect_cap;

    struct link_entry *links;
    int link_count;
    int link_cap;
};

/* Create and initialize layout document */
struct layout_document *layout_doc_create(void);

/* Free layout document resources */
void layout_doc_free(struct layout_document *doc);

/* Parse and compute layout for HTML text given viewport width */
void html_layout(struct layout_document *doc, const char *html, int view_w);

/* Render document to DUI window offset by scroll_y */
void html_render_dui(DuiConnection *conn,
                     DuiWindow win,
                     struct layout_document *doc,
                     int view_x,
                     int view_y,
                     int view_w,
                     int view_h,
                     int scroll_y,
                     int hovered_link_idx);

/* Check if mouse coordinate (mx, my) hits a link */
int html_find_link_at(struct layout_document *doc, int mx, int my, int scroll_y);

/* Terminal text mode fallback renderer */
void html_render_terminal(const char *html);

#endif /* _DWEB_HTML_ENGINE_H */
