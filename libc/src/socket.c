#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#define SYS_socket   41
#define SYS_connect  42
#define SYS_accept   43
#define SYS_sendto   44
#define SYS_recvfrom 45
#define SYS_shutdown 48
#define SYS_bind     49
#define SYS_listen   50

extern int64_t __syscall(int64_t num, ...);

int socket(int domain, int type, int protocol) {
    int64_t res = __syscall(SYS_socket, domain, type, protocol);
    return (int)res;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    int64_t res = __syscall(SYS_bind, sockfd, (int64_t)addr, addrlen);
    return (int)res;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    int64_t res = __syscall(SYS_connect, sockfd, (int64_t)addr, addrlen);
    return (int)res;
}

int listen(int sockfd, int backlog) {
    int64_t res = __syscall(SYS_listen, sockfd, backlog, 0);
    return (int)res;
}

int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    int64_t res = __syscall(SYS_accept, sockfd, (int64_t)addr, (int64_t)addrlen);
    if (res < 0) {
        errno = (int)(-res);
        return -1;
    }
    return (int)res;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    return sendto(sockfd, buf, len, flags, NULL, 0);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    return recvfrom(sockfd, buf, len, flags, NULL, NULL);
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    int64_t res = __syscall(SYS_sendto, sockfd, (int64_t)buf, (int64_t)len, flags, (int64_t)dest_addr, (int64_t)addrlen);
    return (ssize_t)res;
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    int64_t res = __syscall(SYS_recvfrom, sockfd, (int64_t)buf, (int64_t)len, flags, (int64_t)src_addr, (int64_t)addrlen);
    return (ssize_t)res;
}

int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen) {
    (void)sockfd; (void)level; (void)optname; (void)optval; (void)optlen;
    return 0;
}

int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen) {
    (void)sockfd; (void)level; (void)optname; (void)optval; (void)optlen;
    return 0;
}

int shutdown(int sockfd, int how) {
    int64_t res = __syscall(SYS_shutdown, sockfd, how, 0);
    return (int)res;
}

in_addr_t inet_addr(const char *cp) {
    if (!cp) return INADDR_NONE;
    unsigned int val[4] = {0, 0, 0, 0};
    int parts = 0;
    const char *p = cp;
    bool has_digit = false;

    while (*p) {
        if (*p >= '0' && *p <= '9') {
            val[parts] = val[parts] * 10 + (*p - '0');
            if (val[parts] > 255) return INADDR_NONE;
            has_digit = true;
        } else if (*p == '.') {
            if (!has_digit) return INADDR_NONE;
            parts++;
            if (parts >= 4) return INADDR_NONE;
            has_digit = false;
        } else {
            return INADDR_NONE;
        }
        p++;
    }

    if (parts != 3 || !has_digit) return INADDR_NONE;
    return (in_addr_t)((val[0]) | (val[1] << 8) | (val[2] << 16) | (val[3] << 24));
}

static char ntoa_buf[32];
char *inet_ntoa(struct in_addr in) {
    uint8_t *b = (uint8_t *)&in.s_addr;
    snprintf(ntoa_buf, sizeof(ntoa_buf), "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    return ntoa_buf;
}
