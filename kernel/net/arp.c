#include <net/arp.h>
#include <net/net.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

#define ARP_CACHE_SIZE 64

struct arp_entry {
    uint32_t ip;
    uint8_t  mac[6];
    bool     valid;
};

static struct arp_entry arp_cache[ARP_CACHE_SIZE];

void arp_init(void) {
    memset(arp_cache, 0, sizeof(arp_cache));
}

int arp_lookup(uint32_t ip, uint8_t *mac_out) {
    /* Broadcast address */
    if (ip == 0xFFFFFFFF) {
        memset(mac_out, 0xFF, 6);
        return 0;
    }

    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            memcpy(mac_out, arp_cache[i].mac, 6);
            return 0;
        }
    }
    return -1; /* Not found */
}

void arp_insert(uint32_t ip, const uint8_t *mac) {
    /* Check if already in cache */
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            memcpy(arp_cache[i].mac, mac, 6);
            return;
        }
    }

    /* Find empty slot or overwrite first */
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) {
            arp_cache[i].ip = ip;
            memcpy(arp_cache[i].mac, mac, 6);
            arp_cache[i].valid = true;
            return;
        }
    }

    arp_cache[0].ip = ip;
    memcpy(arp_cache[0].mac, mac, 6);
    arp_cache[0].valid = true;
}

int arp_request(struct net_if *netif, uint32_t target_ip) {
    if (!netif) return -1;

    struct arp_packet pkt;
    pkt.hw_type = htons(ARP_HW_ETHERNET);
    pkt.proto_type = htons(ARP_PROTO_IPV4);
    pkt.hw_size = 6;
    pkt.proto_size = 4;
    pkt.opcode = htons(ARP_OP_REQUEST);
    memcpy(pkt.src_mac, netif->mac, 6);
    pkt.src_ip = netif->ip;
    memset(pkt.dest_mac, 0x00, 6);
    pkt.dest_ip = target_ip;

    uint8_t broadcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    return net_send_eth(netif, broadcast_mac, ETHERTYPE_ARP, &pkt, sizeof(pkt));
}

void arp_handle_packet(struct net_if *netif, const void *data, size_t len) {
    if (!netif || !data || len < sizeof(struct arp_packet)) return;

    const struct arp_packet *pkt = (const struct arp_packet *)data;
    uint16_t opcode = ntohs(pkt->opcode);

    /* Update ARP cache with sender information */
    arp_insert(pkt->src_ip, pkt->src_mac);

    if (opcode == ARP_OP_REQUEST && pkt->dest_ip == netif->ip) {
        /* Send ARP Reply */
        struct arp_packet reply;
        reply.hw_type = htons(ARP_HW_ETHERNET);
        reply.proto_type = htons(ARP_PROTO_IPV4);
        reply.hw_size = 6;
        reply.proto_size = 4;
        reply.opcode = htons(ARP_OP_REPLY);
        memcpy(reply.src_mac, netif->mac, 6);
        reply.src_ip = netif->ip;
        memcpy(reply.dest_mac, pkt->src_mac, 6);
        reply.dest_ip = pkt->src_ip;

        net_send_eth(netif, pkt->src_mac, ETHERTYPE_ARP, &reply, sizeof(reply));
    }
}
