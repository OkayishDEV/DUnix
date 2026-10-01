#ifndef _ARCH_X86_64_DRIVERS_ATA_H
#define _ARCH_X86_64_DRIVERS_ATA_H

#include <dunix/types.h>
#include <dunix/stdbool.h>
#include <fs/block.h>

#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376

#define ATA_MASTER          0x00
#define ATA_SLAVE           0x01

#define ATA_MAX_DRIVES      4

struct ata_drive {
    bool                present;
    uint8_t             bus;        /* 0 = primary, 1 = secondary */
    uint8_t             drive_type; /* 0 = master, 1 = slave */
    uint16_t            io_base;
    uint16_t            ctrl_base;
    char                model[41];
    char                serial[21];
    uint64_t            sectors;
    uint32_t            sector_size;
    struct block_device bdev;
};

void ata_init(void);
int  ata_read_sectors(struct ata_drive *drive, uint64_t lba, size_t count, void *buf);
int  ata_write_sectors(struct ata_drive *drive, uint64_t lba, size_t count, const void *buf);

#endif /* _ARCH_X86_64_DRIVERS_ATA_H */
