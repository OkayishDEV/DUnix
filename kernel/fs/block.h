#ifndef _FS_BLOCK_H
#define _FS_BLOCK_H

#include <dunix/types.h>
#include <dunix/stdbool.h>

#define BLOCK_SIZE_DEFAULT 512

struct block_device;

typedef int (*block_read_t)(struct block_device *bdev, uint64_t block_lba, size_t count, void *buffer);
typedef int (*block_write_t)(struct block_device *bdev, uint64_t block_lba, size_t count, const void *buffer);
typedef int (*block_flush_t)(struct block_device *bdev);

struct block_device {
    char                 name[32];
    char                 model[48];
    uint32_t             block_size;
    uint64_t             total_blocks;
    block_read_t         read;
    block_write_t        write;
    block_flush_t        flush;
    void                *priv;
    struct block_device *next;
};

#define BLKRRPART 0x125F

void                 block_init(void);
int                  block_device_register(struct block_device *bdev);
int                  block_device_rescan_partitions(struct block_device *bdev);
struct block_device *block_device_get_by_name(const char *name);
struct block_device *block_device_get_first(void);
int                  block_device_read(struct block_device *bdev, uint64_t lba, size_t count, void *buf);
int                  block_device_write(struct block_device *bdev, uint64_t lba, size_t count, const void *buf);
int                  block_device_sync_all(void);

#endif /* _FS_BLOCK_H */
