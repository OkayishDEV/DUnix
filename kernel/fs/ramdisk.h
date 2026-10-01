#ifndef _FS_RAMDISK_H
#define _FS_RAMDISK_H

#include <fs/block.h>

struct ramdisk {
    uint8_t             *data;
    size_t               size;
    struct block_device  bdev;
};

struct ramdisk *ramdisk_create(const char *name, size_t size_bytes);
void            ramdisk_destroy(struct ramdisk *rd);

#endif /* _FS_RAMDISK_H */
