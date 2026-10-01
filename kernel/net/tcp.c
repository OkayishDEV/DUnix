#include <net/tcp.h>
#include <net/ipv4.h>
#include <net/net.h>
#include <net/e1000.h>
#include <mm/heap.h>
#include <sched/sched.h>
#include <arch/x86_64/drivers/pit.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

static struct tcp_socket *tcp_sockets = NULL;
static uint16_t next_tcp_port = 49152;
static uint32_t initial_seq = 1000000;

static inline bool is_loopback_ip(uint32_t ip) {
    return (ip & 0xFF) == 127;
}

static struct net_if *tcp_route_interface(uint32_t dest_ip) {
    if (is_loopback_ip(dest_ip)) {
        struct net_if *lo = net_get_by_name("lo0");
        if (lo) return lo;
    }
    return net_get_default_if();
}

static uint16_t tcp_calc_checksum(uint32_t src_ip, uint32_t dest_ip, const void *tcp_seg, size_t tcp_len) {
    size_t total_len = sizeof(struct tcp_pseudo_hdr) + tcp_len;
    uint8_t *buf = (uint8_t *)kmalloc(total_len);
    if (!buf) return 0;

    struct tcp_pseudo_hdr *phdr = (struct tcp_pseudo_hdr *)buf;
    phdr->src_ip = src_ip;
    phdr->dest_ip = dest_ip;
    phdr->zero = 0;
    phdr->protocol = IPV4_PROTO_TCP;
    phdr->tcp_len = htons((uint16_t)tcp_len);

    memcpy(buf + sizeof(struct tcp_pseudo_hdr), tcp_seg, tcp_len);

    uint16_t cs = net_checksum(buf, total_len);
    kfree(buf);
    return cs;
}

static int tcp_send_segment(struct net_if *netif, uint32_t src_ip, uint32_t dest_ip,
                            uint16_t src_port, uint16_t dest_port,
                            uint32_t seq, uint32_t ack, uint8_t flags,
                            const void *payload, size_t payload_len) {
    if (!netif) return -1;

    size_t total_len = sizeof(struct tcp_hdr) + payload_len;
    uint8_t *packet = (uint8_t *)kmalloc(total_len);
    if (!packet) return -1;

    struct tcp_hdr *hdr = (struct tcp_hdr *)packet;
    hdr->src_port = htons(src_port);
    hdr->dest_port = htons(dest_port);
    hdr->seq_num = htonl(seq);
    hdr->ack_num = htonl(ack);
    hdr->data_offset = (sizeof(struct tcp_hdr) / 4) << 4;
    hdr->flags = flags;
    hdr->window_size = htons(TCP_RX_BUFFER_SIZE);
    hdr->checksum = 0;
    hdr->urgent_ptr = 0;

    if (payload && payload_len > 0) {
        memcpy(packet + sizeof(struct tcp_hdr), payload, payload_len);
    }

    hdr->checksum = tcp_calc_checksum(src_ip, dest_ip, packet, total_len);

    int res = ipv4_send(netif, dest_ip, IPV4_PROTO_TCP, packet, total_len);
    kfree(packet);
    return res;
}

void tcp_init(void) {
    tcp_sockets = NULL;
    next_tcp_port = 49152;
    initial_seq = 1000000;
}

struct tcp_socket *tcp_socket_create(void) {
    struct tcp_socket *sock = (struct tcp_socket *)kzalloc(sizeof(struct tcp_socket));
    if (!sock) return NULL;

    sock->state = TCP_STATE_CLOSED;
    sock->local_port = next_tcp_port++;
    if (next_tcp_port >= 65535) next_tcp_port = 49152;
    sock->seq_num = initial_seq += 10000;

    sock->next = tcp_sockets;
    tcp_sockets = sock;
    return sock;
}

void tcp_socket_close(struct tcp_socket *sock) {
    if (!sock) return;

    if (sock->state == TCP_STATE_ESTABLISHED) {
        struct net_if *netif = tcp_route_interface(sock->remote_ip);
        if (netif) {
            tcp_send_segment(netif, sock->local_ip, sock->remote_ip, sock->local_port, sock->remote_port,
                             sock->seq_num, sock->ack_num, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0);
            sock->seq_num++;
        }
    }
    sock->state = TCP_STATE_CLOSED;

    if (tcp_sockets == sock) {
        tcp_sockets = sock->next;
    } else {
        struct tcp_socket *curr = tcp_sockets;
        while (curr && curr->next != sock) curr = curr->next;
        if (curr) curr->next = sock->next;
    }

    kfree(sock);
}

