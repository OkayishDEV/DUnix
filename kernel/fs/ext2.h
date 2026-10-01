#ifndef _FS_EXT2_H
#define _FS_EXT2_H

#include <dunix/types.h>
#include <dunix/stdbool.h>
#include <fs/vfs.h>
#include <fs/block.h>

#define EXT2_SUPER_MAGIC        0xEF53

/* Inode Modes */
#define EXT2_S_IFMT             0xF000
#define EXT2_S_IFSOCK           0xC000
#define EXT2_S_IFLNK            0xA000
#define EXT2_S_IFREG            0x8000
#define EXT2_S_IFBLK            0x6000
#define EXT2_S_IFDIR            0x4000
#define EXT2_S_IFCHR            0x2000
#define EXT2_S_IFIFO            0x1000

#define EXT2_S_ISUID            0x0800
#define EXT2_S_ISGID            0x0400
#define EXT2_S_ISVTX            0x0200
#define EXT2_S_IRUSR            0x0100
#define EXT2_S_IWUSR            0x0080
#define EXT2_S_IXUSR            0x0040
#define EXT2_S_IRGRP            0x0020
#define EXT2_S_IWGRP            0x0010
#define EXT2_S_IXGRP            0x0008
#define EXT2_S_IROTH            0x0004
#define EXT2_S_IWOTH            0x0002
#define EXT2_S_IXOTH            0x0001

/* Special Inode Numbers */
#define EXT2_BAD_INO            1
#define EXT2_ROOT_INO           2
#define EXT2_BOOT_LOADER_INO    5
#define EXT2_UNDEL_DIR_INO      6

/* Feature Flags */
#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER 0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE   0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR    0x0004

#define EXT2_FEATURE_INCOMPAT_COMPRESSION   0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE      0x0002
#define EXT2_FEATURE_INCOMPAT_RECOVER       0x0004
#define EXT2_FEATURE_INCOMPAT_JOURNAL_DEV   0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG       0x0010

/* Directory Entry File Types */
#define EXT2_FT_UNKNOWN         0
#define EXT2_FT_REG_FILE        1
#define EXT2_FT_DIR             2
#define EXT2_FT_CHRDEV          3
#define EXT2_FT_BLKDEV          4
#define EXT2_FT_FIFO            5
#define EXT2_FT_SOCK            6
#define EXT2_FT_SYMLINK         7

struct ext2_superblock {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    /* EXT2_DYNAMIC_REV Specific */
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algo_bitmap;
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_padding1;
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_reserved_char_pad;
    uint16_t s_reserved_word_pad;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint32_t s_reserved[190];
} __attribute__((packed));

struct ext2_group_desc {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
} __attribute__((packed));

struct ext2_inode {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks; /* Number of 512-byte sectors */
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15]; /* 0..11: direct, 12: indirect, 13: d-indirect, 14: t-indirect */
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint32_t i_osd2[3];
} __attribute__((packed));

struct ext2_dir_entry_2 {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[];
} __attribute__((packed));

struct ext2_fs {
    struct block_device     *bdev;
    struct ext2_superblock   sb;
    struct ext2_group_desc  *bgd;
    uint32_t                 block_size;
    uint32_t                 sectors_per_block;
    uint32_t                 num_groups;
    uint32_t                 inode_size;
};

struct vfs_node *ext2_mount(struct block_device *bdev);
int              ext2_format(struct block_device *bdev);
void             ext2_sync(struct ext2_fs *fs);

#endif /* _FS_EXT2_H */
