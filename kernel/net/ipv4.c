#include <net/ipv4.h>
#include <net/net.h>
#include <net/arp.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/e1000.h>
#include <arch/x86_64/drivers/pit.h>
#include <sched/sched.h>
#include <mm/heap.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

static uint16_t packet_id_seq = 1;

uint16_t net_checksum(const void *data, size_t len) {
    const uint16_t *buf = (const uint16_t *)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *buf++;
        len -= 2;
    }

    if (len == 1) {
        sum += *(const uint8_t *)buf;
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

void ipv4_init(void) {
    packet_id_seq = 1;
}

int ipv4_send(struct net_if *netif, uint32_t dest_ip, uint8_t proto, const void *payload, size_t len) {
    if (!netif || !payload) return -1;

    size_t total_len = sizeof(struct ipv4_hdr) + len;
    uint8_t *packet = (uint8_t *)kmalloc(total_len);
    if (!packet) return -1;

    struct ipv4_hdr *hdr = (struct ipv4_hdr *)packet;
    hdr->version_ihl = 0x45; /* IPv4, 5 words (20 bytes) */
    hdr->tos = 0;
    hdr->total_len = htons((uint16_t)total_len);
    hdr->id = htons(packet_id_seq++);
    hdr->flags_frag = htons(0x4000); /* Don't Fragment (DF) */
    hdr->ttl = 64;
    hdr->protocol = proto;
    hdr->checksum = 0;
    hdr->src_ip = netif->ip;
    hdr->dest_ip = dest_ip;
    hdr->checksum = net_checksum(hdr, sizeof(struct ipv4_hdr));

    memcpy(packet + sizeof(struct ipv4_hdr), payload, len);

    /* Loopback address handling */
    if ((dest_ip & 0xFF) == 127 || dest_ip == netif->ip) {
        /* Local loopback delivery */
        ipv4_handle_packet(netif, packet, total_len);
        kfree(packet);
        return 0;
    }

    /* Next hop determination */
    uint32_t next_hop_ip = dest_ip;
    if ((dest_ip & netif->netmask) != (netif->ip & netif->netmask)) {
        /* Outside local subnet -> route to default gateway */
        next_hop_ip = netif->gateway;
    }

    uint8_t dest_mac[6];
    if (arp_lookup(next_hop_ip, dest_mac) != 0) {
        /* Send ARP request to resolve MAC */
        arp_request(netif, next_hop_ip);
        uint64_t start_arp = pit_get_uptime_ms();
        while (pit_get_uptime_ms() - start_arp < 50) {
            e1000_poll_rx();
            if (arp_lookup(next_hop_ip, dest_mac) == 0) break;
            sched_sleep(1);
        }
        if (arp_lookup(next_hop_ip, dest_mac) != 0) {
            klog(KLOG_INFO, "[NET] Failed to resolve ARP for %08x\n", next_hop_ip);
            kfree(packet);
            return -1;
        }
    }

    int res = net_send_eth(netif, dest_mac, ETHERTYPE_IPV4, packet, total_len);
    kfree(packet);
    return res;
}

void ipv4_handle_packet(struct net_if *netif, const void *data, size_t len) {
    if (!netif || !data || len < sizeof(struct ipv4_hdr)) return;

    const struct ipv4_hdr *hdr = (const struct ipv4_hdr *)data;
    if ((hdr->version_ihl >> 4) != 4) return; /* Not IPv4 */

    size_t ihl = (hdr->version_ihl & 0x0F) * 4;
    if (ihl < sizeof(struct ipv4_hdr) || len < ihl) return;

    uint16_t total_len = ntohs(hdr->total_len);
    if (len < total_len) return;

    /* Verify checksum */
    if (net_checksum(hdr, ihl) != 0) {
        return; /* Corrupted header */
    }

    const uint8_t *payload = (const uint8_t *)data + ihl;
    size_t payload_len = total_len - ihl;

    switch (hdr->protocol) {
        case IPV4_PROTO_ICMP:
            icmp_handle_packet(netif, hdr->src_ip, payload, payload_len);
            break;
        case IPV4_PROTO_UDP:
            udp_handle_packet(netif, hdr->src_ip, payload, payload_len);
            break;
        case IPV4_PROTO_TCP:
            tcp_handle_packet(netif, hdr->src_ip, payload, payload_len);
            break;
        default:
            break;
    }
}
