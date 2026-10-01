#include <fs/ext2.h>
#include <mm/heap.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

static struct vfs_ops ext2_ops;

static int ext2_read_block(struct ext2_fs *fs, uint32_t block_num, void *buf) {
    if (!fs || block_num == 0) {
        memset(buf, 0, fs->block_size);
        return 0;
    }
    uint64_t lba = (uint64_t)block_num * fs->sectors_per_block;
    return block_device_read(fs->bdev, lba, fs->sectors_per_block, buf);
}

static int ext2_write_block(struct ext2_fs *fs, uint32_t block_num, const void *buf) {
    if (!fs || block_num == 0) return -1;
    uint64_t lba = (uint64_t)block_num * fs->sectors_per_block;
    return block_device_write(fs->bdev, lba, fs->sectors_per_block, buf);
}

static int ext2_read_inode(struct ext2_fs *fs, uint32_t inode_num, struct ext2_inode *inode_out) {
    if (!fs || inode_num == 0 || inode_num > fs->sb.s_inodes_count) return -1;

    uint32_t bg = (inode_num - 1) / fs->sb.s_inodes_per_group;
    uint32_t index = (inode_num - 1) % fs->sb.s_inodes_per_group;

    uint32_t table_block = fs->bgd[bg].bg_inode_table;
    uint32_t block_offset = (index * fs->inode_size) / fs->block_size;
    uint32_t offset_in_block = (index * fs->inode_size) % fs->block_size;

    uint8_t *buf = (uint8_t *)kmalloc(fs->block_size);
    if (!buf) return -1;

    if (ext2_read_block(fs, table_block + block_offset, buf) != 0) {
        kfree(buf);
        return -1;
    }

    memcpy(inode_out, buf + offset_in_block, sizeof(struct ext2_inode));
    kfree(buf);
    return 0;
}

static int ext2_write_inode(struct ext2_fs *fs, uint32_t inode_num, const struct ext2_inode *inode_in) {
    if (!fs || inode_num == 0 || inode_num > fs->sb.s_inodes_count) return -1;

    uint32_t bg = (inode_num - 1) / fs->sb.s_inodes_per_group;
    uint32_t index = (inode_num - 1) % fs->sb.s_inodes_per_group;

    uint32_t table_block = fs->bgd[bg].bg_inode_table;
    uint32_t block_offset = (index * fs->inode_size) / fs->block_size;
    uint32_t offset_in_block = (index * fs->inode_size) % fs->block_size;

    uint8_t *buf = (uint8_t *)kmalloc(fs->block_size);
    if (!buf) return -1;

    if (ext2_read_block(fs, table_block + block_offset, buf) != 0) {
        kfree(buf);
        return -1;
    }

    memcpy(buf + offset_in_block, inode_in, sizeof(struct ext2_inode));
    int res = ext2_write_block(fs, table_block + block_offset, buf);
    kfree(buf);
    return res;
}

static uint32_t ext2_alloc_block(struct ext2_fs *fs) {
    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size);
    if (!bitmap) return 0;

    for (uint32_t g = 0; g < fs->num_groups; g++) {
        if (fs->bgd[g].bg_free_blocks_count == 0) continue;

        if (ext2_read_block(fs, fs->bgd[g].bg_block_bitmap, bitmap) != 0) continue;

        for (uint32_t i = 0; i < fs->sb.s_blocks_per_group; i++) {
            if (!(bitmap[i / 8] & (1 << (i % 8)))) {
                bitmap[i / 8] |= (1 << (i % 8));
                ext2_write_block(fs, fs->bgd[g].bg_block_bitmap, bitmap);

                fs->bgd[g].bg_free_blocks_count--;
                fs->sb.s_free_blocks_count--;

                uint32_t block = g * fs->sb.s_blocks_per_group + i + fs->sb.s_first_data_block;

                /* Clear newly allocated block */
                memset(bitmap, 0, fs->block_size);
                ext2_write_block(fs, block, bitmap);

                kfree(bitmap);
                return block;
            }
        }
    }

    kfree(bitmap);
    return 0;
}

