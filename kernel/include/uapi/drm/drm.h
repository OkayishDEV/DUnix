#ifndef _UAPI_DRM_H
#define _UAPI_DRM_H

#include <dunix/types.h>
#include <uapi/drm/drm_mode.h>

#define DRM_NAME           "dunix-drm"
#define DRM_DATE           "20260925"
#define DRM_MAJOR          1
#define DRM_MINOR          4
#define DRM_PATCHLEVEL     0

/* DRM Core IOCTLs */
#define DRM_IOCTL_BASE              'd'
#define DRM_COMMAND_BASE            0x40

#define DRM_IOCTL_VERSION           0xC0406400  /* _IOWR('d', 0x00, struct drm_version) */
#define DRM_IOCTL_GET_UNIQUE        0xC0106401
#define DRM_IOCTL_SET_MASTER        0x0000641E  /* _IO('d', 0x1e) */
#define DRM_IOCTL_DROP_MASTER       0x0000641F  /* _IO('d', 0x1f) */

/* DRM KMS Mode Setting IOCTLs */
#define DRM_IOCTL_MODE_GETRESOURCES 0xC04064A0  /* _IOWR('d', 0xa0, struct drm_mode_card_res) */
#define DRM_IOCTL_MODE_GETCRTC      0xC06864A1  /* _IOWR('d', 0xa1, struct drm_mode_crtc) */
#define DRM_IOCTL_MODE_SETCRTC      0xC06864A2  /* _IOWR('d', 0xa2, struct drm_mode_crtc) */
#define DRM_IOCTL_MODE_GETCONNECTOR 0xC05064A7  /* _IOWR('d', 0xa7, struct drm_mode_get_connector) */
#define DRM_IOCTL_MODE_ADDFB        0xC01C64AE  /* _IOWR('d', 0xae, struct drm_mode_fb_cmd) */
#define DRM_IOCTL_MODE_RMFB         0xC00464AF  /* _IOWR('d', 0xaf, uint32_t) */
#define DRM_IOCTL_MODE_PAGE_FLIP    0xC01864B0  /* _IOWR('d', 0xb0, struct drm_mode_crtc_page_flip) */
#define DRM_IOCTL_MODE_CREATE_DUMB  0xC02064B2  /* _IOWR('d', 0xb2, struct drm_mode_create_dumb) */
#define DRM_IOCTL_MODE_MAP_DUMB     0xC01064B3  /* _IOWR('d', 0xb3, struct drm_mode_map_dumb) */
#define DRM_IOCTL_MODE_DESTROY_DUMB 0xC00464B4  /* _IOWR('d', 0xb4, struct drm_mode_destroy_dumb) */

struct drm_version {
    int version_major;
    int version_minor;
    int version_patchlevel;
    size_t name_len;
    char *name;
    size_t date_len;
    char *date;
    size_t desc_len;
    char *desc;
};

struct drm_unique {
    size_t unique_len;
    char *unique;
};

#endif /* _UAPI_DRM_H */
