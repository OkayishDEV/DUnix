#include <arch/x86_64/drivers/ahci.h>
#include <arch/x86_64/drivers/pci.h>
#include <arch/x86_64/io.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

static struct ahci_drive ahci_drives[AHCI_MAX_DRIVES];
static int num_ahci_drives = 0;
static volatile struct ahci_hba_mem *hba_mem = NULL;

static void ahci_fix_string(char *dest, const uint16_t *src, size_t word_count) {
    size_t out_idx = 0;
    for (size_t i = 0; i < word_count; i++) {
        dest[out_idx++] = (char)(src[i] >> 8);
        dest[out_idx++] = (char)(src[i] & 0xFF);
    }
    dest[out_idx] = '\0';

    /* Trim trailing whitespace */
    while (out_idx > 0 && (dest[out_idx - 1] == ' ' || dest[out_idx - 1] == '\0')) {
        dest[--out_idx] = '\0';
    }
}

static inline void ahci_delay(void) {
    __asm__ volatile("pause" ::: "memory");
}

static void ahci_port_stop(volatile struct ahci_port *port) {
    /* If already stopped, nothing to do */
    if (!(port->cmd & (AHCI_PORT_CMD_ST | AHCI_PORT_CMD_CR | AHCI_PORT_CMD_FRE | AHCI_PORT_CMD_FR))) {
        return;
    }

    /* 1. Clear ST (bit 0) */
    port->cmd &= ~AHCI_PORT_CMD_ST;

    /* 2. Wait until CR (bit 15) is cleared */
    for (int i = 0; i < 200000; i++) {
        if (!(port->cmd & AHCI_PORT_CMD_CR)) break;
        ahci_delay();
    }

    /* 3. Clear FRE (bit 4) */
    port->cmd &= ~AHCI_PORT_CMD_FRE;

    /* 4. Wait until FR (bit 14) is cleared */
    for (int i = 0; i < 200000; i++) {
        if (!(port->cmd & AHCI_PORT_CMD_FR)) break;
        ahci_delay();
    }
}

static void ahci_port_start(volatile struct ahci_port *port) {
    /* 1. Wait until CR is cleared */
    for (int i = 0; i < 200000; i++) {
        if (!(port->cmd & AHCI_PORT_CMD_CR)) break;
        ahci_delay();
    }

    /* 2. Set FRE first (AHCI 1.3 spec §10.3.1) */
    port->cmd |= AHCI_PORT_CMD_FRE;

    /* 3. Wait for FR to become 1 before setting ST */
    for (int i = 0; i < 200000; i++) {
        if (port->cmd & AHCI_PORT_CMD_FR) break;
        ahci_delay();
    }

    /* 4. Set ST (bit 0) */
    port->cmd |= AHCI_PORT_CMD_ST;
}

static int ahci_bdev_read(struct block_device *bdev, uint64_t lba, size_t count, void *buffer) {
    struct ahci_drive *drive = (struct ahci_drive *)bdev->priv;
    return ahci_read_sectors(drive, lba, count, buffer);
}

static int ahci_bdev_write(struct block_device *bdev, uint64_t lba, size_t count, const void *buffer) {
    struct ahci_drive *drive = (struct ahci_drive *)bdev->priv;
    return ahci_write_sectors(drive, lba, count, buffer);
}

static int ahci_bdev_flush(struct block_device *bdev) {
    struct ahci_drive *drive = (struct ahci_drive *)bdev->priv;
    if (!drive || !drive->present) return -1;

    volatile struct ahci_port *port = drive->port;

    /* Wait for drive ready (BSY and DRQ clear) */
    for (int w = 0; w < 200000; w++) {
        if (!(port->tfd & (0x80 | 0x08))) break;
        ahci_delay();
    }
    if (port->tfd & (0x80 | 0x08)) return -1;

    struct ahci_cmd_header *cmd_hdr = (struct ahci_cmd_header *)PHYS_TO_VIRT(drive->dma_page_phys);
    struct ahci_cmd_table *cmd_tbl = (struct ahci_cmd_table *)PHYS_TO_VIRT(drive->dma_page_phys + 2048);

    memset(cmd_hdr, 0, sizeof(struct ahci_cmd_header));
    cmd_hdr->cfl = sizeof(struct fis_reg_h2d) / 4;
    cmd_hdr->w = 1;
    cmd_hdr->prdtl = 0;
    cmd_hdr->ctba = (uint32_t)(drive->dma_page_phys + 2048);
    cmd_hdr->ctbau = (uint32_t)((drive->dma_page_phys + 2048) >> 32);

    memset(cmd_tbl, 0, sizeof(struct ahci_cmd_table));
    struct fis_reg_h2d *cfis = (struct fis_reg_h2d *)cmd_tbl->cfis;
    cfis->fis_type = 0x27;
    cfis->pm_c = 0x80;
    cfis->command = ATA_CMD_FLUSH_EXT;
    cfis->device = 1 << 6;

    port->serr = 0xFFFFFFFF;
    port->is = 0xFFFFFFFF;
    port->ci = 1;

    for (int spin = 0; spin < 1000000; spin++) {
        if ((port->ci & 1) == 0) return 0;
        if (port->is & (1 << 30)) break;
        ahci_delay();
    }
    return -1;
}