static uint32_t ext2_alloc_inode(struct ext2_fs *fs, bool is_dir) {
    uint8_t *bitmap = (uint8_t *)kmalloc(fs->block_size);
    if (!bitmap) return 0;

    for (uint32_t g = 0; g < fs->num_groups; g++) {
        if (fs->bgd[g].bg_free_inodes_count == 0) continue;

        if (ext2_read_block(fs, fs->bgd[g].bg_inode_bitmap, bitmap) != 0) continue;

        for (uint32_t i = 0; i < fs->sb.s_inodes_per_group; i++) {
            if (!(bitmap[i / 8] & (1 << (i % 8)))) {
                bitmap[i / 8] |= (1 << (i % 8));
                ext2_write_block(fs, fs->bgd[g].bg_inode_bitmap, bitmap);

                fs->bgd[g].bg_free_inodes_count--;
                if (is_dir) {
                    fs->bgd[g].bg_used_dirs_count++;
                }
                fs->sb.s_free_inodes_count--;

                uint32_t inode_num = g * fs->sb.s_inodes_per_group + i + 1;
                kfree(bitmap);
                return inode_num;
            }
        }
    }

    kfree(bitmap);
    return 0;
}

static uint32_t ext2_get_block_number(struct ext2_fs *fs, struct ext2_inode *inode, uint32_t block_index, bool allocate) {
    uint32_t ptrs_per_block = fs->block_size / sizeof(uint32_t);

    /* Direct blocks (0..11) */
    if (block_index < 12) {
        if (inode->i_block[block_index] == 0 && allocate) {
            inode->i_block[block_index] = ext2_alloc_block(fs);
            inode->i_blocks += fs->sectors_per_block;
        }
        return inode->i_block[block_index];
    }

    block_index -= 12;

    /* Singly Indirect (block 12) */
    if (block_index < ptrs_per_block) {
        if (inode->i_block[12] == 0) {
            if (!allocate) return 0;
            inode->i_block[12] = ext2_alloc_block(fs);
            inode->i_blocks += fs->sectors_per_block;
        }

        uint32_t *indirect = (uint32_t *)kmalloc(fs->block_size);
        if (!indirect) return 0;
        ext2_read_block(fs, inode->i_block[12], indirect);

        if (indirect[block_index] == 0 && allocate) {
            indirect[block_index] = ext2_alloc_block(fs);
            inode->i_blocks += fs->sectors_per_block;
            ext2_write_block(fs, inode->i_block[12], indirect);
        }

        uint32_t res = indirect[block_index];
        kfree(indirect);
        return res;
    }

    block_index -= ptrs_per_block;

    /* Doubly Indirect (block 13) */
    uint32_t d_limit = ptrs_per_block * ptrs_per_block;
    if (block_index < d_limit) {
        if (inode->i_block[13] == 0) {
            if (!allocate) return 0;
            inode->i_block[13] = ext2_alloc_block(fs);
            inode->i_blocks += fs->sectors_per_block;
        }

        uint32_t d_idx = block_index / ptrs_per_block;
        uint32_t s_idx = block_index % ptrs_per_block;

        uint32_t *d_indirect = (uint32_t *)kmalloc(fs->block_size);
        if (!d_indirect) return 0;
        ext2_read_block(fs, inode->i_block[13], d_indirect);

        if (d_indirect[d_idx] == 0) {
            if (!allocate) { kfree(d_indirect); return 0; }
            d_indirect[d_idx] = ext2_alloc_block(fs);
            inode->i_blocks += fs->sectors_per_block;
            ext2_write_block(fs, inode->i_block[13], d_indirect);
        }

        uint32_t *s_indirect = (uint32_t *)kmalloc(fs->block_size);
        if (!s_indirect) { kfree(d_indirect); return 0; }
        ext2_read_block(fs, d_indirect[d_idx], s_indirect);

        if (s_indirect[s_idx] == 0 && allocate) {
            s_indirect[s_idx] = ext2_alloc_block(fs);
            inode->i_blocks += fs->sectors_per_block;
            ext2_write_block(fs, d_indirect[d_idx], s_indirect);
        }

        uint32_t res = s_indirect[s_idx];
        kfree(s_indirect);
        kfree(d_indirect);
        return res;
    }

    return 0;
}

static ssize_t ext2_vfs_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    if (!node || !buffer) return -1;
    struct ext2_fs *fs = (struct ext2_fs *)node->device;
    if (!fs) return -1;

    struct ext2_inode inode;
    if (ext2_read_inode(fs, node->inode, &inode) != 0) return -1;

    if (offset >= inode.i_size) return 0;
    if (offset + size > inode.i_size) {
        size = (size_t)(inode.i_size - offset);
    }

    uint8_t *block_buf = (uint8_t *)kmalloc(fs->block_size);
    if (!block_buf) return -1;

    size_t bytes_read = 0;
    while (bytes_read < size) {
        uint32_t block_idx = (uint32_t)((offset + bytes_read) / fs->block_size);
        size_t block_off = (size_t)((offset + bytes_read) % fs->block_size);
        size_t to_read = fs->block_size - block_off;
        if (to_read > size - bytes_read) to_read = size - bytes_read;

        uint32_t p_block = ext2_get_block_number(fs, &inode, block_idx, false);
        if (p_block != 0) {
            ext2_read_block(fs, p_block, block_buf);
            memcpy((uint8_t *)buffer + bytes_read, block_buf + block_off, to_read);
        } else {
            memset((uint8_t *)buffer + bytes_read, 0, to_read);
        }

        bytes_read += to_read;
    }

    kfree(block_buf);
    return (ssize_t)bytes_read;
}

