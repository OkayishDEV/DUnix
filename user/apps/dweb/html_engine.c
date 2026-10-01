#include "html_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define COLOR_TEXT        0x001E293B  /* Slate 800 */
#define COLOR_H1          0x001E3A8A  /* Blue 900 */
#define COLOR_H2          0x000F766E  /* Teal 700 */
#define COLOR_H3          0x000369A1  /* Sky 700 */
#define COLOR_LINK        0x001D4ED8  /* Blue 700 */
#define COLOR_LINK_HOVER  0x000284C7  /* Light Blue 600 */
#define COLOR_CODE_BG     0x00F1F5F9  /* Slate 100 */
#define COLOR_CODE_BORDER 0x00CBD5E1  /* Slate 300 */
#define COLOR_CODE_TXT    0x000F172A  /* Slate 900 */
#define COLOR_HR          0x00CBD5E1  /* Slate 300 */
#define COLOR_BLOCKQUOTE  0x003B82F6  /* Blue 500 */
#define COLOR_MUTED       0x0064748B  /* Slate 500 */
#define COLOR_TABLE_BG    0x00F8FAFC
#define COLOR_TABLE_HDR   0x00E2E8F0
#define COLOR_HTML_BTN_BG      0x00E2E8F0
#define COLOR_HTML_BTN_BORDER  0x0094A3B8

struct text_span {
    char text[128];
    int start_x;
    int y;
    uint32_t color;
    bool is_bold;
    bool is_link;
    bool is_code;
    char link_url[256];
};

struct layout_document *layout_doc_create(void) {
    struct layout_document *doc = (struct layout_document *)malloc(sizeof(struct layout_document));
    if (!doc) return NULL;
    memset(doc, 0, sizeof(struct layout_document));

    doc->text_cap = 1024;
    doc->texts = (struct text_box *)malloc(sizeof(struct text_box) * doc->text_cap);

    doc->line_cap = 512;
    doc->lines = (struct line_box *)malloc(sizeof(struct line_box) * doc->line_cap);

    doc->rect_cap = 512;
    doc->rects = (struct rect_box *)malloc(sizeof(struct rect_box) * doc->rect_cap);

    doc->link_cap = 256;
    doc->links = (struct link_entry *)malloc(sizeof(struct link_entry) * doc->link_cap);

    strcpy(doc->title, "DWeb Navigator");
    return doc;
}

void layout_doc_free(struct layout_document *doc) {
    if (!doc) return;
    if (doc->texts) free(doc->texts);
    if (doc->lines) free(doc->lines);
    if (doc->rects) free(doc->rects);
    if (doc->links) free(doc->links);
    free(doc);
}

static void doc_add_text(struct layout_document *doc, int x, int y, const char *txt, uint32_t color, bool bold, bool link, bool code, int link_idx) {
    if (!doc || !txt || txt[0] == '\0') return;
    if (doc->text_count >= doc->text_cap) {
        doc->text_cap *= 2;
        doc->texts = (struct text_box *)realloc(doc->texts, sizeof(struct text_box) * doc->text_cap);
        if (!doc->texts) return;
    }
    struct text_box *b = &doc->texts[doc->text_count++];
    b->x = x;
    b->y = y;
    strncpy(b->text, txt, sizeof(b->text) - 1);
    b->text[sizeof(b->text) - 1] = '\0';
    b->color = color;
    b->is_bold = bold;
    b->is_link = link;
    b->is_code = code;
    b->link_index = link_idx;
}

static void doc_add_line(struct layout_document *doc, int x1, int y1, int x2, int y2, uint32_t color) {
    if (!doc) return;
    if (doc->line_count >= doc->line_cap) {
        doc->line_cap *= 2;
        doc->lines = (struct line_box *)realloc(doc->lines, sizeof(struct line_box) * doc->line_cap);
        if (!doc->lines) return;
    }
    struct line_box *l = &doc->lines[doc->line_count++];
    l->x1 = x1;
    l->y1 = y1;
    l->x2 = x2;
    l->y2 = y2;
    l->color = color;
}

