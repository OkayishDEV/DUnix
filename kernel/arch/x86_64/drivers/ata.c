#include <arch/x86_64/drivers/ata.h>
#include <arch/x86_64/io.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_FEATURES    0x01
#define ATA_REG_SECCOUNT    0x02
#define ATA_REG_LBA0        0x03
#define ATA_REG_LBA1        0x04
#define ATA_REG_LBA2        0x05
#define ATA_REG_HDDEVSEL    0x06
#define ATA_REG_COMMAND     0x07
#define ATA_REG_STATUS      0x07

#define ATA_REG_CONTROL     0x00
#define ATA_REG_ALTSTATUS   0x00

#define ATA_SR_BSY          0x80
#define ATA_SR_DRDY         0x40
#define ATA_SR_DF           0x20
#define ATA_SR_DSC          0x10
#define ATA_SR_DRQ          0x08
#define ATA_SR_CORR         0x04
#define ATA_SR_IDX          0x02
#define ATA_SR_ERR          0x01

#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_IDENTIFY    0xEC

static struct ata_drive ata_drives[ATA_MAX_DRIVES];

static void ata_io_wait(uint16_t ctrl_base) {
    /* Reading AltStatus 4 times gives ~400ns delay */
    inb(ctrl_base + ATA_REG_ALTSTATUS);
    inb(ctrl_base + ATA_REG_ALTSTATUS);
    inb(ctrl_base + ATA_REG_ALTSTATUS);
    inb(ctrl_base + ATA_REG_ALTSTATUS);
}

static int ata_wait_ready(uint16_t io_base, uint16_t ctrl_base) {
    (void)io_base;
    ata_io_wait(ctrl_base);
    for (int i = 0; i < 2000000; i++) {
        uint8_t status = inb(ctrl_base + ATA_REG_ALTSTATUS);
        if (status == 0xFF) return -1;
        if (!(status & (ATA_SR_BSY | ATA_SR_DRQ))) {
            return 0;
        }
        __asm__ volatile("pause" ::: "memory");
    }
    return -1;
}

static int ata_poll(uint16_t io_base, uint16_t ctrl_base, bool check_drq) {
    ata_io_wait(ctrl_base);

    for (int i = 0; i < 2000000; i++) {
        uint8_t status = inb(ctrl_base + ATA_REG_ALTSTATUS);
        if (status == 0xFF) {
            klog(KLOG_ERROR, "ata_poll: status=0xFF (floating)\n");
            return -1; /* Floating bus, no device */
        }
        if (!(status & ATA_SR_BSY)) {
            if (status & ATA_SR_ERR) {
                uint8_t err = inb(io_base + 1);
                klog(KLOG_ERROR, "ata_poll: ERR bit set! status=0x%02x err=0x%02x\n", status, err);
                return -1;
            }
            if (status & ATA_SR_DF) {
                klog(KLOG_ERROR, "ata_poll: DF bit set! status=0x%02x\n", status);
                return -1;
            }
            if (!check_drq || (status & ATA_SR_DRQ)) {
                inb(io_base + ATA_REG_STATUS);
                return 0; /* Ready */
            }
        }
        __asm__ volatile("pause" ::: "memory");
    }
    uint8_t final_st = inb(io_base + ATA_REG_STATUS);
    uint8_t err = inb(io_base + 1);
    klog(KLOG_ERROR, "ata_poll: TIMEOUT! status=0x%02x err=0x%02x check_drq=%d\n", final_st, err, check_drq);
    return -1; /* Timeout */
}

static void ata_fix_string(char *dest, const uint16_t *src, size_t word_count) {
    size_t out_idx = 0;
    for (size_t i = 0; i < word_count; i++) {
        dest[out_idx++] = (char)(src[i] >> 8);
        dest[out_idx++] = (char)(src[i] & 0xFF);
    }
    dest[out_idx] = '\0';

    /* Trim trailing spaces */
    while (out_idx > 0 && (dest[out_idx - 1] == ' ' || dest[out_idx - 1] == '\0')) {
        dest[--out_idx] = '\0';
    }
}

static int ata_bdev_read(struct block_device *bdev, uint64_t lba, size_t count, void *buffer) {
    struct ata_drive *drive = (struct ata_drive *)bdev->priv;
    return ata_read_sectors(drive, lba, count, buffer);
}

