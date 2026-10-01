#ifndef _FS_RAMFS_H
#define _FS_RAMFS_H

#include <fs/vfs.h>

struct vfs_node *ramfs_create_root(void);
struct vfs_node *ramfs_create_file(struct vfs_node *parent, const char *name, const void *data, size_t size, mode_t mode);
struct vfs_node *ramfs_create_dir(struct vfs_node *parent, const char *name, mode_t mode);
int              ramfs_rename(struct vfs_node *old_parent, const char *old_name, struct vfs_node *new_parent, const char *new_name);

#endif /* _FS_RAMFS_H */