static ssize_t ext2_vfs_write(struct vfs_node *node, uint64_t offset, size_t size, const void *buffer) {
    if (!node || !buffer) return -1;
    struct ext2_fs *fs = (struct ext2_fs *)node->device;
    if (!fs) return -1;

    struct ext2_inode inode;
    if (ext2_read_inode(fs, node->inode, &inode) != 0) return -1;

    uint8_t *block_buf = (uint8_t *)kmalloc(fs->block_size);
    if (!block_buf) return -1;

    size_t bytes_written = 0;
    while (bytes_written < size) {
        uint32_t block_idx = (uint32_t)((offset + bytes_written) / fs->block_size);
        size_t block_off = (size_t)((offset + bytes_written) % fs->block_size);
        size_t to_write = fs->block_size - block_off;
        if (to_write > size - bytes_written) to_write = size - bytes_written;

        uint32_t p_block = ext2_get_block_number(fs, &inode, block_idx, true);
        if (p_block == 0) {
            kfree(block_buf);
            return -1;
        }

        if (block_off != 0 || to_write < fs->block_size) {
            ext2_read_block(fs, p_block, block_buf);
        }

        memcpy(block_buf + block_off, (const uint8_t *)buffer + bytes_written, to_write);
        ext2_write_block(fs, p_block, block_buf);

        bytes_written += to_write;
    }

    if (offset + size > inode.i_size) {
        inode.i_size = (uint32_t)(offset + size);
        node->length = inode.i_size;
    }

    ext2_write_inode(fs, node->inode, &inode);
    kfree(block_buf);
    return (ssize_t)bytes_written;
}

static struct dirent g_ext2_dirent;

static struct dirent *ext2_vfs_readdir(struct vfs_node *node, uint32_t index) {
    if (!node) return NULL;
    struct ext2_fs *fs = (struct ext2_fs *)node->device;
    if (!fs) return NULL;

    struct ext2_inode inode;
    if (ext2_read_inode(fs, node->inode, &inode) != 0) return NULL;
    if (!(inode.i_mode & EXT2_S_IFDIR)) return NULL;

    uint8_t *buf = (uint8_t *)kmalloc(inode.i_size);
    if (!buf) return NULL;

    if (ext2_vfs_read(node, 0, inode.i_size, buf) <= 0) {
        kfree(buf);
        return NULL;
    }

    uint32_t curr_idx = 0;
    size_t offset = 0;
    while (offset < inode.i_size) {
        struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(buf + offset);
        if (entry->rec_len == 0) break;

        if (entry->inode != 0) {
            if (curr_idx == index) {
                g_ext2_dirent.d_ino = entry->inode;
                g_ext2_dirent.d_type = (entry->file_type == EXT2_FT_DIR) ? 2 : 1;
                size_t nlen = entry->name_len < 127 ? entry->name_len : 127;
                memcpy(g_ext2_dirent.d_name, entry->name, nlen);
                g_ext2_dirent.d_name[nlen] = '\0';
                kfree(buf);
                return &g_ext2_dirent;
            }
            curr_idx++;
        }
        offset += entry->rec_len;
    }

    kfree(buf);
    return NULL;
}

static struct vfs_node *ext2_vfs_finddir(struct vfs_node *node, const char *name) {
    if (!node || !name) return NULL;
    struct ext2_fs *fs = (struct ext2_fs *)node->device;
    if (!fs) return NULL;

    struct ext2_inode inode;
    if (ext2_read_inode(fs, node->inode, &inode) != 0) return NULL;
    if (!(inode.i_mode & EXT2_S_IFDIR)) return NULL;

    uint8_t *buf = (uint8_t *)kmalloc(inode.i_size);
    if (!buf) return NULL;

    if (ext2_vfs_read(node, 0, inode.i_size, buf) <= 0) {
        kfree(buf);
        return NULL;
    }

