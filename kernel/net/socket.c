#include <net/socket.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <net/icmp.h>
#include <net/ipv4.h>
#include <net/net.h>
#include <process/process.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

static struct vfs_ops socket_vfs_ops;

static ssize_t socket_vfs_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)offset;
    if (!node || !node->device || !buffer || size == 0) return -1;
    struct socket_handle *sock = (struct socket_handle *)node->device;

    if (sock->type == SOCK_STREAM && sock->tcp) {
        return tcp_recv(sock->tcp, buffer, size, false, false);
    } else if (sock->type == SOCK_DGRAM && sock->udp) {
        return udp_recv(sock->udp, buffer, size, NULL, NULL, false);
    }
    return -1;
}

static ssize_t socket_vfs_write(struct vfs_node *node, uint64_t offset, size_t size, const void *buffer) {
    (void)offset;
    if (!node || !node->device || !buffer || size == 0) return -1;
    struct socket_handle *sock = (struct socket_handle *)node->device;

    if (sock->type == SOCK_STREAM && sock->tcp) {
        return tcp_send(sock->tcp, buffer, size);
    }
    return -1;
}

static int socket_vfs_close(struct vfs_node *node) {
    if (!node || !node->device) return 0;
    struct socket_handle *sock = (struct socket_handle *)node->device;

    if (sock->type == SOCK_STREAM && sock->tcp) {
        tcp_socket_close(sock->tcp);
    } else if (sock->type == SOCK_DGRAM && sock->udp) {
        udp_socket_close(sock->udp);
    }
    kfree(sock);
    kfree(node);
    return 0;
}

void socket_init(void) {
    socket_vfs_ops.read = socket_vfs_read;
    socket_vfs_ops.write = socket_vfs_write;
    socket_vfs_ops.close = socket_vfs_close;
}

static struct socket_handle *get_socket_from_fd(int sockfd) {
    struct process *proc = process_get_current();
    if (!proc || sockfd < 0 || sockfd >= MAX_FD || !proc->files[sockfd]) {
        return NULL;
    }

    struct file *f = proc->files[sockfd];
    if (!f->node || (f->node->flags & VFS_SOCKET) != VFS_SOCKET) {
        return NULL;
    }

    return (struct socket_handle *)f->node->device;
}

int64_t sys_socket(int domain, int type, int protocol) {
    if (domain != AF_INET && domain != AF_UNIX) {
        return -97; /* -EAFNOSUPPORT */
    }

    struct process *proc = process_get_current();
    if (!proc) return -1;

    int fd = -1;
    for (int i = 0; i < MAX_FD; i++) {
        if (!proc->files[i]) {
            fd = i;
            break;
        }
    }
    if (fd == -1) return -24; /* -EMFILE */

    struct socket_handle *sock = (struct socket_handle *)kzalloc(sizeof(struct socket_handle));
    if (!sock) return -12; /* -ENOMEM */

    sock->domain = domain;
    sock->type = type;
    sock->protocol = protocol;

    if (type == SOCK_STREAM) {
        sock->tcp = tcp_socket_create();
    } else if (type == SOCK_DGRAM) {
        sock->udp = udp_socket_create();
    }

    struct vfs_node *node = (struct vfs_node *)kzalloc(sizeof(struct vfs_node));
    if (!node) {
        kfree(sock);
        return -12;
    }

    strcpy(node->name, "socket");
    node->flags = VFS_SOCKET;
    node->ops = &socket_vfs_ops;
    node->device = sock;
    sock->node = node;

    struct file *f = (struct file *)kzalloc(sizeof(struct file));
    if (!f) {
        kfree(node);
        kfree(sock);
        return -12;
    }

    f->node = node;
    f->flags = O_RDWR;
    f->ref_count = 1;
    proc->files[fd] = f;

    return fd;
}

int64_t sys_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    (void)addrlen;
    if (!addr) return -14; /* -EFAULT */
    struct socket_handle *sock = get_socket_from_fd(sockfd);
    if (!sock) return -88; /* -ENOTSOCK */

    const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
    uint16_t port = ntohs(in->sin_port);

    if (sock->type == SOCK_STREAM && sock->tcp) {
        return tcp_bind(sock->tcp, port);
    } else if (sock->type == SOCK_DGRAM && sock->udp) {
        return udp_bind(sock->udp, port);
    }

    return -22; /* -EINVAL */
}

int64_t sys_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    (void)addrlen;
    if (!addr) return -14; /* -EFAULT */
    struct socket_handle *sock = get_socket_from_fd(sockfd);
    if (!sock) return -88; /* -ENOTSOCK */

    const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
    uint32_t dest_ip = in->sin_addr.s_addr;
    uint16_t dest_port = ntohs(in->sin_port);

    if (sock->type == SOCK_STREAM && sock->tcp) {
        return tcp_connect(sock->tcp, dest_ip, dest_port);
    }

    return 0;
}