static void doc_add_rect(struct layout_document *doc, int x, int y, int w, int h, uint32_t fill_color, uint32_t border_color) {
    if (!doc) return;
    if (doc->rect_count >= doc->rect_cap) {
        doc->rect_cap *= 2;
        doc->rects = (struct rect_box *)realloc(doc->rects, sizeof(struct rect_box) * doc->rect_cap);
        if (!doc->rects) return;
    }
    struct rect_box *r = &doc->rects[doc->rect_count++];
    r->x = x;
    r->y = y;
    r->w = w;
    r->h = h;
    r->fill_color = fill_color;
    r->border_color = border_color;
}

static int doc_add_link(struct layout_document *doc, int x1, int y1, int x2, int y2, const char *url) {
    if (!doc || !url) return -1;
    if (doc->link_count >= doc->link_cap) {
        doc->link_cap *= 2;
        doc->links = (struct link_entry *)realloc(doc->links, sizeof(struct link_entry) * doc->link_cap);
        if (!doc->links) return -1;
    }
    int idx = doc->link_count++;
    struct link_entry *l = &doc->links[idx];
    l->x1 = x1;
    l->y1 = y1;
    l->x2 = x2;
    l->y2 = y2;
    strncpy(l->url, url, sizeof(l->url) - 1);
    l->url[sizeof(l->url) - 1] = '\0';
    return idx;
}

