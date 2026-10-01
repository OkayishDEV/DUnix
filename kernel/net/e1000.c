#include <net/e1000.h>
#include <net/net.h>
#include <net/arp.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <arch/x86_64/drivers/pic.h>
#include <arch/x86_64/cpu/idt.h>
#include <arch/x86_64/io.h>
#include <dunix/kprintf.h>
#include <dunix/string.h>

static uint64_t e1000_mmio_base = 0;
static struct net_if e1000_netif;

static struct e1000_rx_desc *rx_descs = NULL;
static struct e1000_tx_desc *tx_descs = NULL;
static uint8_t *rx_buffers[E1000_NUM_RX_DESC];
static uint8_t *tx_buffers[E1000_NUM_TX_DESC];

static uint32_t rx_cur = 0;
static uint32_t tx_cur = 0;

static inline void e1000_write32(uint32_t reg, uint32_t val) {
    *((volatile uint32_t *)(e1000_mmio_base + reg)) = val;
}

static inline uint32_t e1000_read32(uint32_t reg) {
    return *((volatile uint32_t *)(e1000_mmio_base + reg));
}

static uint16_t e1000_eeprom_read(uint8_t addr) {
    uint32_t temp = 0;
    e1000_write32(E1000_REG_EEPROM, 1 | ((uint32_t)addr << 8));
    int timeout = 10000;
    while (!((temp = e1000_read32(E1000_REG_EEPROM)) & (1 << 4)) && --timeout > 0) {
        io_wait();
    }
    return (uint16_t)((temp >> 16) & 0xFFFF);
}

static void e1000_read_mac(uint8_t *mac) {
    uint32_t ral = e1000_read32(E1000_REG_RAL);
    uint32_t rah = e1000_read32(E1000_REG_RAH);

    if (ral != 0) {
        mac[0] = (uint8_t)(ral & 0xFF);
        mac[1] = (uint8_t)((ral >> 8) & 0xFF);
        mac[2] = (uint8_t)((ral >> 16) & 0xFF);
        mac[3] = (uint8_t)((ral >> 24) & 0xFF);
        mac[4] = (uint8_t)(rah & 0xFF);
        mac[5] = (uint8_t)((rah >> 8) & 0xFF);
    } else {
        uint16_t val0 = e1000_eeprom_read(0);
        uint16_t val1 = e1000_eeprom_read(1);
        uint16_t val2 = e1000_eeprom_read(2);
        mac[0] = val0 & 0xFF;
        mac[1] = val0 >> 8;
        mac[2] = val1 & 0xFF;
        mac[3] = val1 >> 8;
        mac[4] = val2 & 0xFF;
        mac[5] = val2 >> 8;
    }
}

static void e1000_rx_init(void) {
    /* Allocate RX descriptors */
    paddr_t rx_desc_frame = pmm_alloc_frame();
    rx_descs = (struct e1000_rx_desc *)PHYS_TO_VIRT(rx_desc_frame);
    memset(rx_descs, 0, sizeof(struct e1000_rx_desc) * E1000_NUM_RX_DESC);

    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        paddr_t buf_frame = pmm_alloc_frame();
        rx_buffers[i] = (uint8_t *)PHYS_TO_VIRT(buf_frame);
        rx_descs[i].addr = (uint64_t)buf_frame;
        rx_descs[i].status = 0;
    }

    e1000_write32(E1000_REG_RDBAH, (uint32_t)(rx_desc_frame >> 32));
    e1000_write32(E1000_REG_RDBAL, (uint32_t)(rx_desc_frame & 0xFFFFFFFF));
    e1000_write32(E1000_REG_RDLEN, E1000_NUM_RX_DESC * sizeof(struct e1000_rx_desc));
    e1000_write32(E1000_REG_RDH, 0);
    e1000_write32(E1000_REG_RDT, E1000_NUM_RX_DESC - 1);

    /* RCTL: Enable, broadcast accept, multicast accept, strip CRC, buffer size 2048 (BSIZE=0) */
    e1000_write32(E1000_REG_RCTL, (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 15) | (1 << 26));
}

