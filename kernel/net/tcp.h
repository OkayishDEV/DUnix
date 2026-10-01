#ifndef _NET_TCP_H
#define _NET_TCP_H

#include <dunix/types.h>
#include <net/net.h>

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

enum tcp_state {
    TCP_STATE_CLOSED = 0,
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RECEIVED,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_LAST_ACK,
    TCP_STATE_TIME_WAIT
};

struct __attribute__((packed)) tcp_hdr {
    uint16_t src_port;
    uint16_t dest_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset; /* (offset >> 4) = header length in 32-bit words */
    uint8_t  flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
};

struct __attribute__((packed)) tcp_pseudo_hdr {
    uint32_t src_ip;
    uint32_t dest_ip;
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t tcp_len;
};

#define TCP_RX_BUFFER_SIZE 32768
#define TCP_MAX_PENDING_CONNS 16

struct tcp_socket {
    enum tcp_state state;
    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;

    uint32_t seq_num;
    uint32_t ack_num;

    /* Receive ring buffer */
    uint8_t  rx_buf[TCP_RX_BUFFER_SIZE];
    uint32_t rx_head;
    uint32_t rx_tail;

    /* Backlog for listening socket */
    struct tcp_socket *pending_conns[TCP_MAX_PENDING_CONNS];
    int pending_count;

    struct tcp_socket *next;
};

void tcp_init(void);
struct tcp_socket *tcp_socket_create(void);
void tcp_socket_close(struct tcp_socket *sock);
int  tcp_bind(struct tcp_socket *sock, uint16_t port);
int  tcp_listen(struct tcp_socket *sock, int backlog);
struct tcp_socket *tcp_accept(struct tcp_socket *sock, uint32_t *client_ip, uint16_t *client_port, bool nonblock);
int  tcp_connect(struct tcp_socket *sock, uint32_t dest_ip, uint16_t dest_port);
ssize_t tcp_send(struct tcp_socket *sock, const void *data, size_t len);
ssize_t tcp_recv(struct tcp_socket *sock, void *buf, size_t len, bool nonblock, bool peek);
void tcp_handle_packet(struct net_if *netif, uint32_t src_ip, const void *data, size_t len);

#endif /* _NET_TCP_H */
