#include <fs/block.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

static struct block_device *bdev_list = NULL;

struct partition_priv {
    struct block_device *parent;
    uint64_t lba_offset;
    uint64_t total_blocks;
};

struct mbr_entry {
    uint8_t  status;
    uint8_t  chs_first[3];
    uint8_t  type;
    uint8_t  chs_last[3];
    uint32_t lba_start;
    uint32_t sector_count;
} __attribute__((packed));

static int partition_bdev_read(struct block_device *bdev, uint64_t lba, size_t count, void *buf) {
    struct partition_priv *priv = (struct partition_priv *)bdev->priv;
    if (!priv || !priv->parent) return -1;
    if (lba + count > priv->total_blocks) return -1;
    return block_device_read(priv->parent, priv->lba_offset + lba, count, buf);
}

static int partition_bdev_write(struct block_device *bdev, uint64_t lba, size_t count, const void *buf) {
    struct partition_priv *priv = (struct partition_priv *)bdev->priv;
    if (!priv || !priv->parent) return -1;
    if (lba + count > priv->total_blocks) return -1;
    return block_device_write(priv->parent, priv->lba_offset + lba, count, buf);
}

static int partition_bdev_flush(struct block_device *bdev) {
    struct partition_priv *priv = (struct partition_priv *)bdev->priv;
    if (!priv || !priv->parent) return -1;
    return priv->parent->flush ? priv->parent->flush(priv->parent) : 0;
}

int block_device_rescan_partitions(struct block_device *bdev) {
    if (!bdev || !bdev->read) return -1;

    uint8_t sector[512];
    if (block_device_read(bdev, 0, 1, sector) != 0) {
        return -1;
    }

    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        return 0;
    }

    struct mbr_entry *entries = (struct mbr_entry *)(sector + 446);
    int registered_count = 0;

    for (int i = 0; i < 4; i++) {
        if (entries[i].type != 0 && entries[i].sector_count > 0) {
            char part_name[32];
            ksnprintf(part_name, sizeof(part_name), "%s%d", bdev->name, i + 1);

            struct block_device *existing = block_device_get_by_name(part_name);
            if (existing) {
                struct partition_priv *priv = (struct partition_priv *)existing->priv;
                if (priv) {
                    priv->lba_offset = entries[i].lba_start;
                    priv->total_blocks = entries[i].sector_count;
                    existing->total_blocks = entries[i].sector_count;
                }
                registered_count++;
                continue;
            }

            struct block_device *part_bdev = (struct block_device *)kzalloc(sizeof(struct block_device));
            struct partition_priv *priv = (struct partition_priv *)kzalloc(sizeof(struct partition_priv));
            if (!part_bdev || !priv) {
                if (part_bdev) kfree(part_bdev);
                if (priv) kfree(priv);
                continue;
            }

            priv->parent = bdev;
            priv->lba_offset = entries[i].lba_start;
            priv->total_blocks = entries[i].sector_count;

            strncpy(part_bdev->name, part_name, sizeof(part_bdev->name) - 1);
            ksnprintf(part_bdev->model, sizeof(part_bdev->model), "%s Partition %d",
                      bdev->model[0] ? bdev->model : "Disk", i + 1);
            part_bdev->block_size = bdev->block_size ? bdev->block_size : 512;
            part_bdev->total_blocks = entries[i].sector_count;
            part_bdev->read = partition_bdev_read;
            part_bdev->write = partition_bdev_write;
            part_bdev->flush = partition_bdev_flush;
            part_bdev->priv = priv;

            block_device_register(part_bdev);
            registered_count++;
        }
    }

    return registered_count;
}

void block_init(void) {
    bdev_list = NULL;
}

int block_device_register(struct block_device *bdev) {
    if (!bdev || !bdev->read) return -1;

    bdev->next = NULL;
    if (!bdev_list) {
        bdev_list = bdev;
    } else {
        struct block_device *curr = bdev_list;
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = bdev;
    }

    vfs_register_block_device(bdev);

    klog(KLOG_INFO, "Block device registered: %s (%lu MB, %lu blocks of %u bytes, model: '%s')\n",
         bdev->name,
         (bdev->total_blocks * bdev->block_size) / (1024 * 1024),
         bdev->total_blocks,
         bdev->block_size,
         bdev->model[0] ? bdev->model : "Generic");

    size_t nlen = strlen(bdev->name);
    bool is_whole_disk = (nlen == 3 && (strncmp(bdev->name, "sd", 2) == 0 || strncmp(bdev->name, "hd", 2) == 0));
    if (is_whole_disk) {
        block_device_rescan_partitions(bdev);
    }

    return 0;
}

struct block_device *block_device_get_by_name(const char *name) {
    if (!name) return NULL;
    struct block_device *curr = bdev_list;
    while (curr) {
        if (strcmp(curr->name, name) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

struct block_device *block_device_get_first(void) {
    return bdev_list;
}

int block_device_read(struct block_device *bdev, uint64_t lba, size_t count, void *buf) {
    if (!bdev || !bdev->read) return -1;
    if (lba + count > bdev->total_blocks) return -1;
    return bdev->read(bdev, lba, count, buf);
}

int block_device_write(struct block_device *bdev, uint64_t lba, size_t count, const void *buf) {
    if (!bdev || !bdev->write) return -1;
    if (lba + count > bdev->total_blocks) return -1;
    return bdev->write(bdev, lba, count, buf);
}

int block_device_sync_all(void) {
    struct block_device *curr = bdev_list;
    while (curr) {
        if (curr->flush) {
            curr->flush(curr);
        }
        curr = curr->next;
    }
    return 0;
}