static void e1000_tx_init(void) {
    /* Allocate TX descriptors */
    paddr_t tx_desc_frame = pmm_alloc_frame();
    tx_descs = (struct e1000_tx_desc *)PHYS_TO_VIRT(tx_desc_frame);
    memset(tx_descs, 0, sizeof(struct e1000_tx_desc) * E1000_NUM_TX_DESC);

    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        paddr_t buf_frame = pmm_alloc_frame();
        tx_buffers[i] = (uint8_t *)PHYS_TO_VIRT(buf_frame);
        tx_descs[i].addr = (uint64_t)buf_frame;
        tx_descs[i].cmd = 0;
        tx_descs[i].status = (1 << 0); /* TXD_STAT_DD */
    }

    e1000_write32(E1000_REG_TDBAH, (uint32_t)(tx_desc_frame >> 32));
    e1000_write32(E1000_REG_TDBAL, (uint32_t)(tx_desc_frame & 0xFFFFFFFF));
    e1000_write32(E1000_REG_TDLEN, E1000_NUM_TX_DESC * sizeof(struct e1000_tx_desc));
    e1000_write32(E1000_REG_TDH, 0);
    e1000_write32(E1000_REG_TDT, 0);

    /* TIPG: IPG values for standard IEEE 802.3 */
    e1000_write32(E1000_REG_TIPG, 10 | (8 << 10) | (6 << 20));

    /* TCTL: Enable, Pad Short Packets, Collision Threshold 0x0F, Collision Distance 0x40 */
    e1000_write32(E1000_REG_TCTL, (1 << 1) | (1 << 3) | (0x0F << 4) | (0x40 << 12));
}

int e1000_send(struct net_if *netif, const void *data, size_t len) {
    if (!data || len == 0 || len > E1000_BUFFER_SIZE) return -1;

    uint32_t idx = tx_cur;
    int tx_timeout = 10000;
    while (!(tx_descs[idx].status & (1 << 0)) && --tx_timeout > 0) {
        io_wait();
    }

    memcpy(tx_buffers[idx], data, len);
    tx_descs[idx].length = (uint16_t)len;
    /* CMD: End Of Packet (EOP = bit 0), Insert FCS/CRC (IFCS = bit 1), Report Status (RS = bit 3) */
    tx_descs[idx].cmd = (1 << 0) | (1 << 1) | (1 << 3);
    tx_descs[idx].status = 0;

    tx_cur = (tx_cur + 1) % E1000_NUM_TX_DESC;
    e1000_write32(E1000_REG_TDT, tx_cur);

    if (netif) {
        netif->tx_packets++;
        netif->tx_bytes += len;
    }

    return 0;
}

void e1000_poll_rx(void) {
    while (rx_descs[rx_cur].status & (1 << 0)) { /* RXD_STAT_DD */
        uint16_t len = rx_descs[rx_cur].length;
        if (len > 0 && len <= E1000_BUFFER_SIZE) {
            e1000_netif.rx_packets++;
            e1000_netif.rx_bytes += len;
            net_handle_packet(&e1000_netif, rx_buffers[rx_cur], len);
        }

        rx_descs[rx_cur].status = 0;
        uint32_t old_tail = rx_cur;
        rx_cur = (rx_cur + 1) % E1000_NUM_RX_DESC;
        e1000_write32(E1000_REG_RDT, old_tail);
    }
}

void e1000_handle_irq(struct interrupt_frame *frame) {
    (void)frame;
    (void)e1000_read32(E1000_REG_ICR);
    e1000_poll_rx();
}

