#ifndef _FS_DEVFS_H
#define _FS_DEVFS_H

#include <fs/vfs.h>

void devfs_init(struct vfs_node *dev_dir);

#endif /* _FS_DEVFS_H */