static bool ahci_identify(struct ahci_drive *drive) {
    volatile struct ahci_port *port = drive->port;

    /* Wait until device is not busy (BSY and DRQ clear) */
    for (int i = 0; i < 200000; i++) {
        if (!(port->tfd & (0x80 | 0x08))) break;
        ahci_delay();
    }
    if (port->tfd & (0x80 | 0x08)) {
        klog(KLOG_WARN, "AHCI: Port %d: Device busy before IDENTIFY (tfd=0x%08x)\n",
             drive->port_no, port->tfd);
        return false;
    }

    struct ahci_cmd_header *cmd_hdr = (struct ahci_cmd_header *)PHYS_TO_VIRT(drive->dma_page_phys);
    struct ahci_cmd_table *cmd_tbl = (struct ahci_cmd_table *)PHYS_TO_VIRT(drive->dma_page_phys + 2048);

    memset(cmd_hdr, 0, sizeof(struct ahci_cmd_header));
    cmd_hdr->cfl = sizeof(struct fis_reg_h2d) / 4;
    cmd_hdr->w = 0;
    cmd_hdr->prdtl = 1;
    cmd_hdr->ctba = (uint32_t)(drive->dma_page_phys + 2048);
    cmd_hdr->ctbau = (uint32_t)((drive->dma_page_phys + 2048) >> 32);

    memset(cmd_tbl, 0, sizeof(struct ahci_cmd_table));
    cmd_tbl->prdt_entries[0].dba = (uint32_t)drive->bounce_phys;
    cmd_tbl->prdt_entries[0].dbau = (uint32_t)(drive->bounce_phys >> 32);
    cmd_tbl->prdt_entries[0].dbc = (512 - 1) | (1U << 31);

    struct fis_reg_h2d *cfis = (struct fis_reg_h2d *)cmd_tbl->cfis;
    cfis->fis_type = 0x27;
    cfis->pm_c = 0x80;
    cfis->command = ATA_CMD_IDENTIFY;
    cfis->device = 0;

    port->serr = 0xFFFFFFFF;
    port->is = 0xFFFFFFFF;
    port->ci = 1;

    for (int spin = 0; spin < 1000000; spin++) {
        if ((port->ci & 1) == 0) break;
        if (port->is & (1 << 30)) {
            /* Task File Error */
            klog(KLOG_WARN, "AHCI: Port %d: IDENTIFY Task File Error (tfd=0x%08x)\n",
                 drive->port_no, port->tfd);
            return false;
        }
        ahci_delay();
    }

    if (port->ci & 1) {
        klog(KLOG_WARN, "AHCI: Port %d: IDENTIFY timed out (ci=0x%08x, tfd=0x%08x)\n",
             drive->port_no, port->ci, port->tfd);
        return false;
    }

    const uint16_t *id_data = (const uint16_t *)PHYS_TO_VIRT(drive->bounce_phys);
    ahci_fix_string(drive->serial, &id_data[10], 10);
    ahci_fix_string(drive->model, &id_data[27], 20);

    /* 48-bit or 28-bit LBA sector count */
    uint32_t lba28 = (uint32_t)id_data[60] | ((uint32_t)id_data[61] << 16);
    uint64_t lba48 = (uint64_t)id_data[100] |
                     ((uint64_t)id_data[101] << 16) |
                     ((uint64_t)id_data[102] << 32) |
                     ((uint64_t)id_data[103] << 48);

    drive->sectors = (lba48 > 0) ? lba48 : lba28;
    drive->sector_size = 512;
    drive->present = true;

    return true;
}

