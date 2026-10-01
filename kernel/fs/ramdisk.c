#include <fs/ramdisk.h>
#include <mm/heap.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

static int ramdisk_bdev_read(struct block_device *bdev, uint64_t lba, size_t count, void *buffer) {
    struct ramdisk *rd = (struct ramdisk *)bdev->priv;
    size_t offset = lba * bdev->block_size;
    size_t bytes = count * bdev->block_size;

    if (offset + bytes > rd->size) return -1;
    memcpy(buffer, rd->data + offset, bytes);
    return 0;
}

static int ramdisk_bdev_write(struct block_device *bdev, uint64_t lba, size_t count, const void *buffer) {
    struct ramdisk *rd = (struct ramdisk *)bdev->priv;
    size_t offset = lba * bdev->block_size;
    size_t bytes = count * bdev->block_size;

    if (offset + bytes > rd->size) return -1;
    memcpy(rd->data + offset, buffer, bytes);
    return 0;
}

static int ramdisk_bdev_flush(struct block_device *bdev) {
    (void)bdev;
    return 0;
}

struct ramdisk *ramdisk_create(const char *name, size_t size_bytes) {
    struct ramdisk *rd = (struct ramdisk *)kmalloc(sizeof(struct ramdisk));
    if (!rd) return NULL;

    rd->data = (uint8_t *)kmalloc(size_bytes);
    if (!rd->data) {
        kfree(rd);
        return NULL;
    }
    memset(rd->data, 0, size_bytes);
    rd->size = size_bytes;

    strncpy(rd->bdev.name, name ? name : "ram0", sizeof(rd->bdev.name) - 1);
    strcpy(rd->bdev.model, "RAM Disk (Live/Scratch)");
    rd->bdev.block_size = BLOCK_SIZE_DEFAULT;
    rd->bdev.total_blocks = size_bytes / BLOCK_SIZE_DEFAULT;
    rd->bdev.read = ramdisk_bdev_read;
    rd->bdev.write = ramdisk_bdev_write;
    rd->bdev.flush = ramdisk_bdev_flush;
    rd->bdev.priv = rd;

    block_device_register(&rd->bdev);
    return rd;
}

void ramdisk_destroy(struct ramdisk *rd) {
    if (!rd) return;
    if (rd->data) kfree(rd->data);
    kfree(rd);
}
