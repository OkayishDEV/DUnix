#ifndef _ARCH_X86_64_DRIVERS_PCI_H
#define _ARCH_X86_64_DRIVERS_PCI_H

#include <dunix/types.h>

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define PCI_VENDOR_ID      0x00
#define PCI_DEVICE_ID      0x02
#define PCI_COMMAND        0x04
#define PCI_STATUS         0x06
#define PCI_REVISION_ID    0x08
#define PCI_PROG_IF        0x09
#define PCI_SUBCLASS       0x0A
#define PCI_CLASS          0x0B
#define PCI_CACHE_LINE_SZ  0x0C
#define PCI_LATENCY_TIMER  0x0D
#define PCI_HEADER_TYPE    0x0E
#define PCI_BIST           0x0F
#define PCI_BAR0           0x10
#define PCI_BAR1           0x14
#define PCI_BAR2           0x18
#define PCI_BAR3           0x1C
#define PCI_BAR4           0x20
#define PCI_BAR5           0x24
#define PCI_INTERRUPT_LINE 0x3C
#define PCI_INTERRUPT_PIN  0x3D

#define PCI_CLASS_NETWORK  0x02
#define PCI_CLASS_DISPLAY  0x03
#define PCI_CLASS_STORAGE  0x01
#define PCI_CLASS_BRIDGE   0x06

#define PCI_SUBCLASS_IDE   0x01
#define PCI_SUBCLASS_SATA  0x06
#define PCI_SUBCLASS_NVME  0x08

#define PCI_COMMAND_IO          0x01
#define PCI_COMMAND_MEMORY      0x02
#define PCI_COMMAND_BUS_MASTER  0x04

struct pci_device {
    uint8_t  bus;
    uint8_t  device;
    uint8_t  function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t  class_code;
    uint8_t  subclass;
    uint8_t  prog_if;
    uint8_t  irq_line;
    uint64_t bar0;
    uint64_t bar1;
    uint64_t bar2;
    uint64_t bar3;
    uint64_t bar4;
    uint64_t bar5;
    uint32_t bar0_size;
    bool     is_mmio_bar0;
    struct pci_device *next;
};

void pci_init(void);
uint32_t pci_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint16_t pci_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
uint8_t  pci_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
void pci_write16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value);
void pci_write8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value);

void pci_enable_bus_mastering(struct pci_device *dev);
struct pci_device *pci_find_device(uint16_t vendor_id, uint16_t device_id);
struct pci_device *pci_find_class(uint8_t class_code, uint8_t subclass);
struct pci_device *pci_get_device_list(void);

#endif /* _ARCH_X86_64_DRIVERS_PCI_H */
