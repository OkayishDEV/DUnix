#ifndef _NET_E1000_H
#define _NET_E1000_H

#include <dunix/types.h>
#include <net/net.h>
#include <arch/x86_64/drivers/pci.h>
#include <arch/x86_64/cpu/idt.h>

#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_ID 0x100E

#define E1000_REG_CTRL    0x0000
#define E1000_REG_STATUS  0x0008
#define E1000_REG_EEPROM  0x0014
#define E1000_REG_ICR     0x00C0
#define E1000_REG_IMS     0x00D0
#define E1000_REG_IMC     0x00D8
#define E1000_REG_RCTL    0x0100
#define E1000_REG_TCTL    0x0400
#define E1000_REG_TIPG    0x0410
#define E1000_REG_RDBAL   0x2800
#define E1000_REG_RDBAH   0x2804
#define E1000_REG_RDLEN   0x2808
#define E1000_REG_RDH     0x2810
#define E1000_REG_RDT     0x2818
#define E1000_REG_TDBAL   0x3800
#define E1000_REG_TDBAH   0x3804
#define E1000_REG_TDLEN   0x3808
#define E1000_REG_TDH     0x3810
#define E1000_REG_TDT     0x3818
#define E1000_REG_MTA     0x5200
#define E1000_REG_RAL     0x5400
#define E1000_REG_RAH     0x5404

#define E1000_NUM_RX_DESC 32
#define E1000_NUM_TX_DESC 16
#define E1000_BUFFER_SIZE 2048

struct __attribute__((packed)) e1000_rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
};

struct __attribute__((packed)) e1000_tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
};

int e1000_init(struct pci_device *pci_dev);
int e1000_send(struct net_if *netif, const void *data, size_t len);
void e1000_handle_irq(struct interrupt_frame *frame);
void e1000_poll_rx(void);

#endif /* _NET_E1000_H */
