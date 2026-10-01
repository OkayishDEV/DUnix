#ifndef _FS_PROCFS_H
#define _FS_PROCFS_H

#include <fs/vfs.h>

void             procfs_init(void);
struct vfs_node *procfs_mount(void);

#endif /* _FS_PROCFS_H */