int tcp_bind(struct tcp_socket *sock, uint16_t port) {
    if (!sock) return -1;
    sock->local_port = port;
    return 0;
}

int tcp_listen(struct tcp_socket *sock, int backlog) {
    (void)backlog;
    if (!sock) return -1;
    sock->state = TCP_STATE_LISTEN;
    sock->pending_count = 0;
    return 0;
}

struct tcp_socket *tcp_accept(struct tcp_socket *sock, uint32_t *client_ip, uint16_t *client_port, bool nonblock) {
    if (!sock || sock->state != TCP_STATE_LISTEN) return NULL;

    for (;;) {
        if (sock->pending_count > 0) {
            struct tcp_socket *client = sock->pending_conns[0];
            for (int i = 0; i < sock->pending_count - 1; i++) {
                sock->pending_conns[i] = sock->pending_conns[i + 1];
            }
            sock->pending_count--;

            if (client_ip) *client_ip = client->remote_ip;
            if (client_port) *client_port = client->remote_port;
            return client;
        }

        if (nonblock) return NULL;
        sched_sleep(10);
    }
}

int tcp_connect(struct tcp_socket *sock, uint32_t dest_ip, uint16_t dest_port) {
    if (!sock) return -1;

    struct net_if *netif = tcp_route_interface(dest_ip);
    if (!netif) return -1;

    sock->local_ip = is_loopback_ip(dest_ip) ? ((127) | (0 << 8) | (0 << 16) | (1 << 24)) : netif->ip;
    sock->remote_ip = dest_ip;
    sock->remote_port = dest_port;

    /* Send SYN */
    sock->state = TCP_STATE_SYN_SENT;
    uint32_t syn_seq = sock->seq_num;
    tcp_send_segment(netif, sock->local_ip, dest_ip, sock->local_port, dest_port,
                     syn_seq, 0, TCP_FLAG_SYN, NULL, 0);
    sock->seq_num++;

    /* Wait for ESTABLISHED with timeout and retransmit */
    uint64_t start_ms = pit_get_uptime_ms();
    uint64_t last_syn_ms = start_ms;
    while (pit_get_uptime_ms() - start_ms < 4000) {
        e1000_poll_rx();
        if (sock->state == TCP_STATE_ESTABLISHED) {
            return 0; /* Connected */
        }
        if (sock->state == TCP_STATE_CLOSED) {
            return -1; /* Connection refused */
        }
        if (pit_get_uptime_ms() - last_syn_ms >= 500) {
            tcp_send_segment(netif, sock->local_ip, dest_ip, sock->local_port, dest_port,
                             syn_seq, 0, TCP_FLAG_SYN, NULL, 0);
            last_syn_ms = pit_get_uptime_ms();
        }
        sched_sleep(10);
    }

    sock->state = TCP_STATE_CLOSED;
    return -110; /* -ETIMEDOUT */
}

ssize_t tcp_send(struct tcp_socket *sock, const void *data, size_t len) {
    if (!sock || sock->state != TCP_STATE_ESTABLISHED) return -1;

    struct net_if *netif = tcp_route_interface(sock->remote_ip);
    if (!netif) return -1;

    int res = tcp_send_segment(netif, sock->local_ip, sock->remote_ip,
                               sock->local_port, sock->remote_port,
                               sock->seq_num, sock->ack_num,
                               TCP_FLAG_PSH | TCP_FLAG_ACK, data, len);
    if (res == 0) {
        sock->seq_num += (uint32_t)len;
        return (ssize_t)len;
    }
    return -1;
}

ssize_t tcp_recv(struct tcp_socket *sock, void *buf, size_t len, bool nonblock, bool peek) {
    if (!sock || !buf || len == 0) return -1;

    for (;;) {
        if (sock->rx_head != sock->rx_tail) {
            size_t count = 0;
            char *out = (char *)buf;
            uint32_t tail = sock->rx_tail;
            while (count < len && sock->rx_head != tail) {
                out[count++] = sock->rx_buf[tail];
                tail = (tail + 1) % TCP_RX_BUFFER_SIZE;
            }
            if (!peek) {
                sock->rx_tail = tail;
            }
            return (ssize_t)count;
        }

        if (sock->state == TCP_STATE_CLOSED || sock->state == TCP_STATE_CLOSE_WAIT) {
            return 0; /* EOF */
        }

        if (nonblock) {
            return -11; /* -EAGAIN */
        }

        e1000_poll_rx();
        sched_sleep(10);
    }
}