static int ata_bdev_write(struct block_device *bdev, uint64_t lba, size_t count, const void *buffer) {
    struct ata_drive *drive = (struct ata_drive *)bdev->priv;
    return ata_write_sectors(drive, lba, count, buffer);
}

static int ata_bdev_flush(struct block_device *bdev) {
    struct ata_drive *drive = (struct ata_drive *)bdev->priv;
    if (!drive || !drive->present) return -1;
    if (ata_wait_ready(drive->io_base, drive->ctrl_base) != 0) return -1;
    outb(drive->io_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    return ata_poll(drive->io_base, drive->ctrl_base, false);
}

static bool ata_identify_drive(struct ata_drive *drive) {
    uint16_t io = drive->io_base;
    uint16_t ctrl = drive->ctrl_base;

    /* Select drive (Master: 0xA0, Slave: 0xB0) */
    outb(io + ATA_REG_HDDEVSEL, drive->drive_type == ATA_MASTER ? 0xA0 : 0xB0);
    ata_io_wait(ctrl);

    /* Zero sector count and LBA registers */
    outb(io + ATA_REG_SECCOUNT, 0);
    outb(io + ATA_REG_LBA0, 0);
    outb(io + ATA_REG_LBA1, 0);
    outb(io + ATA_REG_LBA2, 0);

    /* Send IDENTIFY command */
    outb(io + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_io_wait(ctrl);

    uint8_t status = inb(io + ATA_REG_STATUS);
    if (status == 0 || status == 0xFF) {
        return false; /* No device */
    }

    /* Check for ATAPI / non-ATA device */
    uint8_t lba1 = inb(io + ATA_REG_LBA1);
    uint8_t lba2 = inb(io + ATA_REG_LBA2);
    if ((lba1 == 0x14 && lba2 == 0xEB) || (lba1 == 0x69 && lba2 == 0x96)) {
        /* ATAPI device, skip for standard ATA PIO */
        return false;
    }

    if (ata_poll(io, ctrl, true) != 0) {
        return false;
    }

    /* Read 256 words (512 bytes) identify data */
    uint16_t identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(io + ATA_REG_DATA);
    }

    ata_fix_string(drive->serial, &identify_data[10], 10);
    ata_fix_string(drive->model,  &identify_data[27], 20);

    /* Total sectors in 28-bit or 48-bit LBA mode */
    uint32_t lba28_sectors = (uint32_t)identify_data[60] | ((uint32_t)identify_data[61] << 16);
    uint64_t lba48_sectors = (uint64_t)identify_data[100] |
                             ((uint64_t)identify_data[101] << 16) |
                             ((uint64_t)identify_data[102] << 32) |
                             ((uint64_t)identify_data[103] << 48);
    drive->sectors = (lba48_sectors > 0) ? lba48_sectors : lba28_sectors;
    drive->sector_size = 512;
    drive->present = true;

    /* Populate block device structure */
    strncpy(drive->bdev.model, drive->model, sizeof(drive->bdev.model) - 1);
    drive->bdev.block_size = drive->sector_size;
    drive->bdev.total_blocks = drive->sectors;
    drive->bdev.read = ata_bdev_read;
    drive->bdev.write = ata_bdev_write;
    drive->bdev.flush = ata_bdev_flush;
    drive->bdev.priv = drive;

    return true;
}

int ata_read_sectors(struct ata_drive *drive, uint64_t lba, size_t count, void *buf) {
    if (!drive || !drive->present || !buf) return -1;
    uint16_t io = drive->io_base;
    uint16_t ctrl = drive->ctrl_base;
    uint16_t *dest = (uint16_t *)buf;

    for (size_t s = 0; s < count; s++) {
        uint32_t cur_lba = (uint32_t)(lba + s);

        if (ata_wait_ready(io, ctrl) != 0) {
            klog(KLOG_ERROR, "ATA: drive busy before read at LBA %u\n", cur_lba);
            return -1;
        }

        outb(io + ATA_REG_HDDEVSEL, (drive->drive_type == ATA_MASTER ? 0xE0 : 0xF0) | ((cur_lba >> 24) & 0x0F));
        ata_io_wait(ctrl);

        outb(io + ATA_REG_SECCOUNT, 1);
        outb(io + ATA_REG_LBA0, (uint8_t)(cur_lba & 0xFF));
        outb(io + ATA_REG_LBA1, (uint8_t)((cur_lba >> 8) & 0xFF));
        outb(io + ATA_REG_LBA2, (uint8_t)((cur_lba >> 16) & 0xFF));
        outb(io + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

        if (ata_poll(io, ctrl, true) != 0) {
            return -1;
        }

        for (int i = 0; i < 256; i++) {
            *dest++ = inw(io + ATA_REG_DATA);
        }
    }

    return 0;
}

int ata_write_sectors(struct ata_drive *drive, uint64_t lba, size_t count, const void *buf) {
    if (!drive || !drive->present || !buf) return -1;
    uint16_t io = drive->io_base;
    uint16_t ctrl = drive->ctrl_base;
    const uint16_t *src = (const uint16_t *)buf;

    for (size_t s = 0; s < count; s++) {
        uint32_t cur_lba = (uint32_t)(lba + s);

        if (ata_wait_ready(io, ctrl) != 0) {
            klog(KLOG_ERROR, "ATA: drive busy before write at LBA %u\n", cur_lba);
            return -1;
        }

        outb(io + ATA_REG_HDDEVSEL, (drive->drive_type == ATA_MASTER ? 0xE0 : 0xF0) | ((cur_lba >> 24) & 0x0F));
        ata_io_wait(ctrl);

        outb(io + ATA_REG_SECCOUNT, 1);
        outb(io + ATA_REG_LBA0, (uint8_t)(cur_lba & 0xFF));
        outb(io + ATA_REG_LBA1, (uint8_t)((cur_lba >> 8) & 0xFF));
        outb(io + ATA_REG_LBA2, (uint8_t)((cur_lba >> 16) & 0xFF));
        outb(io + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

        if (ata_poll(io, ctrl, true) != 0) {
            klog(KLOG_ERROR, "ATA: write poll(true) failed at LBA %u (s=%zu/%zu)\n", cur_lba, s, count);
            return -1;
        }

        for (int i = 0; i < 256; i++) {
            outw(io + ATA_REG_DATA, *src++);
        }

        if (ata_poll(io, ctrl, false) != 0) {
            klog(KLOG_ERROR, "ATA: write poll(false) failed at LBA %u (s=%zu/%zu)\n", cur_lba, s, count);
            return -1;
        }
    }

    return 0;
}

void ata_init(void) {
    memset(ata_drives, 0, sizeof(ata_drives));

    /* Configure 4 standard ATA IDE drives */
    ata_drives[0].bus = 0; ata_drives[0].drive_type = ATA_MASTER;
    ata_drives[0].io_base = ATA_PRIMARY_IO; ata_drives[0].ctrl_base = ATA_PRIMARY_CTRL;
    strcpy(ata_drives[0].bdev.name, "hda");

    ata_drives[1].bus = 0; ata_drives[1].drive_type = ATA_SLAVE;
    ata_drives[1].io_base = ATA_PRIMARY_IO; ata_drives[1].ctrl_base = ATA_PRIMARY_CTRL;
    strcpy(ata_drives[1].bdev.name, "hdb");

    ata_drives[2].bus = 1; ata_drives[2].drive_type = ATA_MASTER;
    ata_drives[2].io_base = ATA_SECONDARY_IO; ata_drives[2].ctrl_base = ATA_SECONDARY_CTRL;
    strcpy(ata_drives[2].bdev.name, "hdc");

    ata_drives[3].bus = 1; ata_drives[3].drive_type = ATA_SLAVE;
    ata_drives[3].io_base = ATA_SECONDARY_IO; ata_drives[3].ctrl_base = ATA_SECONDARY_CTRL;
    strcpy(ata_drives[3].bdev.name, "hdd");

    int detected = 0;
    for (int i = 0; i < ATA_MAX_DRIVES; i++) {
        if (ata_identify_drive(&ata_drives[i])) {
            block_device_register(&ata_drives[i].bdev);
            klog(KLOG_INFO, "ATA drive /dev/%s detected: Model '%s', %lu Sectors (%lu MB)\n",
                 ata_drives[i].bdev.name,
                 ata_drives[i].model,
                 ata_drives[i].sectors,
                 (ata_drives[i].sectors * 512) / (1024 * 1024));
            detected++;
        }
    }

    if (detected == 0) {
        klog(KLOG_INFO, "ATA driver initialized (No hardware ATA drives attached)\n");
    }
}
