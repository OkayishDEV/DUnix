#ifndef _LIBC_SYS_SOCKET_H
#define _LIBC_SYS_SOCKET_H

#include <sys/types.h>
#include <stdint.h>

#define AF_UNSPEC 0
#define AF_UNIX   1
#define AF_INET   2

#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOCK_RAW    3

#define MSG_PEEK     0x02
#define MSG_DONTWAIT 0x40

#define SOL_SOCKET   1
#define SO_REUSEADDR 2

typedef uint32_t socklen_t;

struct sockaddr {
    uint16_t sa_family;
    char     sa_data[14];
};

int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
int shutdown(int sockfd, int how);

#endif /* _LIBC_SYS_SOCKET_H */