int64_t sys_listen(int sockfd, int backlog) {
    struct socket_handle *sock = get_socket_from_fd(sockfd);
    if (!sock) return -88; /* -ENOTSOCK */

    if (sock->type == SOCK_STREAM && sock->tcp) {
        return tcp_listen(sock->tcp, backlog);
    }

    return -95; /* -EOPNOTSUPP */
}

int64_t sys_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    struct process *proc = process_get_current();
    if (!proc || sockfd < 0 || sockfd >= MAX_FD || !proc->files[sockfd]) {
        return -9; /* -EBADF */
    }

    struct socket_handle *sock = get_socket_from_fd(sockfd);
    if (!sock) return -88; /* -ENOTSOCK */
    if (sock->type != SOCK_STREAM || !sock->tcp) return -95;

    bool nonblock = (proc->files[sockfd]->flags & O_NONBLOCK) != 0;

    uint32_t client_ip = 0;
    uint16_t client_port = 0;
    struct tcp_socket *child_tcp = tcp_accept(sock->tcp, &client_ip, &client_port, nonblock);
    if (!child_tcp) {
        return nonblock ? -11 : -1; /* -EAGAIN if nonblock */
    }

    int newfd = -1;
    for (int i = 0; i < MAX_FD; i++) {
        if (!proc->files[i]) {
            newfd = i;
            break;
        }
    }
    if (newfd == -1) {
        tcp_socket_close(child_tcp);
        return -24; /* -EMFILE */
    }

    struct socket_handle *child_sock = (struct socket_handle *)kzalloc(sizeof(struct socket_handle));
    if (!child_sock) {
        tcp_socket_close(child_tcp);
        return -12;
    }

    child_sock->domain = sock->domain;
    child_sock->type = SOCK_STREAM;
    child_sock->protocol = sock->protocol;
    child_sock->tcp = child_tcp;

    struct vfs_node *node = (struct vfs_node *)kzalloc(sizeof(struct vfs_node));
    node->flags = VFS_SOCKET;
    node->ops = &socket_vfs_ops;
    node->device = child_sock;
    child_sock->node = node;

    struct file *f = (struct file *)kzalloc(sizeof(struct file));
    f->node = node;
    f->flags = O_RDWR;
    f->ref_count = 1;
    proc->files[newfd] = f;

    if (addr && addrlen && *addrlen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in *in = (struct sockaddr_in *)addr;
        in->sin_family = AF_INET;
        in->sin_port = htons(client_port);
        in->sin_addr.s_addr = client_ip;
        *addrlen = sizeof(struct sockaddr_in);
    }

    return newfd;
}

int64_t sys_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    (void)flags; (void)addrlen;
    struct socket_handle *sock = get_socket_from_fd(sockfd);
    if (!sock) return -88; /* -ENOTSOCK */

    if (sock->type == SOCK_STREAM && sock->tcp) {
        return tcp_send(sock->tcp, buf, len);
    } else if (sock->type == SOCK_DGRAM && sock->udp) {
        if (!dest_addr) return -14;
        const struct sockaddr_in *in = (const struct sockaddr_in *)dest_addr;
        uint32_t dest_ip = in->sin_addr.s_addr;
        uint16_t dest_port = ntohs(in->sin_port);
        struct net_if *netif = net_get_default_if();
        if (udp_send(netif, dest_ip, sock->udp->port, dest_port, buf, len) == 0) {
            return (int64_t)len;
        }
        return -1;
    }

    return -1;
}

int64_t sys_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    struct socket_handle *sock = get_socket_from_fd(sockfd);
    if (!sock) return -88; /* -ENOTSOCK */

    struct process *proc = process_get_current();
    bool nonblock = ((flags & MSG_DONTWAIT) != 0) ||
                    (proc && sockfd >= 0 && sockfd < MAX_FD && proc->files[sockfd] &&
                     (proc->files[sockfd]->flags & O_NONBLOCK));
    bool peek = (flags & MSG_PEEK) != 0;

    if (sock->type == SOCK_STREAM && sock->tcp) {
        return tcp_recv(sock->tcp, buf, len, nonblock, peek);
    } else if (sock->type == SOCK_DGRAM && sock->udp) {
        uint32_t src_ip = 0;
        uint16_t src_port = 0;
        ssize_t ret = udp_recv(sock->udp, buf, len, &src_ip, &src_port, nonblock);
        if (ret >= 0 && src_addr && addrlen && *addrlen >= sizeof(struct sockaddr_in)) {
            struct sockaddr_in *in = (struct sockaddr_in *)src_addr;
            in->sin_family = AF_INET;
            in->sin_port = htons(src_port);
            in->sin_addr.s_addr = src_ip;
            *addrlen = sizeof(struct sockaddr_in);
        }
        return ret;
    }

    return -1;
}

int64_t sys_shutdown(int sockfd, int how) {
    (void)how;
    struct socket_handle *sock = get_socket_from_fd(sockfd);
    if (!sock) return -88;

    if (sock->type == SOCK_STREAM && sock->tcp) {
        tcp_socket_close(sock->tcp);
        sock->tcp = NULL;
    }
    return 0;
}
