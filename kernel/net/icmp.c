#include <net/icmp.h>
#include <net/ipv4.h>
#include <net/net.h>
#include <arch/x86_64/drivers/pit.h>
#include <sched/sched.h>
#include <mm/heap.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

#define MAX_PENDING_REPLIES 16
static struct icmp_echo_reply pending_replies[MAX_PENDING_REPLIES];

void icmp_init(void) {
    memset(pending_replies, 0, sizeof(pending_replies));
}

int icmp_send_echo_request(struct net_if *netif, uint32_t dest_ip, uint16_t id, uint16_t seq, const void *data, size_t len) {
    if (!netif) return -1;

    size_t total_len = sizeof(struct icmp_hdr) + len;
    uint8_t *packet = (uint8_t *)kmalloc(total_len);
    if (!packet) return -1;

    struct icmp_hdr *hdr = (struct icmp_hdr *)packet;
    hdr->type = ICMP_TYPE_ECHO_REQUEST;
    hdr->code = 0;
    hdr->checksum = 0;
    hdr->id = htons(id);
    hdr->sequence = htons(seq);

    if (data && len > 0) {
        memcpy(packet + sizeof(struct icmp_hdr), data, len);
    }

    hdr->checksum = net_checksum(packet, total_len);

    int res = ipv4_send(netif, dest_ip, IPV4_PROTO_ICMP, packet, total_len);
    kfree(packet);
    return res;
}

void icmp_handle_packet(struct net_if *netif, uint32_t src_ip, const void *data, size_t len) {
    if (!netif || !data || len < sizeof(struct icmp_hdr)) return;

    const struct icmp_hdr *hdr = (const struct icmp_hdr *)data;

    if (hdr->type == ICMP_TYPE_ECHO_REQUEST) {
        /* Send Echo Reply */
        size_t reply_len = len;
        uint8_t *reply_buf = (uint8_t *)kmalloc(reply_len);
        if (!reply_buf) return;

        memcpy(reply_buf, data, reply_len);
        struct icmp_hdr *reply_hdr = (struct icmp_hdr *)reply_buf;
        reply_hdr->type = ICMP_TYPE_ECHO_REPLY;
        reply_hdr->code = 0;
        reply_hdr->checksum = 0;
        reply_hdr->checksum = net_checksum(reply_buf, reply_len);

        ipv4_send(netif, src_ip, IPV4_PROTO_ICMP, reply_buf, reply_len);
        kfree(reply_buf);
    } else if (hdr->type == ICMP_TYPE_ECHO_REPLY) {
        /* Store reply */
        uint16_t id = ntohs(hdr->id);
        uint16_t seq = ntohs(hdr->sequence);

        for (int i = 0; i < MAX_PENDING_REPLIES; i++) {
            if (!pending_replies[i].valid) {
                pending_replies[i].src_ip = src_ip;
                pending_replies[i].id = id;
                pending_replies[i].sequence = seq;
                pending_replies[i].timestamp_ms = pit_get_uptime_ms();
                pending_replies[i].valid = true;
                break;
            }
        }
    }
}

int icmp_wait_echo_reply(uint16_t id, uint16_t seq, uint32_t timeout_ms, struct icmp_echo_reply *reply_out) {
    uint64_t start_ms = pit_get_uptime_ms();

    while (pit_get_uptime_ms() - start_ms < timeout_ms) {
        for (int i = 0; i < MAX_PENDING_REPLIES; i++) {
            if (pending_replies[i].valid && pending_replies[i].id == id && pending_replies[i].sequence == seq) {
                if (reply_out) {
                    *reply_out = pending_replies[i];
                }
                pending_replies[i].valid = false;
                return 0; /* Success */
            }
        }
        sched_sleep(10);
    }

    return -1; /* Timeout */
}
