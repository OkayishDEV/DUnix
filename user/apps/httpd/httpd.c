#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char **argv) {
    int port = (argc > 1) ? atoi(argv[1]) : 80;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("httpd: socket creation failed");
        return 1;
    }

    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons((uint16_t)port);
    saddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        perror("httpd: bind failed");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("httpd: listen failed");
        close(server_fd);
        return 1;
    }

    printf("DUnix HTTP Server daemon running on port %d...\n", port);

    for (;;) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int client_fd = accept(server_fd, (struct sockaddr *)&caddr, &clen);
        if (client_fd < 0) continue;

        char req_buf[1024];
        ssize_t n = recv(client_fd, req_buf, sizeof(req_buf) - 1, 0);
        if (n > 0) {
            req_buf[n] = '\0';
            printf("httpd: request from %s\n", inet_ntoa(caddr.sin_addr));

            const char *body =
                "<!DOCTYPE html>\n"
                "<html>\n"
                "<head><title>Welcome to DUnix</title></head>\n"
                "<body style=\"font-family: sans-serif; background: #121212; color: #00FF66; padding: 40px;\">\n"
                "  <h1>DUnix 64-Bit Operating System</h1>\n"
                "  <p>Native HTTP Server Daemon is operational on DUnix TCP/IP Stack!</p>\n"
                "  <hr/>\n"
                "  <pre>Kernel: DUnix 64-bit | Sockets: BSD AF_INET | Storage: Ext2</pre>\n"
                "</body>\n"
                "</html>\n";

            char resp[2048];
            snprintf(resp, sizeof(resp),
                     "HTTP/1.1 200 OK\r\n"
                     "Content-Type: text/html\r\n"
                     "Content-Length: %zu\r\n"
                     "Connection: close\r\n\r\n%s",
                     strlen(body), body);

            send(client_fd, resp, strlen(resp), 0);
        }
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