    size_t name_len = strlen(name);
    size_t offset = 0;
    while (offset < inode.i_size) {
        struct ext2_dir_entry_2 *entry = (struct ext2_dir_entry_2 *)(buf + offset);
        if (entry->rec_len == 0) break;

        if (entry->inode != 0 && entry->name_len == name_len) {
            if (memcmp(entry->name, name, name_len) == 0) {
                struct vfs_node *found = (struct vfs_node *)kzalloc(sizeof(struct vfs_node));
                if (!found) { kfree(buf); return NULL; }

                strncpy(found->name, name, sizeof(found->name) - 1);
                found->inode = entry->inode;
                found->device = fs;
                found->ops = &ext2_ops;

                struct ext2_inode child_in;
                ext2_read_inode(fs, entry->inode, &child_in);
                found->length = child_in.i_size;
                found->mask = child_in.i_mode & 0777;
                found->uid = child_in.i_uid;
                found->gid = child_in.i_gid;

                if (child_in.i_mode & EXT2_S_IFDIR) {
                    found->flags = VFS_DIRECTORY;
                } else if (child_in.i_mode & EXT2_S_IFCHR) {
                    found->flags = VFS_CHARDEVICE;
                } else if (child_in.i_mode & EXT2_S_IFBLK) {
                    found->flags = VFS_BLOCKDEVICE;
                } else {
                    found->flags = VFS_FILE;
                }

                kfree(buf);
                return found;
            }
        }
        offset += entry->rec_len;
    }

    kfree(buf);
    return NULL;
}

static int ext2_add_dir_entry(struct vfs_node *parent, uint32_t inode_num, const char *name, uint8_t file_type) {
    struct ext2_fs *fs = (struct ext2_fs *)parent->device;
    struct ext2_inode p_inode;
    if (ext2_read_inode(fs, parent->inode, &p_inode) != 0) return -1;

    size_t name_len = strlen(name);
    size_t entry_size = sizeof(struct ext2_dir_entry_2) + name_len;
    entry_size = (entry_size + 3) & ~3; /* 4-byte align */

    uint8_t *buf = (uint8_t *)kmalloc(fs->block_size);
    if (!buf) return -1;

    if (p_inode.i_size == 0) {
        uint32_t p_block = ext2_get_block_number(fs, &p_inode, 0, true);
        if (!p_block) { kfree(buf); return -1; }
        memset(buf, 0, fs->block_size);
        struct ext2_dir_entry_2 *new_e = (struct ext2_dir_entry_2 *)buf;
        new_e->inode = inode_num;
        new_e->rec_len = (uint16_t)fs->block_size;
        new_e->name_len = (uint8_t)name_len;
        new_e->file_type = file_type;
        memcpy(new_e->name, name, name_len);

        ext2_write_block(fs, p_block, buf);
        p_inode.i_size = fs->block_size;
        ext2_write_inode(fs, parent->inode, &p_inode);
        kfree(buf);
        return 0;
    }

    /* Append to last block */
    uint32_t last_block_idx = (p_inode.i_size - 1) / fs->block_size;
    uint32_t p_block = ext2_get_block_number(fs, &p_inode, last_block_idx, true);
    ext2_read_block(fs, p_block, buf);

    size_t offset = 0;
    struct ext2_dir_entry_2 *last_entry = NULL;
    while (offset < fs->block_size) {
        struct ext2_dir_entry_2 *curr = (struct ext2_dir_entry_2 *)(buf + offset);
        if (curr->rec_len == 0) break;
        last_entry = curr;
        offset += curr->rec_len;
    }

    if (last_entry) {
        size_t actual_last_size = sizeof(struct ext2_dir_entry_2) + last_entry->name_len;
        actual_last_size = (actual_last_size + 3) & ~3;
        size_t remaining = last_entry->rec_len - actual_last_size;

        if (remaining >= entry_size) {
            last_entry->rec_len = (uint16_t)actual_last_size;
            struct ext2_dir_entry_2 *new_e = (struct ext2_dir_entry_2 *)((uint8_t *)last_entry + actual_last_size);
            new_e->inode = inode_num;
            new_e->rec_len = (uint16_t)remaining;
            new_e->name_len = (uint8_t)name_len;
            new_e->file_type = file_type;
            memcpy(new_e->name, name, name_len);

            ext2_write_block(fs, p_block, buf);
            kfree(buf);
            return 0;
        }
    }

    /* Allocate new block for directory entry */
    uint32_t new_b_idx = last_block_idx + 1;
    uint32_t new_p_block = ext2_get_block_number(fs, &p_inode, new_b_idx, true);
    memset(buf, 0, fs->block_size);

    struct ext2_dir_entry_2 *new_e = (struct ext2_dir_entry_2 *)buf;
    new_e->inode = inode_num;
    new_e->rec_len = (uint16_t)fs->block_size;
    new_e->name_len = (uint8_t)name_len;
    new_e->file_type = file_type;
    memcpy(new_e->name, name, name_len);

    ext2_write_block(fs, new_p_block, buf);
    p_inode.i_size += fs->block_size;
    ext2_write_inode(fs, parent->inode, &p_inode);

    kfree(buf);
    return 0;
}