int ahci_read_sectors(struct ahci_drive *drive, uint64_t lba, size_t count, void *buf) {
    if (!drive || !drive->present || !buf || count == 0) return -1;
    if (lba + count > drive->sectors) return -1;

    volatile struct ahci_port *port = drive->port;
    struct ahci_cmd_header *cmd_hdr = (struct ahci_cmd_header *)PHYS_TO_VIRT(drive->dma_page_phys);
    struct ahci_cmd_table *cmd_tbl = (struct ahci_cmd_table *)PHYS_TO_VIRT(drive->dma_page_phys + 2048);
    uint8_t *dst = (uint8_t *)buf;

    uint64_t cur_lba = lba;
    size_t remaining = count;

    while (remaining > 0) {
        size_t chunk = (remaining > 8) ? 8 : remaining; /* 8 sectors = 4096 bytes (1 page) */
        uint32_t bytes = (uint32_t)(chunk * 512);

        /* Ensure device is ready */
        for (int w = 0; w < 200000; w++) {
            if (!(port->tfd & (0x80 | 0x08))) break;
            ahci_delay();
        }
        if (port->tfd & (0x80 | 0x08)) return -1;

        memset(cmd_hdr, 0, sizeof(struct ahci_cmd_header));
        cmd_hdr->cfl = sizeof(struct fis_reg_h2d) / 4;
        cmd_hdr->w = 0; /* Read */
        cmd_hdr->prdtl = 1;
        cmd_hdr->ctba = (uint32_t)(drive->dma_page_phys + 2048);
        cmd_hdr->ctbau = (uint32_t)((drive->dma_page_phys + 2048) >> 32);

        memset(cmd_tbl, 0, sizeof(struct ahci_cmd_table));
        cmd_tbl->prdt_entries[0].dba = (uint32_t)drive->bounce_phys;
        cmd_tbl->prdt_entries[0].dbau = (uint32_t)(drive->bounce_phys >> 32);
        cmd_tbl->prdt_entries[0].dbc = (bytes - 1) | (1U << 31);

        struct fis_reg_h2d *cfis = (struct fis_reg_h2d *)cmd_tbl->cfis;
        cfis->fis_type = 0x27;
        cfis->pm_c = 0x80;
        cfis->command = ATA_CMD_READ_DMA_EXT;
        cfis->device = 1 << 6; /* LBA */
        cfis->lba0 = (uint8_t)(cur_lba & 0xFF);
        cfis->lba1 = (uint8_t)((cur_lba >> 8) & 0xFF);
        cfis->lba2 = (uint8_t)((cur_lba >> 16) & 0xFF);
        cfis->lba3 = (uint8_t)((cur_lba >> 24) & 0xFF);
        cfis->lba4 = (uint8_t)((cur_lba >> 32) & 0xFF);
        cfis->lba5 = (uint8_t)((cur_lba >> 40) & 0xFF);
        cfis->count_low = (uint8_t)(chunk & 0xFF);
        cfis->count_high = (uint8_t)((chunk >> 8) & 0xFF);

        port->serr = 0xFFFFFFFF;
        port->is = 0xFFFFFFFF;
        port->ci = 1;

        for (int spin = 0; spin < 1000000; spin++) {
            if ((port->ci & 1) == 0) break;
            if (port->is & (1 << 30)) return -1;
            ahci_delay();
        }

        if (port->ci & 1) return -1;

        memcpy(dst, (const void *)PHYS_TO_VIRT(drive->bounce_phys), bytes);

        dst += bytes;
        cur_lba += chunk;
        remaining -= chunk;
    }

    return 0;
}