void tcp_handle_packet(struct net_if *netif, uint32_t src_ip, const void *data, size_t len) {
    if (!netif || !data || len < sizeof(struct tcp_hdr)) return;

    const struct tcp_hdr *hdr = (const struct tcp_hdr *)data;
    uint16_t dest_port = ntohs(hdr->dest_port);
    uint16_t src_port = ntohs(hdr->src_port);
    uint32_t seq = ntohl(hdr->seq_num);
    uint32_t ack = ntohl(hdr->ack_num);
    uint8_t flags = hdr->flags;

    size_t header_len = (hdr->data_offset >> 4) * 4;
    if (header_len < sizeof(struct tcp_hdr) || len < header_len) return;

    const uint8_t *payload = (const uint8_t *)data + header_len;
    size_t payload_len = len - header_len;

    /* Find matching socket: Prioritize exact connected match over listening socket */
    struct tcp_socket *sock = NULL;
    struct tcp_socket *listen_sock = NULL;
    struct tcp_socket *curr = tcp_sockets;
    while (curr) {
        if (curr->local_port == dest_port) {
            if (curr->state != TCP_STATE_LISTEN && curr->remote_port == src_port) {
                sock = curr;
                break;
            } else if (curr->state == TCP_STATE_LISTEN) {
                listen_sock = curr;
            }
        }
        curr = curr->next;
    }

    if (!sock) {
        sock = listen_sock;
    }

    struct net_if *reply_if = tcp_route_interface(src_ip);
    if (!reply_if) reply_if = netif;

    if (!sock) {
        /* No socket listening: Send RST */
        if (!(flags & TCP_FLAG_RST)) {
            uint32_t rst_src = is_loopback_ip(src_ip) ? ((127) | (0 << 8) | (0 << 16) | (1 << 24)) : reply_if->ip;
            tcp_send_segment(reply_if, rst_src, src_ip, dest_port, src_port, ack, seq + 1, TCP_FLAG_RST | TCP_FLAG_ACK, NULL, 0);
        }
        return;
    }

    /* Process based on socket state */
    if (sock->state == TCP_STATE_LISTEN && (flags & TCP_FLAG_SYN)) {
        /* Incoming connection on listening socket */
        if (sock->pending_count < TCP_MAX_PENDING_CONNS) {
            struct tcp_socket *child = tcp_socket_create();
            if (child) {
                child->local_ip = is_loopback_ip(src_ip) ? ((127) | (0 << 8) | (0 << 16) | (1 << 24)) : reply_if->ip;
                child->local_port = sock->local_port;
                child->remote_ip = src_ip;
                child->remote_port = src_port;
                child->ack_num = seq + 1;
                child->state = TCP_STATE_ESTABLISHED;

                /* Send SYN-ACK */
                tcp_send_segment(reply_if, child->local_ip, src_ip, child->local_port, src_port,
                                 child->seq_num, child->ack_num, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
                child->seq_num++;

                sock->pending_conns[sock->pending_count++] = child;
            }
        }
    } else if (sock->state == TCP_STATE_SYN_SENT) {
        if (flags & TCP_FLAG_RST) {
            sock->state = TCP_STATE_CLOSED;
            return;
        }
        if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
            sock->ack_num = seq + 1;
            sock->state = TCP_STATE_ESTABLISHED;

            /* Send ACK */
            tcp_send_segment(reply_if, sock->local_ip, src_ip, sock->local_port, src_port,
                             sock->seq_num, sock->ack_num, TCP_FLAG_ACK, NULL, 0);
        }
    } else if (sock->state == TCP_STATE_ESTABLISHED) {
        if (flags & TCP_FLAG_RST) {
            sock->state = TCP_STATE_CLOSED;
            return;
        }

        if (payload_len > 0) {
            /* Queue data into rx buffer */
            for (size_t i = 0; i < payload_len; i++) {
                uint32_t next = (sock->rx_head + 1) % TCP_RX_BUFFER_SIZE;
                if (next != sock->rx_tail) {
                    sock->rx_buf[sock->rx_head] = payload[i];
                    sock->rx_head = next;
                }
            }
            sock->ack_num = seq + (uint32_t)payload_len;

            /* Send ACK */
            tcp_send_segment(reply_if, sock->local_ip, src_ip, sock->local_port, src_port,
                             sock->seq_num, sock->ack_num, TCP_FLAG_ACK, NULL, 0);
        }

        if (flags & TCP_FLAG_FIN) {
            sock->ack_num = seq + 1;
            sock->state = TCP_STATE_CLOSE_WAIT;

            /* Send ACK for FIN */
            tcp_send_segment(reply_if, sock->local_ip, src_ip, sock->local_port, src_port,
                             sock->seq_num, sock->ack_num, TCP_FLAG_ACK, NULL, 0);
        }
    }
}