static int ext2_vfs_create(struct vfs_node *parent, const char *name, mode_t mode) {
    struct ext2_fs *fs = (struct ext2_fs *)parent->device;
    uint32_t ino = ext2_alloc_inode(fs, false);
    if (!ino) return -1;

    struct ext2_inode inode;
    memset(&inode, 0, sizeof(struct ext2_inode));
    inode.i_mode = EXT2_S_IFREG | (mode & 0777);
    inode.i_links_count = 1;
    ext2_write_inode(fs, ino, &inode);

    return ext2_add_dir_entry(parent, ino, name, EXT2_FT_REG_FILE);
}

static int ext2_vfs_mkdir(struct vfs_node *parent, const char *name, mode_t mode) {
    struct ext2_fs *fs = (struct ext2_fs *)parent->device;
    uint32_t ino = ext2_alloc_inode(fs, true);
    if (!ino) return -1;

    uint32_t dir_block = ext2_alloc_block(fs);
    if (!dir_block) return -1;

    struct ext2_inode inode;
    memset(&inode, 0, sizeof(struct ext2_inode));
    inode.i_mode = EXT2_S_IFDIR | (mode & 0777);
    inode.i_links_count = 2; /* . and parent */
    inode.i_size = fs->block_size;
    inode.i_blocks = fs->sectors_per_block;
    inode.i_block[0] = dir_block;
    ext2_write_inode(fs, ino, &inode);

    /* Initialize '.' and '..' */
    uint8_t *dir_buf = (uint8_t *)kmalloc(fs->block_size);
    if (!dir_buf) return -1;
    memset(dir_buf, 0, fs->block_size);

    struct ext2_dir_entry_2 *dot = (struct ext2_dir_entry_2 *)dir_buf;
    dot->inode = ino;
    dot->rec_len = 12;
    dot->name_len = 1;
    dot->file_type = EXT2_FT_DIR;
    dot->name[0] = '.';

    struct ext2_dir_entry_2 *dotdot = (struct ext2_dir_entry_2 *)(dir_buf + 12);
    dotdot->inode = parent->inode;
    dotdot->rec_len = (uint16_t)(fs->block_size - 12);
    dotdot->name_len = 2;
    dotdot->file_type = EXT2_FT_DIR;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';

    ext2_write_block(fs, dir_block, dir_buf);
    kfree(dir_buf);

    int res = ext2_add_dir_entry(parent, ino, name, EXT2_FT_DIR);
    if (res == 0) {
        struct ext2_inode parent_in;
        if (ext2_read_inode(fs, parent->inode, &parent_in) == 0) {
            parent_in.i_links_count++;
            ext2_write_inode(fs, parent->inode, &parent_in);
        }
    }
    return res;
}

struct vfs_node *ext2_mount(struct block_device *bdev) {
    if (!bdev) return NULL;

    struct ext2_fs *fs = (struct ext2_fs *)kzalloc(sizeof(struct ext2_fs));
    if (!fs) return NULL;

    fs->bdev = bdev;

    /* Read Superblock at 1024-byte offset (LBA 2) */
    uint8_t sb_buf[1024];
    if (block_device_read(bdev, 2, 2, sb_buf) != 0) {
        kfree(fs);
        return NULL;
    }
    memcpy(&fs->sb, sb_buf, sizeof(struct ext2_superblock));

    if (fs->sb.s_magic != EXT2_SUPER_MAGIC) {
        klog(KLOG_WARN, "Ext2: Invalid superblock magic 0x%04x on %s\n", fs->sb.s_magic, bdev->name);
        kfree(fs);
        return NULL;
    }

    fs->block_size = 1024 << fs->sb.s_log_block_size;
    fs->sectors_per_block = fs->block_size / bdev->block_size;
    fs->inode_size = (fs->sb.s_rev_level >= 1) ? fs->sb.s_inode_size : 128;

    fs->num_groups = (fs->sb.s_blocks_count + fs->sb.s_blocks_per_group - 1) / fs->sb.s_blocks_per_group;
    size_t bgd_size = fs->num_groups * sizeof(struct ext2_group_desc);

    fs->bgd = (struct ext2_group_desc *)kmalloc(bgd_size);
    if (!fs->bgd) {
        kfree(fs);
        return NULL;
    }

