#ifndef _NET_ICMP_H
#define _NET_ICMP_H

#include <dunix/types.h>
#include <net/net.h>

#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

struct __attribute__((packed)) icmp_hdr {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
};

void icmp_init(void);
int  icmp_send_echo_request(struct net_if *netif, uint32_t dest_ip, uint16_t id, uint16_t seq, const void *data, size_t len);
void icmp_handle_packet(struct net_if *netif, uint32_t src_ip, const void *data, size_t len);

/* Ping reply notification queue */
struct icmp_echo_reply {
    uint32_t src_ip;
    uint16_t id;
    uint16_t sequence;
    uint64_t timestamp_ms;
    bool     valid;
};

int icmp_wait_echo_reply(uint16_t id, uint16_t seq, uint32_t timeout_ms, struct icmp_echo_reply *reply_out);

#endif /* _NET_ICMP_H */
