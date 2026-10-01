#include "drm.h"
#include <arch/x86_64/drivers/bga.h>
#include <fs/ramfs.h>
#include <mm/heap.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>
#include <process/process.h>

#define MAX_DUMB_BUFFERS 16
#define MAX_FBS          16

struct dumb_bo {
    uint32_t handle;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t pitch;
    uint64_t size;
    uint64_t vram_offset;
    bool active;
};

struct fb_obj {
    uint32_t fb_id;
    uint32_t bo_handle;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    bool active;
};

static struct dumb_bo dumb_buffers[MAX_DUMB_BUFFERS];
static struct fb_obj  framebuffers[MAX_FBS];
static uint32_t next_bo_handle = 1;
static uint32_t next_fb_id = 101;
static pid_t drm_master_pid = 0;

static struct vfs_ops drm_ops;

static struct dumb_bo *find_dumb_bo(uint32_t handle) {
    if (handle == 0) return NULL;
    for (int i = 0; i < MAX_DUMB_BUFFERS; i++) {
        if (dumb_buffers[i].active && dumb_buffers[i].handle == handle) {
            return &dumb_buffers[i];
        }
    }
    return NULL;
}

static struct fb_obj *find_fb_obj(uint32_t fb_id) {
    if (fb_id == 0) return NULL;
    for (int i = 0; i < MAX_FBS; i++) {
        if (framebuffers[i].active && framebuffers[i].fb_id == fb_id) {
            return &framebuffers[i];
        }
    }
    return NULL;
}

