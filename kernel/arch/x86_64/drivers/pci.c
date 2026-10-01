#include <arch/x86_64/drivers/pci.h>
#include <arch/x86_64/io.h>
#include <mm/heap.h>
#include <dunix/kprintf.h>
#include <dunix/string.h>

static struct pci_device *pci_devices = NULL;

static inline uint32_t pci_get_addr(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    return (uint32_t)((1U << 31) | ((uint32_t)bus << 16) | ((uint32_t)device << 11) |
                      ((uint32_t)function << 8) | (offset & 0xFC));
}

uint32_t pci_read32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_get_addr(bus, device, function, offset));
    return inl(PCI_CONFIG_DATA);
}

uint16_t pci_read16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t val = pci_read32(bus, device, function, offset);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_read8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    uint32_t val = pci_read32(bus, device, function, offset);
    return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

void pci_write32(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    outl(PCI_CONFIG_ADDRESS, pci_get_addr(bus, device, function, offset));
    outl(PCI_CONFIG_DATA, value);
}

void pci_write16(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint16_t value) {
    uint32_t old_val = pci_read32(bus, device, function, offset);
    uint32_t mask = 0xFFFF << ((offset & 2) * 8);
    uint32_t new_val = (old_val & ~mask) | (((uint32_t)value) << ((offset & 2) * 8));
    pci_write32(bus, device, function, offset, new_val);
}

void pci_write8(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint8_t value) {
    uint32_t old_val = pci_read32(bus, device, function, offset);
    uint32_t mask = 0xFF << ((offset & 3) * 8);
    uint32_t new_val = (old_val & ~mask) | (((uint32_t)value) << ((offset & 3) * 8));
    pci_write32(bus, device, function, offset, new_val);
}

void pci_enable_bus_mastering(struct pci_device *dev) {
    if (!dev) return;
    uint16_t cmd = pci_read16(dev->bus, dev->device, dev->function, PCI_COMMAND);
    cmd |= (PCI_COMMAND_BUS_MASTER | PCI_COMMAND_MEMORY | PCI_COMMAND_IO);
    pci_write16(dev->bus, dev->device, dev->function, PCI_COMMAND, cmd);
}

static bool bus_scanned[256];
static void pci_scan_bus(uint8_t bus);

