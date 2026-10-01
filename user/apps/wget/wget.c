#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: wget [-O output_file] <http://host[:port]/path | host [port] [path]>\n");
        printf("Note: For HTTPS/GitHub files, serve locally from host via: python3 -m http.server 8000\n");
        return 1;
    }

    const char *out_filename = NULL;
    int arg_idx = 1;

    if (strcmp(argv[arg_idx], "-O") == 0) {
        if (argc < 4) {
            printf("Usage: wget -O <output_file> <url>\n");
            return 1;
        }
        out_filename = argv[2];
        arg_idx = 3;
    }

    const char *raw_target = argv[arg_idx];

    /* Check for HTTPS */
    if (strncmp(raw_target, "https://", 8) == 0) {
        printf("wget: HTTPS (TLS encryption) requested for '%s'.\n", raw_target);
        printf("GitHub and modern HTTPS sites require TLS 1.2/1.3.\n");
        printf("-> Quick Solution: On your host machine run:\n");
        printf("   1) curl -O %s\n", raw_target);
        printf("   2) python3 -m http.server 8000\n");
        printf("   Then in DUnix: wget http://10.0.2.2:8000/%s\n", strrchr(raw_target, '/') ? strrchr(raw_target, '/') + 1 : "file");
        return 1;
    }

    char host[128];
    int port = 80;
    char path[256];
    strcpy(path, "/");

    if (strncmp(raw_target, "http://", 7) == 0) {
        const char *p = raw_target + 7;
        const char *slash = strchr(p, '/');
        const char *colon = strchr(p, ':');

        if (colon && (!slash || colon < slash)) {
            size_t hlen = (size_t)(colon - p);
            if (hlen >= sizeof(host)) hlen = sizeof(host) - 1;
            strncpy(host, p, hlen);
            host[hlen] = '\0';
            port = atoi(colon + 1);
        } else if (slash) {
            size_t hlen = (size_t)(slash - p);
            if (hlen >= sizeof(host)) hlen = sizeof(host) - 1;
            strncpy(host, p, hlen);
            host[hlen] = '\0';
        } else {
            strncpy(host, p, sizeof(host) - 1);
        }

        if (slash) {
            strncpy(path, slash, sizeof(path) - 1);
        }
    } else {
        strncpy(host, raw_target, sizeof(host) - 1);
        if (argc > arg_idx + 1) port = atoi(argv[arg_idx + 1]);
        if (argc > arg_idx + 2) strncpy(path, argv[arg_idx + 2], sizeof(path) - 1);
    }

    struct hostent *he = gethostbyname(host);
    if (!he || !he->h_addr) {
        printf("wget: unable to resolve host '%s'\n", host);
        return 1;
    }

    struct in_addr in;
    memcpy(&in, he->h_addr, sizeof(struct in_addr));
    char *ip_str = inet_ntoa(in);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("wget: socket failed");
        return 1;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons((uint16_t)port);
    saddr.sin_addr = in;

    printf("Connecting to %s (%s):%d...\n", host, ip_str, port);
    if (connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        perror("wget: unable to connect to remote host");
        close(sockfd);
        return 1;
    }

    printf("connected.\nHTTP request sent, awaiting response...\n");

    char req[512];
    snprintf(req, sizeof(req),
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: DUnix-Wget/1.0\r\n"
             "Connection: close\r\n\r\n",
             path, host);

    send(sockfd, req, strlen(req), 0);

    int out_fd = STDOUT_FILENO;
    if (out_filename) {
        out_fd = open(out_filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (out_fd < 0) {
            perror("wget: cannot open output file");
            close(sockfd);
            return 1;
        }
    }

    char buf[1024];
    ssize_t n;
    bool in_header = true;
    size_t total_bytes = 0;

    while ((n = recv(sockfd, buf, sizeof(buf) - 1, 0)) > 0) {
        buf[n] = '\0';

        if (in_header) {
            char *body = strstr(buf, "\r\n\r\n");
            if (body) {
                body += 4;
                size_t body_len = (size_t)(n - (body - buf));
                if (body_len > 0) {
                    write(out_fd, body, body_len);
                    total_bytes += body_len;
                }
                in_header = false;
            } else {
                /* Print HTTP headers to stdout if no output file specified */
                if (!out_filename) {
                    write(STDOUT_FILENO, buf, (size_t)n);
                }
            }
        } else {
            write(out_fd, buf, (size_t)n);
            total_bytes += (size_t)n;
        }
    }

    if (out_filename) {
        close(out_fd);
        printf("wget: Saved %lu bytes to '%s'\n", (unsigned long)total_bytes, out_filename);
    }

    close(sockfd);
    return 0;
}
