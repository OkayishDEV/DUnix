#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdbool.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: nc [-l] <host> <port>\n");
        return 1;
    }

    bool listen_mode = false;
    const char *host = NULL;
    int port = 0;

    if (strcmp(argv[1], "-l") == 0) {
        listen_mode = true;
        port = atoi(argv[2]);
    } else {
        host = argv[1];
        port = atoi(argv[2]);
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("nc: socket creation failed");
        return 1;
    }

    if (listen_mode) {
        struct sockaddr_in saddr;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons((uint16_t)port);
        saddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
            perror("nc: bind failed");
            close(sockfd);
            return 1;
        }

        listen(sockfd, 5);
        printf("nc: listening on port %d...\n", port);

        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int clientfd = accept(sockfd, (struct sockaddr *)&caddr, &clen);
        if (clientfd >= 0) {
            printf("nc: connection accepted from %s:%d\n",
                   inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));

            char buf[512];
            ssize_t n;
            while ((n = read(clientfd, buf, sizeof(buf) - 1)) > 0) {
                buf[n] = '\0';
                write(STDOUT_FILENO, buf, (size_t)n);
            }
            close(clientfd);
        }
    } else {
        struct hostent *he = gethostbyname(host);
        if (!he || !he->h_addr) {
            printf("nc: unable to resolve host '%s'\n", host);
            close(sockfd);
            return 1;
        }

        struct in_addr in;
        memcpy(&in, he->h_addr, sizeof(struct in_addr));

        struct sockaddr_in saddr;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        saddr.sin_port = htons((uint16_t)port);
        saddr.sin_addr = in;

        if (connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
            perror("nc: connect failed");
            close(sockfd);
            return 1;
        }

        printf("nc: connected to %s (%s):%d\n", host, inet_ntoa(in), port);

        char buf[512];
        ssize_t n;
        while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
            write(sockfd, buf, (size_t)n);
        }
    }

    close(sockfd);
    return 0;
}
