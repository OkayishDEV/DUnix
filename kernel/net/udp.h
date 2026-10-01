#ifndef _NET_UDP_H
#define _NET_UDP_H

#include <dunix/types.h>
#include <net/net.h>

struct __attribute__((packed)) udp_hdr {
    uint16_t src_port;
    uint16_t dest_port;
    uint16_t length;
    uint16_t checksum;
};

#define UDP_PACKET_BUF_SIZE 2048
#define UDP_QUEUE_SIZE      16

struct udp_packet_entry {
    uint32_t src_ip;
    uint16_t src_port;
    size_t   len;
    uint8_t  data[UDP_PACKET_BUF_SIZE];
    bool     valid;
};

struct udp_socket {
    uint16_t port;
    struct udp_packet_entry queue[UDP_QUEUE_SIZE];
    uint32_t queue_head;
    uint32_t queue_tail;
    bool     bound;
    struct udp_socket *next;
};

void udp_init(void);
struct udp_socket *udp_socket_create(void);
void udp_socket_close(struct udp_socket *sock);
int  udp_bind(struct udp_socket *sock, uint16_t port);
int  udp_send(struct net_if *netif, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const void *data, size_t len);
ssize_t udp_recv(struct udp_socket *sock, void *buf, size_t len, uint32_t *src_ip_out, uint16_t *src_port_out, bool nonblock);
void udp_handle_packet(struct net_if *netif, uint32_t src_ip, const void *data, size_t len);

#endif /* _NET_UDP_H */
