#include <xf86drm.h>
#include <xf86drmMode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

int drmIoctl(int fd, unsigned long request, void *arg) {
    return ioctl(fd, request, arg);
}

int drmOpen(const char *name, const char *busid) {
    (void)name;
    (void)busid;
    int fd = open("/dev/dri/card0", O_RDWR);
    if (fd < 0) {
        fd = open("/dev/dri/renderD128", O_RDWR);
    }
    return fd;
}

int drmClose(int fd) {
    return close(fd);
}

drmVersionPtr drmGetVersion(int fd) {
    drmVersionPtr v = (drmVersionPtr)malloc(sizeof(drmVersion));
    if (!v) return NULL;
    memset(v, 0, sizeof(drmVersion));

    char name_buf[64] = {0};
    char date_buf[64] = {0};
    char desc_buf[128] = {0};

    struct drm_version req;
    memset(&req, 0, sizeof(req));
    req.name = name_buf;
    req.name_len = sizeof(name_buf);
    req.date = date_buf;
    req.date_len = sizeof(date_buf);
    req.desc = desc_buf;
    req.desc_len = sizeof(desc_buf);

    if (drmIoctl(fd, DRM_IOCTL_VERSION, &req) < 0) {
        free(v);
        return NULL;
    }

    v->version_major = req.version_major;
    v->version_minor = req.version_minor;
    v->version_patchlevel = req.version_patchlevel;
    v->name = strdup(name_buf);
    v->date = strdup(date_buf);
    v->desc = strdup(desc_buf);
    return v;
}

void drmFreeVersion(drmVersionPtr v) {
    if (!v) return;
    if (v->name) free(v->name);
    if (v->date) free(v->date);
    if (v->desc) free(v->desc);
    free(v);
}

int drmSetMaster(int fd) {
    return drmIoctl(fd, DRM_IOCTL_SET_MASTER, NULL);
}

int drmDropMaster(int fd) {
    return drmIoctl(fd, DRM_IOCTL_DROP_MASTER, NULL);
}

drmModeResPtr drmModeGetResources(int fd) {
    struct drm_mode_card_res req;
    memset(&req, 0, sizeof(req));

    if (drmIoctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &req) < 0) {
        return NULL;
    }

    drmModeResPtr res = (drmModeResPtr)malloc(sizeof(drmModeRes));
    if (!res) return NULL;
    memset(res, 0, sizeof(drmModeRes));

    res->count_crtcs = req.count_crtcs;
    res->count_connectors = req.count_connectors;
    res->count_encoders = req.count_encoders;
    res->count_fbs = req.count_fbs;
    res->min_width = req.min_width;
    res->max_width = req.max_width;
    res->min_height = req.min_height;
    res->max_height = req.max_height;

    if (res->count_crtcs > 0) {
        res->crtcs = (uint32_t *)malloc(sizeof(uint32_t) * res->count_crtcs);
        req.crtc_id_ptr = (uint64_t)res->crtcs;
    }
    if (res->count_connectors > 0) {
        res->connectors = (uint32_t *)malloc(sizeof(uint32_t) * res->count_connectors);
        req.connector_id_ptr = (uint64_t)res->connectors;
    }
    if (res->count_encoders > 0) {
        res->encoders = (uint32_t *)malloc(sizeof(uint32_t) * res->count_encoders);
        req.encoder_id_ptr = (uint64_t)res->encoders;
    }
    if (res->count_fbs > 0) {
        res->fbs = (uint32_t *)malloc(sizeof(uint32_t) * res->count_fbs);
        req.fb_id_ptr = (uint64_t)res->fbs;
    }

    if (drmIoctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &req) < 0) {
        drmModeFreeResources(res);
        return NULL;
    }

    return res;
}

void drmModeFreeResources(drmModeResPtr ptr) {
    if (!ptr) return;
    if (ptr->crtcs) free(ptr->crtcs);
    if (ptr->connectors) free(ptr->connectors);
    if (ptr->encoders) free(ptr->encoders);
    if (ptr->fbs) free(ptr->fbs);
    free(ptr);
}

drmModeConnectorPtr drmModeGetConnector(int fd, uint32_t connectorId) {
    struct drm_mode_get_connector req;
    memset(&req, 0, sizeof(req));
    req.connector_id = connectorId;

    if (drmIoctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &req) < 0) {
        return NULL;
    }

    drmModeConnectorPtr conn = (drmModeConnectorPtr)malloc(sizeof(drmModeConnector));
    if (!conn) return NULL;
    memset(conn, 0, sizeof(drmModeConnector));

    conn->connector_id = req.connector_id;
    conn->encoder_id = req.encoder_id;
    conn->connector_type = req.connector_type;
    conn->connector_type_id = req.connector_type_id;
    conn->connection = req.connection;
    conn->mmWidth = req.mm_width;
    conn->mmHeight = req.mm_height;
    conn->subpixel = req.subpixel;
    conn->count_modes = req.count_modes;
    conn->count_encoders = req.count_encoders;

    if (conn->count_modes > 0) {
        conn->modes = (drmModeModeInfoPtr)malloc(sizeof(drmModeModeInfo) * conn->count_modes);
        req.modes_ptr = (uint64_t)conn->modes;
    }
    if (conn->count_encoders > 0) {
        conn->encoders = (uint32_t *)malloc(sizeof(uint32_t) * conn->count_encoders);
        req.encoders_ptr = (uint64_t)conn->encoders;
    }

    if (drmIoctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &req) < 0) {
        drmModeFreeConnector(conn);
        return NULL;
    }

    return conn;
}