    uint32_t bgd_block = (fs->block_size == 1024) ? 2 : 1;
    uint32_t bgd_blocks = (uint32_t)((bgd_size + fs->block_size - 1) / fs->block_size);
    uint8_t *bgd_buf = (uint8_t *)kmalloc(fs->block_size);
    if (bgd_buf) {
        for (uint32_t b = 0; b < bgd_blocks; b++) {
            ext2_read_block(fs, bgd_block + b, bgd_buf);
            size_t to_copy = bgd_size - b * fs->block_size;
            if (to_copy > fs->block_size) to_copy = fs->block_size;
            memcpy((uint8_t *)fs->bgd + b * fs->block_size, bgd_buf, to_copy);
        }
        kfree(bgd_buf);
    }

    /* Setup Ext2 VFS operations */
    ext2_ops.read = ext2_vfs_read;
    ext2_ops.write = ext2_vfs_write;
    ext2_ops.readdir = ext2_vfs_readdir;
    ext2_ops.finddir = ext2_vfs_finddir;
    ext2_ops.create = ext2_vfs_create;
    ext2_ops.mkdir = ext2_vfs_mkdir;

    /* Create root VFS node */
    struct vfs_node *root = (struct vfs_node *)kzalloc(sizeof(struct vfs_node));
    if (!root) {
        kfree(fs->bgd);
        kfree(fs);
        return NULL;
    }

    strcpy(root->name, "/");
    root->inode = EXT2_ROOT_INO;
    root->flags = VFS_DIRECTORY;
    root->mask = 0755;
    root->ops = &ext2_ops;
    root->device = fs;

    struct ext2_inode root_in;
    ext2_read_inode(fs, EXT2_ROOT_INO, &root_in);
    root->length = root_in.i_size;

    klog(KLOG_INFO, "Ext2 mounted on %s: Block size %u, %u blocks, %u inodes, %u groups\n",
         bdev->name, fs->block_size, fs->sb.s_blocks_count, fs->sb.s_inodes_count, fs->num_groups);

    return root;
}

static inline bool ext2_is_sparse_group(uint32_t g) {
    if (g <= 1) return true;
    for (uint32_t b = 3; b <= g; b *= 3) {
        if (b == g) return true;
        if (b > UINT32_MAX / 3) break;
    }
    for (uint32_t b = 5; b <= g; b *= 5) {
        if (b == g) return true;
        if (b > UINT32_MAX / 5) break;
    }
    for (uint32_t b = 7; b <= g; b *= 7) {
        if (b == g) return true;
        if (b > UINT32_MAX / 7) break;
    }
    return false;
}

