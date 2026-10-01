#ifndef _XF86DRM_H
#define _XF86DRM_H

#include <drm/drm.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _drmVersion {
    int version_major;
    int version_minor;
    int version_patchlevel;
    char *name;
    char *date;
    char *desc;
} drmVersion, *drmVersionPtr;

int           drmOpen(const char *name, const char *busid);
int           drmClose(int fd);
drmVersionPtr drmGetVersion(int fd);
void          drmFreeVersion(drmVersionPtr v);
int           drmSetMaster(int fd);
int           drmDropMaster(int fd);
int           drmIoctl(int fd, unsigned long request, void *arg);

/* Dumb Buffer Helpers */
int   drm_create_dumb_buffer(int fd, uint32_t width, uint32_t height, uint32_t bpp,
                             uint32_t *handle, uint32_t *pitch, uint64_t *size);
void *drm_map_dumb_buffer(int fd, uint32_t handle, uint64_t size);
int   drm_destroy_dumb_buffer(int fd, uint32_t handle);

#ifdef __cplusplus
}
#endif

#endif /* _XF86DRM_H */