int drm_device_ioctl(struct vfs_node *node, unsigned long request, void *arg) {
    (void)node;
    if (!bga_is_available()) return -19; /* -ENODEV */

    uint8_t group = (request >> 8) & 0xFF;
    uint8_t cmd   = request & 0xFF;

    if (group != DRM_IOCTL_BASE) {
        return -22; /* -EINVAL */
    }

    switch (cmd) {
    case 0x00: { /* DRM_IOCTL_VERSION */
        if (!arg) return -14; /* -EFAULT */
        struct drm_version *ver = (struct drm_version *)arg;
        ver->version_major = DRM_MAJOR;
        ver->version_minor = DRM_MINOR;
        ver->version_patchlevel = DRM_PATCHLEVEL;

        const char *name_str = DRM_NAME;
        const char *date_str = DRM_DATE;
        const char *desc_str = "DUnix Direct Rendering Manager & KMS Driver";

        if (ver->name && ver->name_len > 0) {
            size_t len = strlen(name_str);
            if (len >= ver->name_len) len = ver->name_len - 1;
            memcpy(ver->name, name_str, len);
            ver->name[len] = '\0';
        }
        ver->name_len = strlen(name_str);

        if (ver->date && ver->date_len > 0) {
            size_t len = strlen(date_str);
            if (len >= ver->date_len) len = ver->date_len - 1;
            memcpy(ver->date, date_str, len);
            ver->date[len] = '\0';
        }
        ver->date_len = strlen(date_str);

        if (ver->desc && ver->desc_len > 0) {
            size_t len = strlen(desc_str);
            if (len >= ver->desc_len) len = ver->desc_len - 1;
            memcpy(ver->desc, desc_str, len);
            ver->desc[len] = '\0';
        }
        ver->desc_len = strlen(desc_str);

        return 0;
    }

    case 0x01: { /* DRM_IOCTL_GET_UNIQUE */
        if (!arg) return -14;
        struct drm_unique *u = (struct drm_unique *)arg;
        const char *uniq = "pci:0000:00:02.0";
        if (u->unique && u->unique_len > 0) {
            size_t len = strlen(uniq);
            if (len >= u->unique_len) len = u->unique_len - 1;
            memcpy(u->unique, uniq, len);
            u->unique[len] = '\0';
        }
        u->unique_len = strlen(uniq);
        return 0;
    }

    case 0x1E: { /* DRM_IOCTL_SET_MASTER */
        struct process *proc = process_get_current();
        if (proc) drm_master_pid = proc->pid;
        return 0;
    }

    case 0x1F: { /* DRM_IOCTL_DROP_MASTER */
        struct process *proc = process_get_current();
        if (proc && drm_master_pid == proc->pid) {
            drm_master_pid = 0;
        }
        return 0;
    }

    case 0xA0: { /* DRM_IOCTL_MODE_GETRESOURCES */
        if (!arg) return -14;
        struct drm_mode_card_res *res = (struct drm_mode_card_res *)arg;

        uint32_t crtc_id = 1;
        uint32_t connector_id = 1;
        uint32_t encoder_id = 1;

        if (res->count_crtcs >= 1 && res->crtc_id_ptr) {
            uint32_t *user_crtcs = (uint32_t *)res->crtc_id_ptr;
            user_crtcs[0] = crtc_id;
        }
        res->count_crtcs = 1;

        if (res->count_connectors >= 1 && res->connector_id_ptr) {
            uint32_t *user_conns = (uint32_t *)res->connector_id_ptr;
            user_conns[0] = connector_id;
        }
        res->count_connectors = 1;

        if (res->count_encoders >= 1 && res->encoder_id_ptr) {
            uint32_t *user_encs = (uint32_t *)res->encoder_id_ptr;
            user_encs[0] = encoder_id;
        }
        res->count_encoders = 1;

        uint32_t active_fb_count = 0;
        for (int i = 0; i < MAX_FBS; i++) {
            if (framebuffers[i].active) active_fb_count++;
        }

        if (res->count_fbs >= active_fb_count && res->fb_id_ptr && active_fb_count > 0) {
            uint32_t *user_fbs = (uint32_t *)res->fb_id_ptr;
            uint32_t idx = 0;
            for (int i = 0; i < MAX_FBS && idx < active_fb_count; i++) {
                if (framebuffers[i].active) {
                    user_fbs[idx++] = framebuffers[i].fb_id;
                }
            }
        }
        res->count_fbs = active_fb_count;

        res->min_width = 640;
        res->max_width = 1920;
        res->min_height = 480;
        res->max_height = 1200;

        return 0;
    }

    case 0xA1: { /* DRM_IOCTL_MODE_GETCRTC */
        if (!arg) return -14;
        struct drm_mode_crtc *crtc = (struct drm_mode_crtc *)arg;
        if (crtc->crtc_id != 1) return -2; /* -ENOENT */

        crtc->x = 0;
        crtc->y = bga_get_display_y_offset();
        crtc->mode_valid = 1;

        crtc->mode.clock = 65000;
        crtc->mode.hdisplay = bga_get_width();
        crtc->mode.vdisplay = bga_get_height();
        crtc->mode.vrefresh = 60;
        strncpy(crtc->mode.name, "1024x768", DRM_DISPLAY_MODE_LEN - 1);
        return 0;
    }

    case 0xA2: { /* DRM_IOCTL_MODE_SETCRTC */
        if (!arg) return -14;
        struct drm_mode_crtc *crtc = (struct drm_mode_crtc *)arg;
        if (crtc->crtc_id != 1) return -2;

        if (crtc->mode_valid && crtc->mode.hdisplay > 0 && crtc->mode.vdisplay > 0) {
            bga_set_video_mode(crtc->mode.hdisplay, crtc->mode.vdisplay, 32);
        }
        if (crtc->fb_id > 0) {
            struct fb_obj *fb = find_fb_obj(crtc->fb_id);
            if (fb) {
                struct dumb_bo *bo = find_dumb_bo(fb->bo_handle);
                if (bo) {
                    uint32_t y_off = (uint32_t)(bo->vram_offset / bo->pitch);
                    bga_set_display_offset(0, y_off);
                }
            }
        }
        return 0;
    }

    case 0xA7: { /* DRM_IOCTL_MODE_GETCONNECTOR */
        if (!arg) return -14;
        struct drm_mode_get_connector *conn = (struct drm_mode_get_connector *)arg;
        if (conn->connector_id != 1) return -2;

        conn->connector_type = DRM_MODE_CONNECTOR_VGA;
        conn->connector_type_id = 1;
        conn->connection = DRM_MODE_CONNECTED;
        conn->mm_width = 320;
        conn->mm_height = 240;
        conn->subpixel = 1;
        conn->encoder_id = 1;

        if (conn->count_modes >= 1 && conn->modes_ptr) {
            struct drm_mode_modeinfo *user_mode = (struct drm_mode_modeinfo *)conn->modes_ptr;
            memset(user_mode, 0, sizeof(struct drm_mode_modeinfo));
            user_mode->clock = 65000;
            user_mode->hdisplay = bga_get_width();
            user_mode->vdisplay = bga_get_height();
            user_mode->vrefresh = 60;
            strncpy(user_mode->name, "1024x768@60", DRM_DISPLAY_MODE_LEN - 1);
        }
        conn->count_modes = 1;

        if (conn->count_encoders >= 1 && conn->encoders_ptr) {
            uint32_t *encs = (uint32_t *)conn->encoders_ptr;
            encs[0] = 1;
        }
        conn->count_encoders = 1;

        return 0;
    }

    case 0xAE: { /* DRM_IOCTL_MODE_ADDFB */
        if (!arg) return -14;
        struct drm_mode_fb_cmd *cmd_fb = (struct drm_mode_fb_cmd *)arg;
        struct dumb_bo *bo = find_dumb_bo(cmd_fb->handle);
        if (!bo) return -2; /* -ENOENT */

        int slot = -1;
        for (int i = 0; i < MAX_FBS; i++) {
            if (!framebuffers[i].active) { slot = i; break; }
        }
        if (slot < 0) return -12; /* -ENOMEM */

        framebuffers[slot].fb_id = next_fb_id++;
        framebuffers[slot].bo_handle = bo->handle;
        framebuffers[slot].width = cmd_fb->width;
        framebuffers[slot].height = cmd_fb->height;
        framebuffers[slot].pitch = cmd_fb->pitch;
        framebuffers[slot].bpp = cmd_fb->bpp;
        framebuffers[slot].active = true;

        cmd_fb->fb_id = framebuffers[slot].fb_id;
        return 0;
    }

    case 0xAF: { /* DRM_IOCTL_MODE_RMFB */
        if (!arg) return -14;
        uint32_t fb_id = *(uint32_t *)arg;
        struct fb_obj *fb = find_fb_obj(fb_id);
        if (!fb) return -2;
        fb->active = false;
        return 0;
    }

    case 0xB0: { /* DRM_IOCTL_MODE_PAGE_FLIP */
        if (!arg) return -14;
        struct drm_mode_crtc_page_flip *flip = (struct drm_mode_crtc_page_flip *)arg;
        struct fb_obj *fb = find_fb_obj(flip->fb_id);
        if (!fb) return -2;

        struct dumb_bo *bo = find_dumb_bo(fb->bo_handle);
        if (!bo) return -2;

        uint32_t y_offset = (uint32_t)(bo->vram_offset / bo->pitch);
        bga_set_display_offset(0, y_offset);
        return 0;
    }

    case 0xB2: { /* DRM_IOCTL_MODE_CREATE_DUMB */
        if (!arg) return -14;
        struct drm_mode_create_dumb *d = (struct drm_mode_create_dumb *)arg;
        if (d->width == 0 || d->height == 0) return -22;

        uint32_t bpp = d->bpp ? d->bpp : 32;
        uint32_t pitch = d->width * (bpp / 8);
        uint64_t size = (uint64_t)pitch * d->height;

        int slot = -1;
        for (int i = 0; i < MAX_DUMB_BUFFERS; i++) {
            if (!dumb_buffers[i].active) { slot = i; break; }
        }
        if (slot < 0) return -12;

        /* Allocate non-overlapping VRAM region aligned to 64KB */
        uint64_t vram_offset = 0;
        for (int i = 0; i < MAX_DUMB_BUFFERS; i++) {
            if (dumb_buffers[i].active) {
                uint64_t end = dumb_buffers[i].vram_offset + dumb_buffers[i].size;
                end = (end + 0xFFFF) & ~0xFFFFULL;
                if (end > vram_offset) {
                    vram_offset = end;
                }
            }
        }
        if (vram_offset + size > 16 * 1024 * 1024) {
            return -12; /* VRAM full */
        }

        dumb_buffers[slot].handle = next_bo_handle++;
        dumb_buffers[slot].width = d->width;
        dumb_buffers[slot].height = d->height;
        dumb_buffers[slot].bpp = bpp;
        dumb_buffers[slot].pitch = pitch;
        dumb_buffers[slot].size = size;
        dumb_buffers[slot].vram_offset = vram_offset;
        dumb_buffers[slot].active = true;

        d->handle = dumb_buffers[slot].handle;
        d->pitch = pitch;
        d->size = size;
        return 0;
    }

    case 0xB3: { /* DRM_IOCTL_MODE_MAP_DUMB */
        if (!arg) return -14;
        struct drm_mode_map_dumb *m = (struct drm_mode_map_dumb *)arg;
        struct dumb_bo *bo = find_dumb_bo(m->handle);
        if (!bo) return -2;

        m->offset = bo->vram_offset;
        return 0;
    }

    case 0xB4: { /* DRM_IOCTL_MODE_DESTROY_DUMB */
        if (!arg) return -14;
        struct drm_mode_destroy_dumb *dest = (struct drm_mode_destroy_dumb *)arg;
        struct dumb_bo *bo = find_dumb_bo(dest->handle);
        if (!bo) return -2;
        bo->active = false;
        return 0;
    }

    default:
        break;
    }

    return -22; /* -EINVAL */
}

void drm_init(struct vfs_node *dev_dir) {
    memset(dumb_buffers, 0, sizeof(dumb_buffers));
    memset(framebuffers, 0, sizeof(framebuffers));

    drm_ops.read = fb_device_read;
    drm_ops.write = fb_device_write;
    drm_ops.ioctl = drm_device_ioctl;

    struct vfs_node *dri_dir = ramfs_create_dir(dev_dir, "dri", 0755);
    if (!dri_dir) {
        klog(KLOG_ERROR, "DRM: Failed to create /dev/dri directory\n");
        return;
    }

    struct vfs_node *card0 = ramfs_create_file(dri_dir, "card0", NULL, 0, 0666);
    if (card0) {
        card0->flags = VFS_CHARDEVICE;
        card0->ops = &drm_ops;
    }

    struct vfs_node *render128 = ramfs_create_file(dri_dir, "renderD128", NULL, 0, 0666);
    if (render128) {
        render128->flags = VFS_CHARDEVICE;
        render128->ops = &drm_ops;
    }

    klog(KLOG_INFO, "DRM/KMS Subsystem Initialized: /dev/dri/card0, /dev/dri/renderD128 (BGA/QEMU Hardware Acceleration)\n");
}
