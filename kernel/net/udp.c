#include <net/udp.h>
#include <net/ipv4.h>
#include <net/net.h>
#include <mm/heap.h>
#include <sched/sched.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

static struct udp_socket *udp_sockets = NULL;
static uint16_t next_ephemeral_port = 49152;

void udp_init(void) {
    udp_sockets = NULL;
    next_ephemeral_port = 49152;
}

struct udp_socket *udp_socket_create(void) {
    struct udp_socket *sock = (struct udp_socket *)kzalloc(sizeof(struct udp_socket));
    if (!sock) return NULL;

    sock->port = next_ephemeral_port++;
    if (next_ephemeral_port >= 65535) next_ephemeral_port = 49152;
    sock->bound = false;

    sock->next = udp_sockets;
    udp_sockets = sock;
    return sock;
}

void udp_socket_close(struct udp_socket *sock) {
    if (!sock) return;

    if (udp_sockets == sock) {
        udp_sockets = sock->next;
    } else {
        struct udp_socket *curr = udp_sockets;
        while (curr && curr->next != sock) {
            curr = curr->next;
        }
        if (curr) {
            curr->next = sock->next;
        }
    }
    kfree(sock);
}

int udp_bind(struct udp_socket *sock, uint16_t port) {
    if (!sock) return -1;
    sock->port = port;
    sock->bound = true;
    return 0;
}

int udp_send(struct net_if *netif, uint32_t dest_ip, uint16_t src_port, uint16_t dest_port, const void *data, size_t len) {
    if (!netif || (!data && len > 0)) return -1;

    size_t total_len = sizeof(struct udp_hdr) + len;
    uint8_t *packet = (uint8_t *)kmalloc(total_len);
    if (!packet) return -1;

    struct udp_hdr *hdr = (struct udp_hdr *)packet;
    hdr->src_port = htons(src_port);
    hdr->dest_port = htons(dest_port);
    hdr->length = htons((uint16_t)total_len);
    hdr->checksum = 0; /* Optional in IPv4 */

    if (data && len > 0) {
        memcpy(packet + sizeof(struct udp_hdr), data, len);
    }

    int res = ipv4_send(netif, dest_ip, IPV4_PROTO_UDP, packet, total_len);
    kfree(packet);
    return res;
}

ssize_t udp_recv(struct udp_socket *sock, void *buf, size_t len, uint32_t *src_ip_out, uint16_t *src_port_out, bool nonblock) {
    if (!sock || !buf || len == 0) return -1;

    for (;;) {
        if (sock->queue_head != sock->queue_tail) {
            struct udp_packet_entry *entry = &sock->queue[sock->queue_tail];
            sock->queue_tail = (sock->queue_tail + 1) % UDP_QUEUE_SIZE;

            if (src_ip_out) *src_ip_out = entry->src_ip;
            if (src_port_out) *src_port_out = entry->src_port;

            size_t to_copy = (len < entry->len) ? len : entry->len;
            memcpy(buf, entry->data, to_copy);
            return (ssize_t)to_copy;
        }

        if (nonblock) {
            return -11; /* -EAGAIN */
        }

        sched_sleep(10);
    }
}

void udp_handle_packet(struct net_if *netif, uint32_t src_ip, const void *data, size_t len) {
    (void)netif;
    if (!data || len < sizeof(struct udp_hdr)) return;

    const struct udp_hdr *hdr = (const struct udp_hdr *)data;
    uint16_t dest_port = ntohs(hdr->dest_port);
    uint16_t src_port = ntohs(hdr->src_port);
    uint16_t udp_len = ntohs(hdr->length);

    if (udp_len < sizeof(struct udp_hdr) || len < udp_len) return;

    const uint8_t *payload = (const uint8_t *)data + sizeof(struct udp_hdr);
    size_t payload_len = udp_len - sizeof(struct udp_hdr);

    /* Find matching socket */
    struct udp_socket *curr = udp_sockets;
    while (curr) {
        if (curr->port == dest_port) {
            uint32_t next_head = (curr->queue_head + 1) % UDP_QUEUE_SIZE;
            if (next_head != curr->queue_tail) {
                struct udp_packet_entry *entry = &curr->queue[curr->queue_head];
                entry->src_ip = src_ip;
                entry->src_port = src_port;
                entry->len = (payload_len < UDP_PACKET_BUF_SIZE) ? payload_len : UDP_PACKET_BUF_SIZE;
                memcpy(entry->data, payload, entry->len);
                entry->valid = true;
                curr->queue_head = next_head;
            }
            break;
        }
        curr = curr->next;
    }
}
