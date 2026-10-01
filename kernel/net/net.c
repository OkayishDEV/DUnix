#include <net/net.h>
#include <net/arp.h>
#include <net/ipv4.h>
#include <net/icmp.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/e1000.h>
#include <arch/x86_64/drivers/pci.h>
#include <mm/heap.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

static struct net_if *net_interfaces = NULL;
static struct net_if loopback_if;

static int loopback_send(struct net_if *netif, const void *data, size_t len) {
    if (!netif || !data || len == 0) return -1;
    netif->tx_packets++;
    netif->tx_bytes += len;

    /* Loop packet directly back into receiver */
    netif->rx_packets++;
    netif->rx_bytes += len;
    net_handle_packet(netif, data, len);
    return 0;
}

void net_register_if(struct net_if *netif) {
    if (!netif) return;
    netif->next = net_interfaces;
    net_interfaces = netif;
}

struct net_if *net_get_default_if(void) {
    struct net_if *curr = net_interfaces;
    while (curr) {
        if (!(curr->flags & IFF_LOOPBACK) && (curr->flags & IFF_UP)) {
            return curr;
        }
        curr = curr->next;
    }
    return &loopback_if;
}

struct net_if *net_get_by_name(const char *name) {
    if (!name) return NULL;
    struct net_if *curr = net_interfaces;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

struct net_if *net_get_all(void) {
    return net_interfaces;
}

int net_send_eth(struct net_if *netif, const uint8_t *dest_mac, uint16_t ethertype, const void *payload, size_t len) {
    if (!netif || !payload || !netif->send_packet) return -1;

    size_t total_len = sizeof(struct eth_hdr) + len;
    uint8_t *frame = (uint8_t *)kmalloc(total_len);
    if (!frame) return -1;

    struct eth_hdr *hdr = (struct eth_hdr *)frame;
    memcpy(hdr->dest_mac, dest_mac, 6);
    memcpy(hdr->src_mac, netif->mac, 6);
    hdr->ethertype = htons(ethertype);

    memcpy(frame + sizeof(struct eth_hdr), payload, len);

    int res = netif->send_packet(netif, frame, total_len);
    kfree(frame);
    return res;
}

void net_handle_packet(struct net_if *netif, const void *data, size_t len) {
    if (!netif || !data || len < sizeof(struct eth_hdr)) return;

    const struct eth_hdr *hdr = (const struct eth_hdr *)data;
    uint16_t ethertype = ntohs(hdr->ethertype);

    const uint8_t *payload = (const uint8_t *)data + sizeof(struct eth_hdr);
    size_t payload_len = len - sizeof(struct eth_hdr);

    switch (ethertype) {
        case ETHERTYPE_ARP:
            arp_handle_packet(netif, payload, payload_len);
            break;
        case ETHERTYPE_IPV4:
            ipv4_handle_packet(netif, payload, payload_len);
            break;
        default:
            break;
    }
}

void net_init(void) {
    net_interfaces = NULL;

    /* Initialize Protocols */
    arp_init();
    ipv4_init();
    icmp_init();
    udp_init();
    tcp_init();

    /* Initialize Loopback interface lo0 */
    memset(&loopback_if, 0, sizeof(loopback_if));
    strcpy(loopback_if.name, "lo0");
    loopback_if.ip = (127) | (0 << 8) | (0 << 16) | (1 << 24); /* 127.0.0.1 */
    loopback_if.netmask = (255) | (0 << 8) | (0 << 16) | (0 << 24); /* 255.0.0.0 */
    loopback_if.gateway = loopback_if.ip;
    loopback_if.flags = IFF_UP | IFF_LOOPBACK | IFF_RUNNING;
    loopback_if.mtu = 65536;
    loopback_if.send_packet = loopback_send;
    net_register_if(&loopback_if);

    /* Scan PCI for Intel E1000 Network Cards */
    struct pci_device *pdev = NULL;
    struct pci_device *curr = pci_get_device_list();
    while (curr) {
        if (curr->vendor_id == 0x8086 && curr->class_code == PCI_CLASS_NETWORK) {
            pdev = curr;
            break;
        }
        curr = curr->next;
    }

    if (pdev) {
        e1000_init(pdev);
    } else {
        klog(KLOG_INFO, "No supported Intel E1000 NIC found, loopback lo0 active\n");
    }

    klog(KLOG_INFO, "Networking subsystem initialized (lo0 127.0.0.1 online)\n");
}