static void pci_scan_function(uint8_t bus, uint8_t device, uint8_t function) {
    uint16_t vendor_id = pci_read16(bus, device, function, PCI_VENDOR_ID);
    if (vendor_id == 0xFFFF || vendor_id == 0x0000) {
        return;
    }

    uint16_t device_id = pci_read16(bus, device, function, PCI_DEVICE_ID);
    uint8_t class_code = pci_read8(bus, device, function, PCI_CLASS);
    uint8_t subclass   = pci_read8(bus, device, function, PCI_SUBCLASS);
    uint8_t prog_if    = pci_read8(bus, device, function, PCI_PROG_IF);
    uint8_t irq_line   = pci_read8(bus, device, function, PCI_INTERRUPT_LINE);

    struct pci_device *dev = (struct pci_device *)kzalloc(sizeof(struct pci_device));
    if (!dev) return;

    dev->bus = bus;
    dev->device = device;
    dev->function = function;
    dev->vendor_id = vendor_id;
    dev->device_id = device_id;
    dev->class_code = class_code;
    dev->subclass = subclass;
    dev->prog_if = prog_if;
    dev->irq_line = irq_line;

    /* Read BAR0 */
    uint32_t bar0 = pci_read32(bus, device, function, PCI_BAR0);
    if (bar0 & 1) {
        /* I/O Port */
        dev->bar0 = bar0 & ~0x3;
        dev->is_mmio_bar0 = false;
    } else {
        /* MMIO */
        dev->is_mmio_bar0 = true;
        if ((bar0 & 0x06) == 0x04) {
            /* 64-bit BAR */
            uint32_t bar1 = pci_read32(bus, device, function, PCI_BAR1);
            dev->bar0 = ((uint64_t)bar1 << 32) | (bar0 & ~0xFULL);
        } else {
            dev->bar0 = bar0 & ~0xFULL;
        }
    }

    /* Read BAR1 */
    uint32_t bar1 = pci_read32(bus, device, function, PCI_BAR1);
    dev->bar1 = (bar1 & 1) ? (bar1 & ~0x3) : (bar1 & ~0xF);

    /* Read BAR2, BAR3, BAR4, BAR5 (ABAR) */
    uint32_t bar2 = pci_read32(bus, device, function, PCI_BAR2);
    dev->bar2 = (bar2 & 1) ? (bar2 & ~0x3) : (bar2 & ~0xF);

    uint32_t bar3 = pci_read32(bus, device, function, PCI_BAR3);
    dev->bar3 = (bar3 & 1) ? (bar3 & ~0x3) : (bar3 & ~0xF);

    uint32_t bar4 = pci_read32(bus, device, function, PCI_BAR4);
    dev->bar4 = (bar4 & 1) ? (bar4 & ~0x3) : (bar4 & ~0xF);

    uint32_t bar5 = pci_read32(bus, device, function, PCI_BAR5);
    dev->bar5 = (bar5 & 1) ? (bar5 & ~0x3) : (bar5 & ~0xFULL);

    /* Link device */
    dev->next = pci_devices;
    pci_devices = dev;

    klog(KLOG_INFO, "PCI [%02x:%02x.%d] %04x:%04x (Class %02x:%02x, IRQ %u, BAR0 0x%lx, BAR5 0x%lx)\n",
         bus, device, function, vendor_id, device_id, class_code, subclass, irq_line, dev->bar0, dev->bar5);

    /* If PCI-to-PCI bridge (Class 06:04), recursively scan secondary bus */
    if (class_code == PCI_CLASS_BRIDGE && subclass == 0x04) {
        uint8_t sec_bus = pci_read8(bus, device, function, 0x19);
        if (sec_bus > 0 && !bus_scanned[sec_bus]) {
            pci_scan_bus(sec_bus);
        }
    }
}

static void pci_scan_device(uint8_t bus, uint8_t device) {
    uint16_t vendor_id = pci_read16(bus, device, 0, PCI_VENDOR_ID);
    if (vendor_id == 0xFFFF || vendor_id == 0x0000) {
        return;
    }

    pci_scan_function(bus, device, 0);

    uint8_t header_type = pci_read8(bus, device, 0, PCI_HEADER_TYPE);
    if (header_type & 0x80) {
        /* Multi-function device */
        for (uint8_t fn = 1; fn < 8; fn++) {
            pci_scan_function(bus, device, fn);
        }
    }
}

static void pci_scan_bus(uint8_t bus) {
    if (bus_scanned[bus]) return;
    bus_scanned[bus] = true;

    for (uint8_t dev = 0; dev < 32; dev++) {
        pci_scan_device(bus, dev);
    }
}

void pci_init(void) {
    pci_devices = NULL;
    memset(bus_scanned, 0, sizeof(bus_scanned));

    /* Check Host Bridge (00:00.0) */
    uint8_t header_type = pci_read8(0, 0, 0, PCI_HEADER_TYPE);
    if ((header_type & 0x80) == 0) {
        /* Single host bridge: start from bus 0 */
        pci_scan_bus(0);
    } else {
        /* Multiple host bridges */
        for (uint8_t fn = 0; fn < 8; fn++) {
            if (pci_read16(0, 0, fn, PCI_VENDOR_ID) != 0xFFFF) {
                pci_scan_bus(fn);
            }
        }
    }

    klog(KLOG_INFO, "PCI bus scan completed\n");
}

struct pci_device *pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    struct pci_device *curr = pci_devices;
    while (curr) {
        if (curr->vendor_id == vendor_id && curr->device_id == device_id) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

struct pci_device *pci_find_class(uint8_t class_code, uint8_t subclass) {
    struct pci_device *curr = pci_devices;
    while (curr) {
        if (curr->class_code == class_code && (subclass == 0xFF || curr->subclass == subclass)) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

struct pci_device *pci_get_device_list(void) {
    return pci_devices;
}