void drmModeFreeConnector(drmModeConnectorPtr ptr) {
    if (!ptr) return;
    if (ptr->modes) free(ptr->modes);
    if (ptr->encoders) free(ptr->encoders);
    if (ptr->props) free(ptr->props);
    if (ptr->prop_values) free(ptr->prop_values);
    free(ptr);
}

drmModeCrtcPtr drmModeGetCrtc(int fd, uint32_t crtcId) {
    struct drm_mode_crtc req;
    memset(&req, 0, sizeof(req));
    req.crtc_id = crtcId;

    if (drmIoctl(fd, DRM_IOCTL_MODE_GETCRTC, &req) < 0) {
        return NULL;
    }

    drmModeCrtcPtr crtc = (drmModeCrtcPtr)malloc(sizeof(drmModeCrtc));
    if (!crtc) return NULL;
    memset(crtc, 0, sizeof(drmModeCrtc));

    crtc->crtc_id = req.crtc_id;
    crtc->buffer_id = req.fb_id;
    crtc->x = req.x;
    crtc->y = req.y;
    crtc->width = req.mode.hdisplay;
    crtc->height = req.mode.vdisplay;
    crtc->mode_valid = req.mode_valid;
    crtc->gamma_size = req.gamma_size;
    memcpy(&crtc->mode, &req.mode, sizeof(drmModeModeInfo));
    return crtc;
}

void drmModeFreeCrtc(drmModeCrtcPtr ptr) {
    if (!ptr) return;
    free(ptr);
}

int drmModeSetCrtc(int fd, uint32_t crtcId, uint32_t bufferId,
                   uint32_t x, uint32_t y, uint32_t *connectors, int count,
                   drmModeModeInfoPtr mode) {
    struct drm_mode_crtc req;
    memset(&req, 0, sizeof(req));
    req.crtc_id = crtcId;
    req.fb_id = bufferId;
    req.x = x;
    req.y = y;
    req.set_connectors_ptr = (uint64_t)connectors;
    req.count_connectors = (uint32_t)count;

    if (mode) {
        req.mode_valid = 1;
        memcpy(&req.mode, mode, sizeof(struct drm_mode_modeinfo));
    }

    return drmIoctl(fd, DRM_IOCTL_MODE_SETCRTC, &req);
}

int drmModeAddFB(int fd, uint32_t width, uint32_t height, uint8_t depth,
                 uint8_t bpp, uint32_t pitch, uint32_t bo_handle,
                 uint32_t *buf_id) {
    struct drm_mode_fb_cmd req;
    memset(&req, 0, sizeof(req));
    req.width = width;
    req.height = height;
    req.pitch = pitch;
    req.bpp = bpp;
    req.depth = depth;
    req.handle = bo_handle;

    int ret = drmIoctl(fd, DRM_IOCTL_MODE_ADDFB, &req);
    if (ret == 0 && buf_id) {
        *buf_id = req.fb_id;
    }
    return ret;
}

int drmModeRmFB(int fd, uint32_t buffer_id) {
    return drmIoctl(fd, DRM_IOCTL_MODE_RMFB, &buffer_id);
}

int drmModePageFlip(int fd, uint32_t crtc_id, uint32_t fb_id,
                    uint32_t flags, void *user_data) {
    struct drm_mode_crtc_page_flip req;
    memset(&req, 0, sizeof(req));
    req.crtc_id = crtc_id;
    req.fb_id = fb_id;
    req.flags = flags;
    req.user_data = (uint64_t)user_data;
    return drmIoctl(fd, DRM_IOCTL_MODE_PAGE_FLIP, &req);
}

int drm_create_dumb_buffer(int fd, uint32_t width, uint32_t height, uint32_t bpp,
                           uint32_t *handle, uint32_t *pitch, uint64_t *size) {
    struct drm_mode_create_dumb req;
    memset(&req, 0, sizeof(req));
    req.width = width;
    req.height = height;
    req.bpp = bpp;

    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &req) < 0) {
        return -1;
    }

    if (handle) *handle = req.handle;
    if (pitch)  *pitch = req.pitch;
    if (size)   *size = req.size;
    return 0;
}

void *drm_map_dumb_buffer(int fd, uint32_t handle, uint64_t size) {
    struct drm_mode_map_dumb req;
    memset(&req, 0, sizeof(req));
    req.handle = handle;

    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &req) < 0) {
        return NULL;
    }

    void *ptr = mmap(NULL, (size_t)size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)req.offset);
    if (ptr == MAP_FAILED) return NULL;
    return ptr;
}

int drm_destroy_dumb_buffer(int fd, uint32_t handle) {
    struct drm_mode_destroy_dumb req;
    memset(&req, 0, sizeof(req));
    req.handle = handle;
    return drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &req);
}
