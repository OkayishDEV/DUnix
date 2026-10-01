#ifndef _NET_IPV4_H
#define _NET_IPV4_H

#include <dunix/types.h>
#include <net/net.h>

#define IPV4_PROTO_ICMP 1
#define IPV4_PROTO_TCP  6
#define IPV4_PROTO_UDP  17

struct __attribute__((packed)) ipv4_hdr {
    uint8_t  version_ihl;
    uint8_t  tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dest_ip;
};

uint16_t net_checksum(const void *data, size_t len);
void ipv4_init(void);
int  ipv4_send(struct net_if *netif, uint32_t dest_ip, uint8_t proto, const void *payload, size_t len);
void ipv4_handle_packet(struct net_if *netif, const void *data, size_t len);

#endif /* _NET_IPV4_H */