int e1000_init(struct pci_device *pci_dev) {
    if (!pci_dev || pci_dev->vendor_id != 0x8086) return -1;

    pci_enable_bus_mastering(pci_dev);

    /* Map BAR0 (MMIO Base: 128 KB) */
    paddr_t bar0_phys = pci_dev->bar0 & ~0xF;
    vaddr_t bar0_virt = 0xFFFFFFFFB0000000ULL;
    vmm_map_range(vmm_get_kernel_pml4(), bar0_virt, bar0_phys, 128 * 1024, PTE_PRESENT | PTE_WRITABLE | PTE_PCD);
    e1000_mmio_base = bar0_virt;

    /* Read MAC Address */
    e1000_read_mac(e1000_netif.mac);

    /* Reset device (CTRL bit 26) with timeout */
    e1000_write32(E1000_REG_CTRL, e1000_read32(E1000_REG_CTRL) | (1 << 26));
    io_wait();
    int rst_timeout = 10000;
    while ((e1000_read32(E1000_REG_CTRL) & (1 << 26)) && --rst_timeout > 0) {
        io_wait();
    }

    /* Initialize Multicast Table Array to 0 */
    for (int i = 0; i < 128; i++) {
        e1000_write32(E1000_REG_MTA + (i * 4), 0);
    }

    /* Initialize RX and TX */
    e1000_rx_init();
    e1000_tx_init();

    /* Clear and enable Interrupts */
    e1000_read32(E1000_REG_ICR);
    /* IMS: RXO, RXT0, LSC, RXDMT0, TXDW */
    e1000_write32(E1000_REG_IMS, (1 << 1) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 7));

    /* Setup network interface struct */
    strcpy(e1000_netif.name, "eth0");
    /* Default IP: 10.0.2.15 (standard QEMU user network address) */
    e1000_netif.ip = (10) | (0 << 8) | (2 << 16) | (15 << 24);
    e1000_netif.netmask = (255) | (255 << 8) | (255 << 16) | (0 << 24);
    e1000_netif.gateway = (10) | (0 << 8) | (2 << 16) | (2 << 24);
    e1000_netif.flags = IFF_UP | IFF_BROADCAST | IFF_RUNNING;
    e1000_netif.mtu = 1500;
    e1000_netif.send_packet = e1000_send;

    /* Register IRQ if available */
    if (pci_dev->irq_line > 0 && pci_dev->irq_line < 16) {
        uint8_t vector = 32 + pci_dev->irq_line;
        register_interrupt_handler(vector, e1000_handle_irq);
        pic_clear_mask(pci_dev->irq_line);
    }

    /* Restore Receive Address Register (RAL/RAH) with AV bit */
    e1000_write32(E1000_REG_RAL, (uint32_t)e1000_netif.mac[0] |
                                 ((uint32_t)e1000_netif.mac[1] << 8) |
                                 ((uint32_t)e1000_netif.mac[2] << 16) |
                                 ((uint32_t)e1000_netif.mac[3] << 24));
    e1000_write32(E1000_REG_RAH, (uint32_t)e1000_netif.mac[4] |
                                 ((uint32_t)e1000_netif.mac[5] << 8) |
                                 (1U << 31));

    /* Pre-seed QEMU gateway (10.0.2.2) and DNS (10.0.2.3) in ARP cache */
    const uint8_t gw_mac[6] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };
    const uint8_t dns_mac[6] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x03 };
    arp_insert(e1000_netif.gateway, gw_mac);
    arp_insert((10) | (0 << 8) | (2 << 16) | (3 << 24), dns_mac);

    net_register_if(&e1000_netif);

    klog(KLOG_INFO, "Intel E1000 NIC initialized: eth0 MAC %02x:%02x:%02x:%02x:%02x:%02x (IP 10.0.2.15, IRQ %u)\n",
         e1000_netif.mac[0], e1000_netif.mac[1], e1000_netif.mac[2],
         e1000_netif.mac[3], e1000_netif.mac[4], e1000_netif.mac[5], pci_dev->irq_line);

    return 0;
}
