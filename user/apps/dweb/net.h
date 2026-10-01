#ifndef _DWEB_NET_H
#define _DWEB_NET_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct url {
    char scheme[16];   /* "http", "file", "about" */
    char host[128];    /* e.g. "127.0.0.1", "localhost", "example.com" */
    int  port;         /* e.g. 80, 8080 */
    char path[256];    /* e.g. "/index.html" */
};

/* Parse a URL string into components */
int url_parse(const char *raw, struct url *u);

/* Resolve a relative URL against a base URL */
void url_resolve(const char *base, const char *rel, char *out, size_t out_sz);

/* Fetch content from a URL (http, file, or about) */
int net_fetch(const char *url_str,
              char **out_body,
              size_t *out_len,
              int *out_status,
              char *out_status_text,
              size_t st_sz,
              char *out_title,
              size_t title_sz);

/* Get built-in HTML page content */
const char *net_get_builtin_page(const char *name);

#endif /* _DWEB_NET_H */
