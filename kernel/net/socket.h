#ifndef _NET_SOCKET_H
#define _NET_SOCKET_H

#include <dunix/types.h>
#include <net/net.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <fs/vfs.h>

#define AF_UNSPEC 0
#define AF_UNIX   1
#define AF_INET   2

#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOCK_RAW    3

#define MSG_PEEK     0x02
#define MSG_DONTWAIT 0x40

typedef uint32_t socklen_t;

struct __attribute__((packed)) in_addr {
    uint32_t s_addr;
};

struct __attribute__((packed)) sockaddr_in {
    uint16_t       sin_family;
    uint16_t       sin_port;
    struct in_addr sin_addr;
    char           sin_zero[8];
};

struct __attribute__((packed)) sockaddr {
    uint16_t sa_family;
    char     sa_data[14];
};

struct socket_handle {
    int domain;
    int type;
    int protocol;
    union {
        struct tcp_socket *tcp;
        struct udp_socket *udp;
    };
    struct vfs_node *node;
};

void socket_init(void);

int64_t sys_socket(int domain, int type, int protocol);
int64_t sys_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int64_t sys_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int64_t sys_listen(int sockfd, int backlog);
int64_t sys_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int64_t sys_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
int64_t sys_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
int64_t sys_shutdown(int sockfd, int how);

#endif /* _NET_SOCKET_H */