static void decode_entities(char *s) {
    if (!s) return;
    char *p = s;
    char *out = s;
    while (*p) {
        if (*p == '&') {
            if (strncmp(p, "&amp;", 5) == 0) { *out++ = '&'; p += 5; }
            else if (strncmp(p, "&lt;", 4) == 0) { *out++ = '<'; p += 4; }
            else if (strncmp(p, "&gt;", 4) == 0) { *out++ = '>'; p += 4; }
            else if (strncmp(p, "&quot;", 6) == 0) { *out++ = '"'; p += 6; }
            else if (strncmp(p, "&#39;", 5) == 0) { *out++ = '\''; p += 5; }
            else if (strncmp(p, "&nbsp;", 6) == 0) { *out++ = ' '; p += 6; }
            else if (strncmp(p, "&copy;", 6) == 0) { *out++ = '('; *out++ = 'c'; *out++ = ')'; p += 6; }
            else { *out++ = *p++; }
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';
}

static uint32_t parse_color_hex(const char *s) {
    if (!s) return COLOR_TEXT;
    while (*s == ' ' || *s == '"' || *s == '\'') s++;
    if (*s == '#') s++;
    if (strlen(s) >= 6) {
        uint32_t c = 0;
        for (int i = 0; i < 6; i++) {
            char ch = s[i];
            int val = 0;
            if (ch >= '0' && ch <= '9') val = ch - '0';
            else if (ch >= 'a' && ch <= 'f') val = ch - 'a' + 10;
            else if (ch >= 'A' && ch <= 'F') val = ch - 'A' + 10;
            else break;
            c = (c << 4) | (uint32_t)val;
        }
        return c;
    }
    if (strcasecmp(s, "red") == 0) return 0x00EA4335;
    if (strcasecmp(s, "blue") == 0) return 0x004285F4;
    if (strcasecmp(s, "green") == 0) return 0x0034A853;
    if (strcasecmp(s, "yellow") == 0) return 0x00FBBC05;
    return COLOR_TEXT;
}

static void flush_span(struct layout_document *doc, struct text_span *span, int end_x, int line_h) {
    if (!span || span->text[0] == '\0') return;

    int link_idx = -1;
    if (span->is_link && span->link_url[0]) {
        link_idx = doc_add_link(doc, span->start_x, span->y, end_x, span->y + line_h, span->link_url);
        doc_add_line(doc, span->start_x, span->y + 14, end_x, span->y + 14, COLOR_LINK);
    }
    if (span->is_code) {
        doc_add_rect(doc, span->start_x - 2, span->y - 1, (end_x - span->start_x) + 4, line_h - 2, COLOR_CODE_BG, COLOR_CODE_BORDER);
    }

    doc_add_text(doc, span->start_x, span->y, span->text, span->color, span->is_bold, span->is_link, span->is_code, link_idx);
    span->text[0] = '\0';
}

void html_layout(struct layout_document *doc, const char *html, int view_w) {
    if (!doc || !html) return;

    doc->text_count = 0;
    doc->line_count = 0;
    doc->rect_count = 0;
    doc->link_count = 0;
    doc->content_height = 0;

    int left_margin = 24;
    int right_margin = 28;
    int max_x = view_w - right_margin;
    if (max_x < 200) max_x = 200;

    int cur_x = left_margin;
    int cur_y = 16;
    int line_h = 18;

    /* Style state */
    bool is_bold = false;
    bool is_italic = false;
    bool is_underline = false;
    bool is_code = false;
    bool is_pre = false;
    bool in_link = false;
    char current_href[256] = "";

    bool in_h1 = false, in_h2 = false, in_h3 = false;
    int list_counter = 0;
    bool in_ol = false;
    bool in_blockquote = false;
    int bq_start_y = 0;

    bool in_table = false;
    int table_col = 0;
    int table_row_y = 0;
    int col_width = (max_x - left_margin) / 4;
    if (col_width < 100) col_width = 100;

    (void)is_italic;
    (void)is_underline;
    (void)in_table;

    struct text_span cur_span;
    memset(&cur_span, 0, sizeof(cur_span));
    bool pending_space = false;
    bool in_font = false;
    uint32_t font_color = COLOR_TEXT;

    const char *p = html;

    while (*p) {
        if (*p == '<') {
            /* Check for comment <!-- ... --> */
            if (strncmp(p, "<!--", 4) == 0) {
                const char *end_cmt = strstr(p, "-->");
                if (end_cmt) p = end_cmt + 3;
                p++;
                continue;
            }

            /* Tag processing */
            p++;
            bool is_closing = false;
            if (*p == '/') {
                is_closing = true;
                p++;
            }

            char tag_name[32];
            int tidx = 0;
            while (*p && !isspace((unsigned char)*p) && *p != '>' && *p != '/' && tidx < 31) {
                tag_name[tidx++] = (char)tolower((unsigned char)*p);
                p++;
            }
            tag_name[tidx] = '\0';

            /* Parse attributes */
            char attr_href[256] = "";
            char attr_value[128] = "";
            char attr_color[32] = "";
            char attr_type[32] = "";
            char attr_alt[128] = "";
            char attr_placeholder[128] = "";
            char attr_title[128] = "";
            while (*p && *p != '>') {
                while (*p && isspace((unsigned char)*p)) p++;
                if (*p == '>') break;

                char aname[32];
                int aidx = 0;
                while (*p && !isspace((unsigned char)*p) && *p != '=' && *p != '>' && aidx < 31) {
                    aname[aidx++] = (char)tolower((unsigned char)*p);
                    p++;
                }
                aname[aidx] = '\0';

                while (*p && isspace((unsigned char)*p)) p++;
                if (*p == '=') {
                    p++;
                    while (*p && isspace((unsigned char)*p)) p++;
                    char quote = 0;
                    if (*p == '"' || *p == '\'') {
                        quote = *p++;
                    }
                    char aval[256];
                    int vidx = 0;
                    while (*p && vidx < 255) {
                        if (quote && *p == quote) { p++; break; }
                        if (!quote && (isspace((unsigned char)*p) || *p == '>')) break;
                        aval[vidx++] = *p++;
                    }
                    aval[vidx] = '\0';

                    if (strcmp(aname, "href") == 0) {
                        strncpy(attr_href, aval, sizeof(attr_href) - 1);
                    } else if (strcmp(aname, "value") == 0) {
                        strncpy(attr_value, aval, sizeof(attr_value) - 1);
                    } else if (strcmp(aname, "color") == 0) {
                        strncpy(attr_color, aval, sizeof(attr_color) - 1);
                    } else if (strcmp(aname, "type") == 0) {
                        strncpy(attr_type, aval, sizeof(attr_type) - 1);
                    } else if (strcmp(aname, "alt") == 0) {
                        strncpy(attr_alt, aval, sizeof(attr_alt) - 1);
                    } else if (strcmp(aname, "placeholder") == 0) {
                        strncpy(attr_placeholder, aval, sizeof(attr_placeholder) - 1);
                    } else if (strcmp(aname, "title") == 0) {
                        strncpy(attr_title, aval, sizeof(attr_title) - 1);
                    }
                }
            }
            if (*p == '>') p++;

            /* Flush current span before layout changes */
            flush_span(doc, &cur_span, cur_x, line_h);

            /* Skip non-display script and style tag bodies */
            if (strcmp(tag_name, "script") == 0) {
                if (!is_closing) {
                    const char *s = p;
                    while (*s) {
                        if (*s == '<' && strncasecmp(s, "</script>", 9) == 0) {
                            p = s + 9;
                            break;
                        }
                        s++;
                    }
                    if (!*s) p = s;
                }
                continue;
            }
            if (strcmp(tag_name, "style") == 0) {
                if (!is_closing) {
                    const char *s = p;
                    while (*s) {
                        if (*s == '<' && strncasecmp(s, "</style>", 8) == 0) {
                            p = s + 8;
                            break;
                        }
                        s++;
                    }
                    if (!*s) p = s;
                }
                continue;
            }
            if (strcmp(tag_name, "noscript") == 0) {
                if (!is_closing) {
                    const char *s = p;
                    while (*s) {
                        if (*s == '<' && strncasecmp(s, "</noscript>", 11) == 0) {
                            p = s + 11;
                            break;
                        }
                        s++;
                    }
                    if (!*s) p = s;
                }
                continue;
            }
            if (strcmp(tag_name, "svg") == 0) {
                if (!is_closing) {
                    const char *s = p;
                    while (*s) {
                        if (*s == '<' && strncasecmp(s, "</svg>", 6) == 0) {
                            p = s + 6;
                            break;
                        }
                        s++;
                    }
                    if (!*s) p = s;
                }
                continue;
            }

            /* Handle tags */
            if (strcmp(tag_name, "title") == 0) {
                if (!is_closing) {
                    const char *tend = strstr(p, "</title>");
                    if (!tend) tend = strstr(p, "</TITLE>");
                    if (tend) {
                        size_t tlen = (size_t)(tend - p);
                        if (tlen >= sizeof(doc->title)) tlen = sizeof(doc->title) - 1;
                        strncpy(doc->title, p, tlen);
                        doc->title[tlen] = '\0';
                        decode_entities(doc->title);
                        p = tend + 8;
                    }
                }
            } else if (strcmp(tag_name, "h1") == 0) {
                if (!is_closing) {
                    if (cur_x > left_margin) { cur_y += line_h; cur_x = left_margin; }
                    cur_y += 18;
                    in_h1 = true;
                    is_bold = true;
                } else {
                    cur_y += 20;
                    doc_add_line(doc, left_margin, cur_y, max_x, cur_y, COLOR_HR);
                    cur_y += 14;
                    cur_x = left_margin;
                    in_h1 = false;
                    is_bold = false;
                }
            } else if (strcmp(tag_name, "h2") == 0) {
                if (!is_closing) {
                    if (cur_x > left_margin) { cur_y += line_h; cur_x = left_margin; }
                    cur_y += 16;
                    in_h2 = true;
                    is_bold = true;
                } else {
                    cur_y += 18;
                    cur_x = left_margin;
                    in_h2 = false;
                    is_bold = false;
                }
            } else if (strcmp(tag_name, "h3") == 0) {
                if (!is_closing) {
                    if (cur_x > left_margin) { cur_y += line_h; cur_x = left_margin; }
                    cur_y += 12;
                    in_h3 = true;
                    is_bold = true;
                } else {
                    cur_y += 18;
                    cur_x = left_margin;
                    in_h3 = false;
                    is_bold = false;
                }
            } else if (strcmp(tag_name, "p") == 0) {
                if (!is_closing) {
                    if (cur_x > left_margin) { cur_y += line_h; cur_x = left_margin; }
                    cur_y += 8;
                } else {
                    cur_y += line_h + 8;
                    cur_x = left_margin;
                }
            } else if (strcmp(tag_name, "br") == 0) {
                cur_y += line_h;
                cur_x = left_margin;
            } else if (strcmp(tag_name, "hr") == 0) {
                if (cur_x > left_margin) cur_y += line_h;
                cur_y += 10;
                doc_add_line(doc, left_margin, cur_y, max_x, cur_y, COLOR_HR);
                cur_y += 12;
                cur_x = left_margin;
            } else if (strcmp(tag_name, "b") == 0 || strcmp(tag_name, "strong") == 0) {
                is_bold = !is_closing;
            } else if (strcmp(tag_name, "i") == 0 || strcmp(tag_name, "em") == 0) {
                is_italic = !is_closing;
            } else if (strcmp(tag_name, "u") == 0) {
                is_underline = !is_closing;
            } else if (strcmp(tag_name, "code") == 0 || strcmp(tag_name, "tt") == 0) {
                is_code = !is_closing;
            } else if (strcmp(tag_name, "font") == 0) {
                if (!is_closing && attr_color[0]) {
                    in_font = true;
                    font_color = parse_color_hex(attr_color);
                } else if (is_closing) {
                    in_font = false;
                }
            } else if (strcmp(tag_name, "pre") == 0) {
                if (!is_closing) {
                    if (cur_x > left_margin) { cur_y += line_h; cur_x = left_margin; }
                    cur_y += 8;
                    is_pre = true;
                } else {
                    cur_y += line_h + 8;
                    cur_x = left_margin;
                    is_pre = false;
                }
            } else if (strcmp(tag_name, "a") == 0) {
                if (!is_closing) {
                    in_link = true;
                    strncpy(current_href, attr_href, sizeof(current_href) - 1);
                    current_href[sizeof(current_href) - 1] = '\0';
                } else {
                    in_link = false;
                    current_href[0] = '\0';
                }
            } else if (strcmp(tag_name, "ul") == 0 || strcmp(tag_name, "ol") == 0) {
                if (!is_closing) {
                    if (cur_x > left_margin) { cur_y += line_h; cur_x = left_margin; }
                    cur_y += 6;
                    in_ol = (strcmp(tag_name, "ol") == 0);
                    list_counter = 1;
                    left_margin += 24;
                    cur_x = left_margin;
                } else {
                    left_margin -= 24;
                    cur_x = left_margin;
                    cur_y += 8;
                }
            } else if (strcmp(tag_name, "li") == 0) {
                if (!is_closing) {
                    if (cur_x > left_margin) { cur_y += line_h; cur_x = left_margin; }
                    char bullet[16];
                    if (in_ol) snprintf(bullet, sizeof(bullet), "%d.", list_counter++);
                    else strcpy(bullet, "*");
                    doc_add_text(doc, left_margin - 16, cur_y, bullet, COLOR_MUTED, true, false, false, -1);
                } else {
                    cur_y += line_h;
                    cur_x = left_margin;
                }
            } else if (strcmp(tag_name, "blockquote") == 0) {
                if (!is_closing) {
                    if (cur_x > left_margin) { cur_y += line_h; cur_x = left_margin; }
                    cur_y += 10;
                    bq_start_y = cur_y;
                    left_margin += 32;
                    cur_x = left_margin;
                    in_blockquote = true;
                } else {
                    doc_add_line(doc, left_margin - 16, bq_start_y, left_margin - 16, cur_y + 16, COLOR_BLOCKQUOTE);
                    doc_add_line(doc, left_margin - 15, bq_start_y, left_margin - 15, cur_y + 16, COLOR_BLOCKQUOTE);
                    left_margin -= 32;
                    cur_x = left_margin;
                    cur_y += line_h + 8;
                    in_blockquote = false;
                }
            } else if (strcmp(tag_name, "table") == 0) {
                if (!is_closing) {
                    if (cur_x > left_margin) { cur_y += line_h; cur_x = left_margin; }
                    cur_y += 12;
                    in_table = true;
                } else {
                    cur_y += line_h + 10;
                    cur_x = left_margin;
                    in_table = false;
                }
            } else if (strcmp(tag_name, "tr") == 0) {
                if (!is_closing) {
                    table_col = 0;
                    table_row_y = cur_y;
                    cur_x = left_margin;
                } else {
                    cur_y += line_h + 4;
                    cur_x = left_margin;
                }
            } else if (strcmp(tag_name, "th") == 0 || strcmp(tag_name, "td") == 0) {
                if (!is_closing) {
                    cur_x = left_margin + table_col * col_width;
                    cur_y = table_row_y;
                    is_bold = (strcmp(tag_name, "th") == 0);
                    if (is_bold) {
                        doc_add_rect(doc, cur_x - 2, cur_y - 2, col_width, line_h + 2, COLOR_TABLE_HDR, COLOR_HR);
                    }
                } else {
                    table_col++;
                    is_bold = false;
                }
            } else if (strcmp(tag_name, "input") == 0) {
                if (!is_closing) {
                    if (strcmp(attr_type, "hidden") == 0) {
                        /* Hidden form field - skip */
                    } else if (strcmp(attr_type, "submit") == 0 || strcmp(attr_type, "button") == 0) {
                        const char *btn_txt = attr_value[0] ? attr_value : "Submit";
                        int bw = (int)strlen(btn_txt) * 8 + 16;
                        doc_add_rect(doc, cur_x, cur_y - 2, bw, 22, COLOR_HTML_BTN_BG, COLOR_HTML_BTN_BORDER);
                        doc_add_text(doc, cur_x + 8, cur_y + 1, btn_txt, COLOR_TEXT, true, false, false, -1);
                        cur_x += bw + 8;
                    } else {
                        const char *val_txt = attr_value[0] ? attr_value : (attr_placeholder[0] ? attr_placeholder : attr_title);
                        int bw = 280;
                        doc_add_rect(doc, cur_x, cur_y - 2, bw, 22, 0x00FFFFFF, COLOR_HTML_BTN_BORDER);
                        if (val_txt[0]) {
                            doc_add_text(doc, cur_x + 8, cur_y + 1, val_txt, COLOR_MUTED, false, false, false, -1);
                        }
                        cur_x += bw + 8;
                    }
                }
            } else if (strcmp(tag_name, "button") == 0) {
                if (!is_closing) {
                    const char *btn_txt = attr_value[0] ? attr_value : "Button";
                    const char *bend = strstr(p, "</button>");
                    if (!bend) bend = strstr(p, "</BUTTON>");
                    char binner[64] = "";
                    if (bend) {
                        size_t blen = (size_t)(bend - p);
                        if (blen >= sizeof(binner)) blen = sizeof(binner) - 1;
                        strncpy(binner, p, blen);
                        binner[blen] = '\0';
                        decode_entities(binner);
                        trim_str(binner);
                        if (binner[0]) btn_txt = binner;
                        p = bend + 9;
                    }
                    int bw = (int)strlen(btn_txt) * 8 + 16;
                    doc_add_rect(doc, cur_x, cur_y - 2, bw, 22, COLOR_HTML_BTN_BG, COLOR_HTML_BTN_BORDER);
                    doc_add_text(doc, cur_x + 8, cur_y + 1, btn_txt, COLOR_TEXT, true, false, false, -1);
                    cur_x += bw + 8;
                }
            } else if (strcmp(tag_name, "img") == 0) {
                char img_label[64] = "[Image]";
                if (attr_alt[0]) {
                    snprintf(img_label, sizeof(img_label), "[%s]", attr_alt);
                }
                int iw = (int)strlen(img_label) * 8 + 12;
                doc_add_rect(doc, cur_x, cur_y - 2, iw, 20, COLOR_HTML_BTN_BG, COLOR_HTML_BTN_BORDER);
                doc_add_text(doc, cur_x + 6, cur_y + 1, img_label, in_link ? COLOR_LINK : COLOR_MUTED, false, false, false, in_link ? (int)doc->link_count - 1 : -1);
                cur_x += iw + 8;
            } else if (strcmp(tag_name, "div") == 0 || strcmp(tag_name, "section") == 0 ||
                       strcmp(tag_name, "article") == 0 || strcmp(tag_name, "header") == 0 ||
                       strcmp(tag_name, "footer") == 0 || strcmp(tag_name, "nav") == 0 ||
                       strcmp(tag_name, "aside") == 0 || strcmp(tag_name, "form") == 0 ||
                       strcmp(tag_name, "center") == 0) {
                if (cur_x > left_margin) {
                    cur_y += line_h;
                    cur_x = left_margin;
                }
            }

            continue;
        }

        /* Preformatted mode: preserve whitespace and newlines */
        if (is_pre) {
            char line_buf[128];
            int lidx = 0;
            while (*p && *p != '<' && *p != '\n' && lidx < 120) {
                line_buf[lidx++] = *p++;
            }
            line_buf[lidx] = '\0';
            decode_entities(line_buf);

            if (lidx > 0) {
                doc_add_text(doc, cur_x, cur_y, line_buf, COLOR_CODE_TXT, false, false, true, -1);
            }
            if (*p == '\n') {
                cur_y += line_h;
                cur_x = left_margin;
                p++;
            }
            continue;
        }

        /* Handle whitespace in non-pre mode */
        if (isspace((unsigned char)*p)) {
            while (*p && isspace((unsigned char)*p)) p++;
            if (cur_x > left_margin) {
                pending_space = true;
            }
            continue;
        }

        if (cur_x == left_margin) {
            pending_space = false;
        }

        /* Collect next word */
        char word[128];
        int widx = 0;
        while (*p && !isspace((unsigned char)*p) && *p != '<' && widx < 120) {
            word[widx++] = *p++;
        }
        word[widx] = '\0';
        decode_entities(word);

        if (widx > 0) {
            uint32_t color = COLOR_TEXT;
            if (in_link) color = COLOR_LINK;
            else if (in_font) color = font_color;
            else if (in_h1) color = COLOR_H1;
            else if (in_h2) color = COLOR_H2;
            else if (in_h3) color = COLOR_H3;
            else if (in_blockquote) color = COLOR_MUTED;
            else if (is_code) color = COLOR_CODE_TXT;

            int word_len = (int)strlen(word);
            int word_w = word_len * 8;
            int space_w = (pending_space && cur_x > left_margin) ? 8 : 0;

            /* Check if word fits on current line */
            bool need_wrap = (cur_x + space_w + word_w > max_x && cur_x > left_margin);
            bool style_changed = (cur_span.text[0] != '\0') &&
                                 (cur_span.color != color ||
                                  cur_span.is_bold != is_bold ||
                                  cur_span.is_link != in_link ||
                                  cur_span.is_code != is_code ||
                                  (in_link && strcmp(cur_span.link_url, current_href) != 0));

            if (need_wrap || style_changed) {
                flush_span(doc, &cur_span, cur_x, line_h);
                if (need_wrap) {
                    cur_x = left_margin;
                    cur_y += line_h;
                    pending_space = false;
                }
            }

            if (cur_span.text[0] == '\0') {
                if (pending_space && cur_x > left_margin) {
                    cur_x += 8;
                }
                cur_span.start_x = cur_x;
                cur_span.y = cur_y;
                cur_span.color = color;
                cur_span.is_bold = is_bold;
                cur_span.is_link = in_link;
                cur_span.is_code = is_code;
                strncpy(cur_span.link_url, in_link ? current_href : "", sizeof(cur_span.link_url) - 1);
                strncpy(cur_span.text, word, sizeof(cur_span.text) - 1);
                cur_x += word_w;
                pending_space = false;
            } else {
                size_t cur_len = strlen(cur_span.text);
                int add_len = (pending_space ? 1 : 0) + word_len;
                if (cur_len + add_len < sizeof(cur_span.text) - 1) {
                    if (pending_space) {
                        cur_span.text[cur_len] = ' ';
                        strcpy(cur_span.text + cur_len + 1, word);
                        cur_x += 8 + word_w;
                    } else {
                        strcpy(cur_span.text + cur_len, word);
                        cur_x += word_w;
                    }
                    pending_space = false;
                } else {
                    flush_span(doc, &cur_span, cur_x, line_h);
                    if (pending_space && cur_x > left_margin) {
                        cur_x += 8;
                    }
                    cur_span.start_x = cur_x;
                    cur_span.y = cur_y;
                    cur_span.color = color;
                    cur_span.is_bold = is_bold;
                    cur_span.is_link = in_link;
                    cur_span.is_code = is_code;
                    strncpy(cur_span.link_url, in_link ? current_href : "", sizeof(cur_span.link_url) - 1);
                    strncpy(cur_span.text, word, sizeof(cur_span.text) - 1);
                    cur_x += word_w;
                    pending_space = false;
                }
            }
        }
    }

    flush_span(doc, &cur_span, cur_x, line_h);
    doc->content_height = cur_y + 40;
}

int html_find_link_at(struct layout_document *doc, int mx, int my, int scroll_y) {
    if (!doc) return -1;
    int page_y = my + scroll_y;
    for (int i = 0; i < doc->link_count; i++) {
        struct link_entry *l = &doc->links[i];
        if (mx >= l->x1 && mx <= l->x2 && page_y >= l->y1 && page_y <= l->y2) {
            return i;
        }
    }
    return -1;
}

void html_render_dui(DuiConnection *conn,
                     DuiWindow win,
                     struct layout_document *doc,
                     int view_x,
                     int view_y,
                     int view_w,
                     int view_h,
                     int scroll_y,
                     int hovered_link_idx) {
    if (!conn || !doc) return;
    (void)view_x;
    (void)view_w;

    /* 1. Draw Rectangles */
    for (int i = 0; i < doc->rect_count; i++) {
        struct rect_box *r = &doc->rects[i];
        int sy = r->y - scroll_y + view_y;
        if (sy + r->h < view_y || sy > view_y + view_h) continue;

        dui_fill_rect(conn, win, r->x, sy, r->w, r->h, r->fill_color);
        if (r->border_color) {
            dui_draw_rect(conn, win, r->x, sy, r->w, r->h, r->border_color);
        }
    }

    /* 2. Draw Lines */
    for (int i = 0; i < doc->line_count; i++) {
        struct line_box *l = &doc->lines[i];
        int sy1 = l->y1 - scroll_y + view_y;
        int sy2 = l->y2 - scroll_y + view_y;
        if (sy1 < view_y && sy2 < view_y) continue;
        if (sy1 > view_y + view_h && sy2 > view_y + view_h) continue;

        dui_draw_line(conn, win, l->x1, sy1, l->x2, sy2, l->color);
    }

    /* 3. Draw Text Elements */
    for (int i = 0; i < doc->text_count; i++) {
        struct text_box *t = &doc->texts[i];
        int sy = t->y - scroll_y + view_y;
        if (sy + 16 < view_y || sy > view_y + view_h) continue;

        uint32_t col = t->color;
        if (t->is_link && t->link_index == hovered_link_idx) {
            col = COLOR_LINK_HOVER;
        }

        dui_draw_text(conn, win, t->x, sy, t->text, col);
        if (t->is_bold) {
            /* 1px pseudo-bold offset */
            dui_draw_text(conn, win, t->x + 1, sy, t->text, col);
        }
    }
}

void html_render_terminal(const char *html) {
    if (!html) return;
    struct layout_document *doc = layout_doc_create();
    if (!doc) return;

    html_layout(doc, html, 80);

    printf("\n=== %s ===\n\n", doc->title);

    int last_y = -1;
    for (int i = 0; i < doc->text_count; i++) {
        struct text_box *t = &doc->texts[i];
        if (last_y != -1 && t->y != last_y) {
            printf("\n");
            if (t->y - last_y > 20) printf("\n");
        }
        last_y = t->y;

        if (t->is_link) {
            printf("\033[34;4m%s\033[0m ", t->text);
        } else if (t->is_bold) {
            printf("\033[1m%s\033[0m ", t->text);
        } else if (t->is_code) {
            printf("\033[33m%s\033[0m ", t->text);
        } else {
            printf("%s ", t->text);
        }
    }
    printf("\n\n");

    if (doc->link_count > 0) {
        printf("References:\n");
        for (int i = 0; i < doc->link_count && i < 20; i++) {
            printf("  [%d] %s\n", i + 1, doc->links[i].url);
        }
        printf("\n");
    }

    layout_doc_free(doc);
}
