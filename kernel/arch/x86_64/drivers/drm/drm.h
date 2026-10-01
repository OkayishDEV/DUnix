#ifndef _DRIVERS_DRM_H
#define _DRIVERS_DRM_H

#include <dunix/types.h>
#include <fs/vfs.h>
#include <uapi/drm/drm.h>

void drm_init(struct vfs_node *dev_dir);
int  drm_device_ioctl(struct vfs_node *node, unsigned long request, void *arg);

#endif /* _DRIVERS_DRM_H */