int ahci_write_sectors(struct ahci_drive *drive, uint64_t lba, size_t count, const void *buf) {
    if (!drive || !drive->present || !buf || count == 0) return -1;
    if (lba + count > drive->sectors) return -1;

    volatile struct ahci_port *port = drive->port;
    struct ahci_cmd_header *cmd_hdr = (struct ahci_cmd_header *)PHYS_TO_VIRT(drive->dma_page_phys);
    struct ahci_cmd_table *cmd_tbl = (struct ahci_cmd_table *)PHYS_TO_VIRT(drive->dma_page_phys + 2048);
    const uint8_t *src = (const uint8_t *)buf;

    uint64_t cur_lba = lba;
    size_t remaining = count;

    while (remaining > 0) {
        size_t chunk = (remaining > 8) ? 8 : remaining;
        uint32_t bytes = (uint32_t)(chunk * 512);

        /* Ensure device is ready */
        for (int w = 0; w < 200000; w++) {
            if (!(port->tfd & (0x80 | 0x08))) break;
            ahci_delay();
        }
        if (port->tfd & (0x80 | 0x08)) return -1;

        memcpy((void *)PHYS_TO_VIRT(drive->bounce_phys), src, bytes);

        memset(cmd_hdr, 0, sizeof(struct ahci_cmd_header));
        cmd_hdr->cfl = sizeof(struct fis_reg_h2d) / 4;
        cmd_hdr->w = 1; /* Write */
        cmd_hdr->prdtl = 1;
        cmd_hdr->ctba = (uint32_t)(drive->dma_page_phys + 2048);
        cmd_hdr->ctbau = (uint32_t)((drive->dma_page_phys + 2048) >> 32);

        memset(cmd_tbl, 0, sizeof(struct ahci_cmd_table));
        cmd_tbl->prdt_entries[0].dba = (uint32_t)drive->bounce_phys;
        cmd_tbl->prdt_entries[0].dbau = (uint32_t)(drive->bounce_phys >> 32);
        cmd_tbl->prdt_entries[0].dbc = (bytes - 1) | (1U << 31);

        struct fis_reg_h2d *cfis = (struct fis_reg_h2d *)cmd_tbl->cfis;
        cfis->fis_type = 0x27;
        cfis->pm_c = 0x80;
        cfis->command = ATA_CMD_WRITE_DMA_EXT;
        cfis->device = 1 << 6; /* LBA */
        cfis->lba0 = (uint8_t)(cur_lba & 0xFF);
        cfis->lba1 = (uint8_t)((cur_lba >> 8) & 0xFF);
        cfis->lba2 = (uint8_t)((cur_lba >> 16) & 0xFF);
        cfis->lba3 = (uint8_t)((cur_lba >> 24) & 0xFF);
        cfis->lba4 = (uint8_t)((cur_lba >> 32) & 0xFF);
        cfis->lba5 = (uint8_t)((cur_lba >> 40) & 0xFF);
        cfis->count_low = (uint8_t)(chunk & 0xFF);
        cfis->count_high = (uint8_t)((chunk >> 8) & 0xFF);

        port->serr = 0xFFFFFFFF;
        port->is = 0xFFFFFFFF;
        port->ci = 1;

        for (int spin = 0; spin < 1000000; spin++) {
            if ((port->ci & 1) == 0) break;
            if (port->is & (1 << 30)) return -1;
            ahci_delay();
        }

        if (port->ci & 1) return -1;

        src += bytes;
        cur_lba += chunk;
        remaining -= chunk;
    }

    return 0;
}

