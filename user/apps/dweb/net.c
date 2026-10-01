#include "net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

static void trim_str(char *s) {
    if (!s) return;
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

int url_parse(const char *raw, struct url *u) {
    if (!raw || !u) return -1;
    memset(u, 0, sizeof(struct url));

    char buf[512];
    strncpy(buf, raw, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    trim_str(buf);

    if (strncmp(buf, "about:", 6) == 0) {
        strcpy(u->scheme, "about");
        strncpy(u->path, buf + 6, sizeof(u->path) - 1);
        if (u->path[0] == '\0') strcpy(u->path, "home");
        return 0;
    }

    if (strncmp(buf, "file://", 7) == 0) {
        strcpy(u->scheme, "file");
        strncpy(u->path, buf + 7, sizeof(u->path) - 1);
        if (u->path[0] != '/') {
            char tmp[256];
            snprintf(tmp, sizeof(tmp), "/%s", u->path);
            strncpy(u->path, tmp, sizeof(u->path) - 1);
        }
        return 0;
    }

    if (buf[0] == '/') {
        strcpy(u->scheme, "file");
        strncpy(u->path, buf, sizeof(u->path) - 1);
        return 0;
    }

    const char *p = buf;
    if (strncmp(buf, "http://", 7) == 0) {
        strcpy(u->scheme, "http");
        p += 7;
        u->port = 80;
    } else if (strncmp(buf, "https://", 8) == 0) {
        strcpy(u->scheme, "https");
        p += 8;
        u->port = 443;
    } else {
        strcpy(u->scheme, "http");
        u->port = 80;
    }

    strcpy(u->path, "/");

    const char *slash = strchr(p, '/');
    const char *colon = strchr(p, ':');

    if (colon && (!slash || colon < slash)) {
        size_t hlen = (size_t)(colon - p);
        if (hlen >= sizeof(u->host)) hlen = sizeof(u->host) - 1;
        strncpy(u->host, p, hlen);
        u->host[hlen] = '\0';
        u->port = atoi(colon + 1);
    } else if (slash) {
        size_t hlen = (size_t)(slash - p);
        if (hlen >= sizeof(u->host)) hlen = sizeof(u->host) - 1;
        strncpy(u->host, p, hlen);
        u->host[hlen] = '\0';
    } else {
        strncpy(u->host, p, sizeof(u->host) - 1);
    }

    if (slash) {
        strncpy(u->path, slash, sizeof(u->path) - 1);
    }

    return 0;
}

void url_resolve(const char *base, const char *rel, char *out, size_t out_sz) {
    if (!rel || !out || out_sz == 0) return;

    /* Absolute URL */
    if (strncmp(rel, "http://", 7) == 0 ||
        strncmp(rel, "https://", 8) == 0 ||
        strncmp(rel, "file://", 7) == 0 ||
        strncmp(rel, "about:", 6) == 0) {
        strncpy(out, rel, out_sz - 1);
        out[out_sz - 1] = '\0';
        return;
    }

    struct url b;
    if (url_parse(base, &b) != 0) {
        strncpy(out, rel, out_sz - 1);
        out[out_sz - 1] = '\0';
        return;
    }

    if (strcmp(b.scheme, "about") == 0) {
        strncpy(out, rel, out_sz - 1);
        out[out_sz - 1] = '\0';
        return;
    }

    if (strcmp(b.scheme, "file") == 0) {
        if (rel[0] == '/') {
            snprintf(out, out_sz, "file://%s", rel);
        } else {
            char dir[256];
            strncpy(dir, b.path, sizeof(dir) - 1);
            dir[sizeof(dir) - 1] = '\0';
            char *last_slash = strrchr(dir, '/');
            if (last_slash) *(last_slash + 1) = '\0';
            else strcpy(dir, "/");
            snprintf(out, out_sz, "file://%s%s", dir, rel);
        }
        return;
    }

    /* HTTP */
    if (rel[0] == '/') {
        if (b.port != 80) {
            snprintf(out, out_sz, "http://%s:%d%s", b.host, b.port, rel);
        } else {
            snprintf(out, out_sz, "http://%s%s", b.host, rel);
        }
    } else {
        char dir[256];
        strncpy(dir, b.path, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = '\0';
        char *last_slash = strrchr(dir, '/');
        if (last_slash) *(last_slash + 1) = '\0';
        else strcpy(dir, "/");

        if (b.port != 80) {
            snprintf(out, out_sz, "http://%s:%d%s%s", b.host, b.port, dir, rel);
        } else {
            snprintf(out, out_sz, "http://%s%s%s", b.host, dir, rel);
        }
    }
}

const char *net_get_builtin_page(const char *name) {
    if (!name || strcmp(name, "home") == 0 || strcmp(name, "") == 0) {
        return "<!DOCTYPE html>\n"
               "<html>\n"
               "<head><title>DWeb - Welcome to DUnix</title></head>\n"
               "<body>\n"
               "  <h1>DWeb Navigator</h1>\n"
               "  <p><b>DWeb</b> is the native graphical web browser for the <b>DUnix 64-Bit Operating System</b>, "
               "featuring an integrated HTTP client and WebKit-like HTML layout engine.</p>\n"
               "  <hr>\n"
               "  <h2>Real Web Destinations</h2>\n"
               "  <ul>\n"
               "    <li><a href=\"http://google.com/\">http://google.com</a> - Google Live Search Engine</li>\n"
               "    <li><a href=\"http://info.cern.ch/\">http://info.cern.ch</a> - The World's First Website (CERN)</li>\n"
               "    <li><a href=\"http://example.com/\">http://example.com</a> - Example Domain (IANA)</li>\n"
               "    <li><a href=\"http://neverssl.com/\">http://neverssl.com</a> - NeverSSL Live Web Page</li>\n"
               "    <li><a href=\"http://127.0.0.1:8080/\">http://127.0.0.1:8080</a> - DUnix Local Web Server</li>\n"
               "  </ul>\n"
               "  <hr>\n"
               "  <h2>Local System & Documentation</h2>\n"
               "  <ul>\n"
               "    <li><a href=\"file:///etc/os-release\">Operating System Release Info (/etc/os-release)</a></li>\n"
               "    <li><a href=\"file:///etc/hosts\">Network Hosts File (/etc/hosts)</a></li>\n"
               "    <li><a href=\"about:about\">About DWeb Engine Architecture</a></li>\n"
               "    <li><a href=\"about:help\">Browser Help & Shortcuts</a></li>\n"
               "  </ul>\n"
               "  <hr>\n"
               "  <h2>DUnix Operating System Features</h2>\n"
               "  <p>DUnix features a monolithic 64-bit Unix kernel with preemptive multitasking, "
               "Ext2 persistent storage, native DUI display server, and multi-user security with sudo.</p>\n"
               "  <blockquote>\"Simplicity is prerequisite for reliability.\" - Edsger W. Dijkstra</blockquote>\n"
               "</body>\n"
               "</html>\n";
    }

    if (strcmp(name, "about") == 0 || strcmp(name, "version") == 0) {
        return "<!DOCTYPE html>\n"
               "<html>\n"
               "<head><title>About DWeb Engine</title></head>\n"
               "<body>\n"
               "  <h1>About DWeb Browser Engine</h1>\n"
               "  <p>DWeb is a high-performance, lightweight web browser engine built natively for DUnix.</p>\n"
               "  <hr>\n"
               "  <h2>Engine Specifications</h2>\n"
               "  <table>\n"
               "    <tr><th>Component</th><th>Implementation</th></tr>\n"
               "    <tr><td>Engine Architecture</td><td>WebKit-Style Tokenizer, DOM & Box Model Layout</td></tr>\n"
               "    <tr><td>Networking</td><td>BSD AF_INET Sockets (HTTP/1.0, HTTP/1.1)</td></tr>\n"
               "    <tr><td>Graphics Subsystem</td><td>DWS Display Protocol & DUI Client Library</td></tr>\n"
               "    <tr><td>Supported Protocols</td><td>http://, file://, about:</td></tr>\n"
               "    <tr><td>User-Agent</td><td>DWeb/1.0 (DUnix x86_64; WebKit-Compatible)</td></tr>\n"
               "  </table>\n"
               "  <hr>\n"
               "  <p><a href=\"about:home\">&lt;- Return to Home Portal</a></p>\n"
               "</body>\n"
               "</html>\n";
    }

    if (strcmp(name, "help") == 0) {
        return "<!DOCTYPE html>\n"
               "<html>\n"
               "<head><title>DWeb Help & Shortcuts</title></head>\n"
               "<body>\n"
               "  <h1>DWeb Help &amp; Keyboard Shortcuts</h1>\n"
               "  <p>Learn how to navigate and browse the web with DWeb.</p>\n"
               "  <hr>\n"
               "  <h2>Navigation Controls</h2>\n"
               "  <ul>\n"
               "    <li><b>Back Button [ &lt; ]:</b> Go to previously visited page</li>\n"
               "    <li><b>Forward Button [ &gt; ]:</b> Go forward in history</li>\n"
               "    <li><b>Reload Button [ R ]:</b> Refresh current page</li>\n"
               "    <li><b>Home Button [ H ]:</b> Return to home portal (about:home)</li>\n"
               "    <li><b>Address Bar:</b> Click to enter any URL and press Enter</li>\n"
               "    <li><b>Hyperlinks:</b> Click any blue link to navigate immediately</li>\n"
               "  </ul>\n"
               "  <hr>\n"
               "  <h2>Keyboard Shortcuts</h2>\n"
               "  <ul>\n"
               "    <li><b>Down Arrow / Space / 'j':</b> Scroll down</li>\n"
               "    <li><b>Up Arrow / 'k':</b> Scroll up</li>\n"
               "    <li><b>'h' / Backspace:</b> Go back</li>\n"
               "    <li><b>'l':</b> Go forward</li>\n"
               "    <li><b>'r':</b> Reload page</li>\n"
               "    <li><b>'/' or 'o':</b> Focus URL address bar</li>\n"
               "  </ul>\n"
               "  <hr>\n"
               "  <p><a href=\"about:home\">&lt;- Return to Home Portal</a></p>\n"
               "</body>\n"
               "</html>\n";
    }

    return "<!DOCTYPE html>\n<html><head><title>DWeb</title></head><body></body></html>\n";
}

static int fetch_file(const char *path, char **out_body, size_t *out_len, int *out_status, char *out_status_text, size_t st_sz) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        *out_status = 404;
        snprintf(out_status_text, st_sz, "404 Not Found");
        char not_found[512];
        snprintf(not_found, sizeof(not_found),
                 "<!DOCTYPE html><html><head><title>404 Not Found</title></head>"
                 "<body><h1>File Not Found</h1><p>Cannot open file: <code>%s</code></p>"
                 "<hr><p><a href=\"about:home\">Return Home</a></p></body></html>", path);
        *out_body = strdup(not_found);
        *out_len = strlen(*out_body);
        return 0;
    }

    /* Read up to 256 KB */
    size_t cap = 32768;
    char *buf = (char *)malloc(cap);
    if (!buf) { close(fd); return -1; }

    size_t total = 0;
    ssize_t n;
    while ((n = read(fd, buf + total, cap - total - 1)) > 0) {
        total += (size_t)n;
        if (total + 4096 >= cap && cap < 262144) {
            cap *= 2;
            buf = (char *)realloc(buf, cap);
            if (!buf) { close(fd); return -1; }
        }
    }
    close(fd);
    buf[total] = '\0';

    /* If plain text, wrap in pre tags for clean display */
    if (strstr(path, ".html") == NULL && strstr(path, ".htm") == NULL) {
        size_t wlen = total + 256;
        char *wrapped = (char *)malloc(wlen);
        if (wrapped) {
            snprintf(wrapped, wlen,
                     "<!DOCTYPE html><html><head><title>%s</title></head>"
                     "<body><h1>File: %s</h1><hr><pre>%s</pre><hr><a href=\"about:home\">Home</a></body></html>",
                     path, path, buf);
            free(buf);
            buf = wrapped;
            total = strlen(buf);
        }
    }

    *out_body = buf;
    *out_len = total;
    *out_status = 200;
    snprintf(out_status_text, st_sz, "200 OK (File)");
    return 0;
}

static int fetch_http_hop(struct url *u, int hops, char **out_body, size_t *out_len, int *out_status, char *out_status_text, size_t st_sz) {
    if (hops > 5) {
        *out_status = 310;
        snprintf(out_status_text, st_sz, "Too Many Redirects");
        return -1;
    }

    /* HTTPS Scheme / Port 443 */
    if (strcmp(u->scheme, "https") == 0 || u->port == 443) {
        /* Attempt HTTP on port 80 first as fallback */
        struct url http_u = *u;
        strcpy(http_u.scheme, "http");
        http_u.port = 80;
        int status = 0;
        char st_buf[64] = "";
        char *try_body = NULL;
        size_t try_len = 0;
        if (hops < 4 && fetch_http_hop(&http_u, hops + 1, &try_body, &try_len, &status, st_buf, sizeof(st_buf)) == 0 &&
            status >= 200 && status < 400) {
            *out_body = try_body;
            *out_len = try_len;
            *out_status = status;
            snprintf(out_status_text, st_sz, "%s (HTTP Fallback)", st_buf);
            return 0;
        }
        if (try_body) free(try_body);

        *out_status = 426;
        snprintf(out_status_text, st_sz, "426 Upgrade Required (TLS)");
        char tls_page[1024];
        snprintf(tls_page, sizeof(tls_page),
            "<!DOCTYPE html><html><head><title>426 TLS Required - %s</title></head>"
            "<body>"
            "  <h1>426 Upgrade Required (TLS/HTTPS)</h1>"
            "  <p>The destination host <b>%s</b> mandates encrypted TLS/HTTPS (port 443).</p>"
            "  <hr>"
            "  <h2>Available Live Web Destinations</h2>"
            "  <ul>"
            "    <li><b>Try Plain HTTP:</b> <a href=\"http://%s%s\">http://%s%s</a></li>"
            "    <li><b>Google Search:</b> <a href=\"http://google.com/\">http://google.com/</a></li>"
            "    <li><b>Example Domain:</b> <a href=\"http://example.com/\">http://example.com/</a></li>"
            "    <li><b>CERN Web:</b> <a href=\"http://info.cern.ch/\">http://info.cern.ch/</a></li>"
            "    <li><b>NeverSSL:</b> <a href=\"http://neverssl.com/\">http://neverssl.com/</a></li>"
            "    <li><b>Local HTTP Server:</b> <a href=\"http://127.0.0.1:8080/\">http://127.0.0.1:8080/</a></li>"
            "  </ul>"
            "  <hr>"
            "  <p><a href=\"about:home\">&lt;- Return to Home Portal</a></p>"
            "</body></html>",
            u->host, u->host, u->host, u->path, u->host, u->path);
        *out_body = strdup(tls_page);
        *out_len = strlen(*out_body);
        return 0;
    }

    struct hostent *he = gethostbyname(u->host);
    if (!he || !he->h_addr) {
        *out_status = 502;
        snprintf(out_status_text, st_sz, "DNS Resolution Failed");
        char err_page[512];
        snprintf(err_page, sizeof(err_page),
                 "<!DOCTYPE html><html><head><title>DNS Error</title></head>"
                 "<body><h1>Server Not Found</h1><p>Cannot resolve hostname <b>%s</b></p>"
                 "<hr><p><a href=\"about:home\">Return Home</a></p></body></html>", u->host);
        *out_body = strdup(err_page);
        *out_len = strlen(*out_body);
        return 0;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        *out_status = 500;
        snprintf(out_status_text, st_sz, "Socket Error");
        return -1;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons((uint16_t)u->port);
    memcpy(&saddr.sin_addr, he->h_addr, sizeof(struct in_addr));

    if (connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        close(sockfd);
        *out_status = 503;
        snprintf(out_status_text, st_sz, "Connection Failed");
        char err_page[512];
        snprintf(err_page, sizeof(err_page),
                 "<!DOCTYPE html><html><head><title>Connection Error</title></head>"
                 "<body><h1>Unable to Connect</h1><p>Could not connect to <b>%s:%d</b></p>"
                 "<hr><p><a href=\"about:home\">Return Home</a></p></body></html>", u->host, u->port);
        *out_body = strdup(err_page);
        *out_len = strlen(*out_body);
        return 0;
    }

    /* Send HTTP GET request */
    char req[512];
    snprintf(req, sizeof(req),
             "GET %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "User-Agent: DWeb/1.0 (DUnix x86_64; WebKit-Compatible)\r\n"
             "Accept: text/html,application/xhtml+xml,text/plain,*/*\r\n"
             "Connection: close\r\n\r\n",
             u->path, u->host);

    send(sockfd, req, strlen(req), 0);

    /* Read response */
    size_t cap = 32768;
    char *resp = (char *)malloc(cap);
    if (!resp) { close(sockfd); return -1; }

    size_t total = 0;
    ssize_t n;
    while ((n = recv(sockfd, resp + total, cap - total - 1, 0)) > 0) {
        total += (size_t)n;
        if (total + 4096 >= cap && cap < 524288) {
            cap *= 2;
            resp = (char *)realloc(resp, cap);
            if (!resp) { close(sockfd); return -1; }
        }
    }
    close(sockfd);
    resp[total] = '\0';

    /* Parse status line */
    int status_code = 200;
    char status_msg[64] = "OK";
    char *eol = strstr(resp, "\r\n");
    if (!eol) eol = strchr(resp, '\n');

    if (eol) {
        *eol = '\0';
        char *sp1 = strchr(resp, ' ');
        if (sp1) {
            status_code = atoi(sp1 + 1);
            char *sp2 = strchr(sp1 + 1, ' ');
            if (sp2) {
                strncpy(status_msg, sp2 + 1, sizeof(status_msg) - 1);
                status_msg[sizeof(status_msg) - 1] = '\0';
            }
        }
        *eol = '\r'; /* restore */
    }

    *out_status = status_code;
    snprintf(out_status_text, st_sz, "%d %s", status_code, status_msg);

    /* Check for Redirect (301, 302, 303, 307) */
    if (status_code >= 301 && status_code <= 308) {
        char *loc_hdr = strstr(resp, "Location:");
        if (!loc_hdr) loc_hdr = strstr(resp, "location:");
        if (loc_hdr) {
            char *loc_val = loc_hdr + 9;
            while (*loc_val == ' ' || *loc_val == '\t') loc_val++;
            char *loc_end = strstr(loc_val, "\r\n");
            if (!loc_end) loc_end = strchr(loc_val, '\n');
            if (loc_end) {
                *loc_end = '\0';
                char raw_loc[256];
                strncpy(raw_loc, loc_val, sizeof(raw_loc) - 1);
                raw_loc[sizeof(raw_loc) - 1] = '\0';
                trim_str(raw_loc);
                free(resp);

                char base_url[256];
                snprintf(base_url, sizeof(base_url), "%s://%s:%d%s", u->scheme, u->host, u->port, u->path);
                char resolved_url[256];
                url_resolve(base_url, raw_loc, resolved_url, sizeof(resolved_url));

                struct url next_u;
                url_parse(resolved_url, &next_u);
                return fetch_http_hop(&next_u, hops + 1, out_body, out_len, out_status, out_status_text, st_sz);
            }
        }
    }

    /* Extract body after headers */
    char *body = strstr(resp, "\r\n\r\n");
    if (body) body += 4;
    else {
        body = strstr(resp, "\n\n");
        if (body) body += 2;
        else body = resp;
    }

    size_t body_len = strlen(body);
    char *body_copy = (char *)malloc(body_len + 1);
    if (body_copy) {
        memcpy(body_copy, body, body_len + 1);
        *out_body = body_copy;
        *out_len = body_len;
    } else {
        *out_body = strdup("");
        *out_len = 0;
    }

    free(resp);
    return 0;
}

int net_fetch(const char *url_str,
              char **out_body,
              size_t *out_len,
              int *out_status,
              char *out_status_text,
              size_t st_sz,
              char *out_title,
              size_t title_sz) {
    if (!url_str || !out_body) return -1;
    *out_body = NULL;
    *out_len = 0;
    *out_status = 0;
    if (out_status_text && st_sz > 0) out_status_text[0] = '\0';
    if (out_title && title_sz > 0) out_title[0] = '\0';

    struct url u;
    url_parse(url_str, &u);

    if (strcmp(u.scheme, "about") == 0) {
        const char *page = net_get_builtin_page(u.path);
        *out_body = strdup(page);
        *out_len = strlen(page);
        *out_status = 200;
        if (out_status_text) snprintf(out_status_text, st_sz, "200 OK (Built-in)");
        return 0;
    }

    if (strcmp(u.scheme, "file") == 0) {
        return fetch_file(u.path, out_body, out_len, out_status, out_status_text, st_sz);
    }

    if (strcmp(u.scheme, "http") == 0) {
        return fetch_http_hop(&u, 0, out_body, out_len, out_status, out_status_text, st_sz);
    }

    *out_status = 400;
    if (out_status_text) snprintf(out_status_text, st_sz, "400 Unsupported Scheme");
    *out_body = strdup("<!DOCTYPE html><html><body><h1>Unsupported Protocol</h1></body></html>");
    *out_len = strlen(*out_body);
    return -1;
}
