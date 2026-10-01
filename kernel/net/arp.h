#ifndef _NET_ARP_H
#define _NET_ARP_H

#include <dunix/types.h>
#include <net/net.h>

#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

#define ARP_HW_ETHERNET 1
#define ARP_PROTO_IPV4  0x0800

struct __attribute__((packed)) arp_packet {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_size;
    uint8_t  proto_size;
    uint16_t opcode;
    uint8_t  src_mac[6];
    uint32_t src_ip;
    uint8_t  dest_mac[6];
    uint32_t dest_ip;
};

void arp_init(void);
void arp_handle_packet(struct net_if *netif, const void *data, size_t len);
int  arp_lookup(uint32_t ip, uint8_t *mac_out);
void arp_insert(uint32_t ip, const uint8_t *mac);
int  arp_request(struct net_if *netif, uint32_t target_ip);

#endif /* _NET_ARP_H */