void ahci_init(void) {
    memset(ahci_drives, 0, sizeof(ahci_drives));
    num_ahci_drives = 0;

    /* Scan PCI for SATA AHCI controller */
    struct pci_device *pdev = NULL;
    struct pci_device *curr = pci_get_device_list();
    while (curr) {
        if (curr->class_code == PCI_CLASS_STORAGE &&
            (curr->subclass == PCI_SUBCLASS_SATA || curr->prog_if == 0x01)) {
            pdev = curr;
            break;
        }
        curr = curr->next;
    }

    if (!pdev) {
        klog(KLOG_INFO, "AHCI: No SATA AHCI controller detected on PCI bus\n");
        return;
    }

    pci_enable_bus_mastering(pdev);

    uint64_t abar_phys = pdev->bar5;
    if (abar_phys == 0) {
        uint32_t b5 = pci_read32(pdev->bus, pdev->device, pdev->function, PCI_BAR5);
        abar_phys = b5 & ~0xFULL;
    }

    if (abar_phys == 0) {
        klog(KLOG_WARN, "AHCI: Invalid ABAR (BAR5 is 0)\n");
        return;
    }

    /* Map ABAR MMIO (64 KB) */
    vmm_map_range(vmm_get_kernel_pml4(), AHCI_MMIO_BASE, abar_phys, 0x10000,
                  PTE_PRESENT | PTE_WRITABLE | PTE_PCD);

    hba_mem = (volatile struct ahci_hba_mem *)AHCI_MMIO_BASE;

    /* Enable AHCI Mode (GHC.AE) */
    hba_mem->ghc |= AHCI_GHC_AE;

    /* BIOS / OS Handoff if supported */
    if (hba_mem->cap2 & (1U << 0)) {
        hba_mem->bohc |= (1U << 1); /* Request OS ownership */
        for (int i = 0; i < 50000; i++) {
            if (!(hba_mem->bohc & (1U << 0))) break;
            ahci_delay();
        }
    }

    uint32_t pi = hba_mem->pi;
    klog(KLOG_INFO, "AHCI: Controller %04x:%04x online (ABAR=0x%lx, Ports Implemented: 0x%08x)\n",
         pdev->vendor_id, pdev->device_id, abar_phys, pi);

    /* Power up and spin up devices on all implemented ports (crucial for AMD FCH) */
    for (int p = 0; p < 32; p++) {
        if (!(pi & (1U << p))) continue;
        volatile struct ahci_port *port = &hba_mem->ports[p];
        port->cmd |= AHCI_PORT_CMD_POD | AHCI_PORT_CMD_SUD;
    }

    /* Brief pause for PHY link establishment */
    for (volatile int d = 0; d < 100000; d++) {
        ahci_delay();
    }

    for (int p = 0; p < 32 && num_ahci_drives < AHCI_MAX_DRIVES; p++) {
        if (!(pi & (1U << p))) continue;

        volatile struct ahci_port *port = &hba_mem->ports[p];

        /* Check device detection (SSTS.DET) */
        uint8_t det = (uint8_t)(port->ssts & 0x0F);
        if (det == 1) {
            /* Presence detected but link negotiating, wait briefly */
            for (int wait = 0; wait < 100000; wait++) {
                det = (uint8_t)(port->ssts & 0x0F);
                if (det == 3) break;
                ahci_delay();
            }
        }

        if (det != 3) {
            continue; /* No physical connection */
        }

        uint32_t sig = port->sig;
        klog(KLOG_INFO, "AHCI: Port %d: Device connected (ssts=0x%08x, sig=0x%08x)\n",
             p, port->ssts, sig);

        if (sig == AHCI_DEV_SATAPI) {
            klog(KLOG_INFO, "AHCI: Port %d: SATAPI optical drive detected (skipping for block storage)\n", p);
            continue;
        }

        /* Stop port to configure structures */
        ahci_port_stop(port);

        /* Allocate 2 physical pages: page 0 = DMA structures, page 1 = bounce buffer */
        paddr_t p0 = pmm_alloc_frame();
        paddr_t p1 = pmm_alloc_frame();
        if (!p0 || !p1) {
            klog(KLOG_WARN, "AHCI: Failed to allocate DMA frames for port %d\n", p);
            continue;
        }

        memset((void *)PHYS_TO_VIRT(p0), 0, PAGE_SIZE_4K);
        memset((void *)PHYS_TO_VIRT(p1), 0, PAGE_SIZE_4K);

        /* Setup command list & FIS addresses */
        port->clb = (uint32_t)p0;
        port->clbu = (uint32_t)(p0 >> 32);
        port->fb = (uint32_t)(p0 + 1024);
        port->fbu = (uint32_t)((p0 + 1024) >> 32);

        port->serr = 0xFFFFFFFF;
        port->is = 0xFFFFFFFF;

        /* Start port */
        ahci_port_start(port);

        struct ahci_drive *drive = &ahci_drives[num_ahci_drives];
        drive->port_no = (uint8_t)p;
        drive->port = port;
        drive->dma_page_phys = p0;
        drive->bounce_phys = p1;

        if (ahci_identify(drive)) {
            char drive_letter = 'a' + num_ahci_drives;
            ksnprintf(drive->bdev.name, sizeof(drive->bdev.name), "sd%c", drive_letter);
            strncpy(drive->bdev.model, drive->model, sizeof(drive->bdev.model) - 1);
            drive->bdev.total_blocks = drive->sectors;
            drive->bdev.block_size = drive->sector_size;
            drive->bdev.read = ahci_bdev_read;
            drive->bdev.write = ahci_bdev_write;
            drive->bdev.flush = ahci_bdev_flush;
            drive->bdev.priv = drive;

            block_device_register(&drive->bdev);

            uint64_t size_mb = (drive->sectors * 512) / (1024 * 1024);
            klog(KLOG_INFO, "AHCI: /dev/%s detected: Model '%s', %lu Sectors (%lu MB / %lu GB)\n",
                 drive->bdev.name, drive->model, drive->sectors, size_mb, size_mb / 1024);

            num_ahci_drives++;
        } else {
            klog(KLOG_WARN, "AHCI: Port %d: IDENTIFY command failed\n", p);
            ahci_port_stop(port);
            pmm_free_frame(p0);
            pmm_free_frame(p1);
        }
    }

    klog(KLOG_INFO, "AHCI: Controller initialized (%d active SATA disk(s))\n", num_ahci_drives);
}