int ext2_format(struct block_device *bdev) {
    if (!bdev) return -1;

    uint32_t block_size = 1024;
    uint64_t dev_bytes = bdev->total_blocks * (uint64_t)(bdev->block_size ? bdev->block_size : 512);
    uint64_t disk_blocks = dev_bytes / block_size;
    /* Cap filesystem to 8 GB (8,388,608 1KB blocks = 1,024 groups) for safe memory footprint */
    if (disk_blocks > 8388608ULL) {
        disk_blocks = 8388608ULL;
    }
    uint32_t total_blocks = (uint32_t)disk_blocks;
    uint32_t blocks_per_group = 8192;
    if (blocks_per_group > total_blocks) blocks_per_group = total_blocks;
    uint32_t inodes_per_group = 1024;
    uint32_t num_groups = (total_blocks + blocks_per_group - 1) / blocks_per_group;
    if (num_groups == 0) num_groups = 1;

    size_t bgd_size = num_groups * sizeof(struct ext2_group_desc);
    uint32_t bgd_blocks = (uint32_t)((bgd_size + block_size - 1) / block_size);
    struct ext2_group_desc *bgd_table = (struct ext2_group_desc *)kzalloc(bgd_blocks * block_size);
    if (!bgd_table) return -1;

    uint8_t *zero_block = (uint8_t *)kzalloc(block_size);
    if (!zero_block) {
        kfree(bgd_table);
        return -1;
    }

    /* Inode table size in blocks */
    uint32_t inode_table_blocks = (inodes_per_group * 128 + block_size - 1) / block_size;

    uint32_t total_free_blocks = 0;
    uint32_t root_dir_block = 0;

    for (uint32_t g = 0; g < num_groups; g++) {
        uint32_t g_blocks = (g == num_groups - 1) ? (total_blocks - g * blocks_per_group - 1) : blocks_per_group;
        uint32_t g_first = g * blocks_per_group + 1;
        bool has_backup = ext2_is_sparse_group(g);
        uint32_t meta_start = has_backup ? (g_first + 1 + bgd_blocks) : g_first;

        bgd_table[g].bg_block_bitmap = meta_start;
        bgd_table[g].bg_inode_bitmap = meta_start + 1;
        bgd_table[g].bg_inode_table  = meta_start + 2;

        uint32_t meta_used = (bgd_table[g].bg_inode_table + inode_table_blocks) - g_first;
        if (g == 0) {
            root_dir_block = bgd_table[0].bg_inode_table + inode_table_blocks;
            meta_used++; /* +1 for the pre-allocated root directory block */
        }

        bgd_table[g].bg_free_blocks_count = (uint16_t)((g_blocks > meta_used) ? (g_blocks - meta_used) : 0);
        bgd_table[g].bg_free_inodes_count = (uint16_t)((g == 0) ? (inodes_per_group - 10) : inodes_per_group);
        bgd_table[g].bg_used_dirs_count = (g == 0) ? 1 : 0;
        total_free_blocks += bgd_table[g].bg_free_blocks_count;
    }

    /* Write Superblock */
    struct ext2_superblock sb;
    memset(&sb, 0, sizeof(sb));
    sb.s_inodes_count = num_groups * inodes_per_group;
    sb.s_blocks_count = total_blocks;
    sb.s_r_blocks_count = 0;
    sb.s_free_blocks_count = total_free_blocks;
    sb.s_free_inodes_count = sb.s_inodes_count - 10; /* inodes 1..10 reserved */
    sb.s_first_data_block = 1;
    sb.s_log_block_size = 0; /* 1024 bytes */
    sb.s_log_frag_size = 0;
    sb.s_blocks_per_group = blocks_per_group;
    sb.s_frags_per_group = blocks_per_group;
    sb.s_inodes_per_group = inodes_per_group;
    sb.s_max_mnt_count = 65535;
    sb.s_magic = EXT2_SUPER_MAGIC;
    sb.s_state = 1; /* Clean */
    sb.s_errors = 1;
    sb.s_rev_level = 1;
    sb.s_first_ino = 11;
    sb.s_inode_size = 128;
    sb.s_feature_incompat = EXT2_FEATURE_INCOMPAT_FILETYPE;
    sb.s_feature_ro_compat = EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER;
    strcpy(sb.s_volume_name, "DUNIX_EXT2");

    memcpy(zero_block, &sb, sizeof(sb));
    block_device_write(bdev, 2, 2, zero_block);

    /* Write Group Descriptors Table starting at block 2 (LBA 4) */
    for (uint32_t b = 0; b < bgd_blocks; b++) {
        block_device_write(bdev, (2 + b) * 2, 2, (const uint8_t *)bgd_table + b * block_size);
    }

    /* Write Group 0 Block Bitmap: mark metadata blocks and root dir block used */
    {
        uint32_t g0_blocks = (num_groups == 1) ? (total_blocks - 1) : blocks_per_group;
        uint32_t meta_count = (root_dir_block + 1) - 1;
        memset(zero_block, 0, block_size);
        for (uint32_t i = 0; i < meta_count; i++) {
            zero_block[i / 8] |= (uint8_t)(1 << (i % 8));
        }
        /* Mark bits beyond end of group as used (padding to block_size*8) */
        for (uint32_t i = g0_blocks; i < (uint32_t)(block_size * 8); i++) {
            zero_block[i / 8] |= (uint8_t)(1 << (i % 8));
        }
        block_device_write(bdev, bgd_table[0].bg_block_bitmap * 2, 2, zero_block);
    }

    /* Write Group 0 Inode Bitmap: mark reserved inodes 1..10 used (inode 11 = lost+found, not created) */
    memset(zero_block, 0, block_size);
    zero_block[0] = 0xFF;
    zero_block[1] = 0x03;
    {
        uint32_t used_bytes = inodes_per_group / 8; /* 128 bytes for 1024 inodes */
        if (inodes_per_group % 8) {
            zero_block[used_bytes] |= (uint8_t)(0xFF << (inodes_per_group % 8));
            used_bytes++;
        }
        memset(zero_block + used_bytes, 0xFF, block_size - used_bytes);
    }
    block_device_write(bdev, bgd_table[0].bg_inode_bitmap * 2, 2, zero_block);

    /* Write Group 0 Root Inode (Inode 2) in Inode Table */
    memset(zero_block, 0, block_size);
    struct ext2_inode root_in;
    memset(&root_in, 0, sizeof(root_in));
    root_in.i_mode = EXT2_S_IFDIR | 0755;
    root_in.i_links_count = 2;
    root_in.i_size = block_size;
    root_in.i_blocks = 2; /* 2 sectors = 1024 bytes */
    root_in.i_block[0] = root_dir_block;
    memcpy(zero_block + 128, &root_in, sizeof(root_in));
    block_device_write(bdev, bgd_table[0].bg_inode_table * 2, 2, zero_block);

    /* Initialize Root Directory entries '.' and '..' in root_dir_block */
    memset(zero_block, 0, block_size);
    struct ext2_dir_entry_2 *dot = (struct ext2_dir_entry_2 *)zero_block;
    dot->inode = EXT2_ROOT_INO;
    dot->rec_len = 12;
    dot->name_len = 1;
    dot->file_type = EXT2_FT_DIR;
    dot->name[0] = '.';

    struct ext2_dir_entry_2 *dotdot = (struct ext2_dir_entry_2 *)(zero_block + 12);
    dotdot->inode = EXT2_ROOT_INO;
    dotdot->rec_len = (uint16_t)(block_size - 12);
    dotdot->name_len = 2;
    dotdot->file_type = EXT2_FT_DIR;
    dotdot->name[0] = '.';
    dotdot->name[1] = '.';
    block_device_write(bdev, root_dir_block * 2, 2, zero_block);

    /* Prepare reusable Inode Bitmap for secondary groups (all valid inodes free, padding = 0xFF) */
    uint8_t *ib_sec = (uint8_t *)kzalloc(block_size);
    if (ib_sec) {
        uint32_t used_bytes = inodes_per_group / 8;
        if (inodes_per_group % 8) {
            ib_sec[used_bytes] |= (uint8_t)(0xFF << (inodes_per_group % 8));
            used_bytes++;
        }
        memset(ib_sec + used_bytes, 0xFF, block_size - used_bytes);
    }

    /* Format secondary groups g > 0 */
    for (uint32_t g = 1; g < num_groups; g++) {
        uint32_t g_blocks = (g == num_groups - 1) ? (total_blocks - g * blocks_per_group - 1) : blocks_per_group;
        uint32_t g_first = g * blocks_per_group + 1;
        bool has_backup = ext2_is_sparse_group(g);
        uint32_t meta_used = (bgd_table[g].bg_inode_table + inode_table_blocks) - g_first;

        if (has_backup) {
            /* Write superblock backup with block group number */
            sb.s_block_group_nr = (uint16_t)g;
            memset(zero_block, 0, block_size);
            memcpy(zero_block, &sb, sizeof(sb));
            block_device_write(bdev, g_first * 2, 2, zero_block);

            /* Write BGD backup */
            for (uint32_t b = 0; b < bgd_blocks; b++) {
                block_device_write(bdev, (g_first + 1 + b) * 2, 2, (const uint8_t *)bgd_table + b * block_size);
            }
        }

        /* Block bitmap: mark metadata blocks used, mark trailing unused padding */
        memset(zero_block, 0, block_size);
        for (uint32_t i = 0; i < meta_used; i++) {
            zero_block[i / 8] |= (uint8_t)(1 << (i % 8));
        }
        for (uint32_t i = g_blocks; i < (uint32_t)(block_size * 8); i++) {
            zero_block[i / 8] |= (uint8_t)(1 << (i % 8));
        }
        block_device_write(bdev, bgd_table[g].bg_block_bitmap * 2, 2, zero_block);

        /* Inode bitmap */
        if (ib_sec) {
            block_device_write(bdev, bgd_table[g].bg_inode_bitmap * 2, 2, ib_sec);
        }
    }

    if (ib_sec) kfree(ib_sec);
    kfree(zero_block);
    kfree(bgd_table);

    if (bdev->flush) {
        bdev->flush(bdev);
    }

    klog(KLOG_INFO, "Ext2 filesystem formatted on %s (%u blocks, %u groups, %u inodes)\n",
         bdev->name, total_blocks, num_groups, sb.s_inodes_count);
    return 0;
}

void ext2_sync(struct ext2_fs *fs) {
    if (!fs || !fs->bdev) return;
    uint8_t *zero_block = (uint8_t *)kzalloc(fs->block_size);
    if (!zero_block) return;

    memcpy(zero_block, &fs->sb, sizeof(struct ext2_superblock));
    block_device_write(fs->bdev, 2, 2, zero_block);

    size_t bgd_size = fs->num_groups * sizeof(struct ext2_group_desc);
    uint32_t bgd_blocks = (uint32_t)((bgd_size + fs->block_size - 1) / fs->block_size);
    for (uint32_t b = 0; b < bgd_blocks; b++) {
        block_device_write(fs->bdev, (2 + b) * 2, 2, (const uint8_t *)fs->bgd + b * fs->block_size);
    }
    kfree(zero_block);

    if (fs->bdev->flush) {
        fs->bdev->flush(fs->bdev);
    }
}
