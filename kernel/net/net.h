#ifndef _NET_NET_H
#define _NET_NET_H

#include <dunix/types.h>

#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_ARP  0x0806

#define IFF_UP        0x0001
#define IFF_BROADCAST 0x0002
#define IFF_LOOPBACK  0x0008
#define IFF_RUNNING   0x0040

struct __attribute__((packed)) eth_hdr {
    uint8_t  dest_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;
};

struct net_if {
    char     name[16];
    uint8_t  mac[6];
    uint32_t ip;        /* Network byte order */
    uint32_t netmask;   /* Network byte order */
    uint32_t gateway;   /* Network byte order */
    uint32_t flags;
    uint32_t mtu;
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;

    int (*send_packet)(struct net_if *netif, const void *data, size_t len);
    struct net_if *next;
};

/* Endian conversion utilities */
static inline uint16_t htons(uint16_t val) {
    return (uint16_t)((val << 8) | (val >> 8));
}

static inline uint16_t ntohs(uint16_t val) {
    return htons(val);
}

static inline uint32_t htonl(uint32_t val) {
    return ((val & 0x000000FF) << 24) |
           ((val & 0x0000FF00) << 8)  |
           ((val & 0x00FF0000) >> 8)  |
           ((val & 0xFF000000) >> 24);
}

static inline uint32_t ntohl(uint32_t val) {
    return htonl(val);
}

void net_init(void);
void net_register_if(struct net_if *netif);
struct net_if *net_get_default_if(void);
struct net_if *net_get_by_name(const char *name);
struct net_if *net_get_all(void);

int net_send_eth(struct net_if *netif, const uint8_t *dest_mac, uint16_t ethertype, const void *payload, size_t len);
void net_handle_packet(struct net_if *netif, const void *data, size_t len);

#endif /* _NET_NET_H */
