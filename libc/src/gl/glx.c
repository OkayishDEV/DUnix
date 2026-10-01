#include <GL/glx.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern void gl_set_target(uint32_t *color_buf, float *depth_buf, int w, int h);

static struct dunix_gl_context *g_current_ctx = NULL;

GLXContext glXCreateContext(void *dpy, void *vis, GLXContext shareList, int direct) {
    (void)dpy; (void)vis; (void)shareList; (void)direct;
    struct dunix_gl_context *ctx = (struct dunix_gl_context *)malloc(sizeof(struct dunix_gl_context));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(struct dunix_gl_context));
    return (GLXContext)ctx;
}

void glXDestroyContext(void *dpy, GLXContext ctx) {
    (void)dpy;
    if (!ctx) return;
    struct dunix_gl_context *c = (struct dunix_gl_context *)ctx;
    if (c->depth_buffer) free(c->depth_buffer);
    if (c->hardware_dri && c->drm_fd >= 0) {
        if (c->bo_handle) drm_destroy_dumb_buffer(c->drm_fd, c->bo_handle);
        close(c->drm_fd);
    } else if (c->backend == GL_BACKEND_DUI && c->target_buffer) {
        free(c->target_buffer);
    }
    if (c == g_current_ctx) g_current_ctx = NULL;
    free(c);
}

GLXContext glCreateDrmContext(int drm_fd, uint32_t width, uint32_t height) {
    if (drm_fd < 0) return NULL;

    struct dunix_gl_context *ctx = (struct dunix_gl_context *)malloc(sizeof(struct dunix_gl_context));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(struct dunix_gl_context));

    ctx->backend = GL_BACKEND_DRM;
    ctx->drm_fd = drm_fd;
    ctx->crtc_id = 1;
    ctx->width = (int)width;
    ctx->height = (int)height;

    /* Allocate Front Dumb Buffer */
    uint32_t bo_front = 0, pitch_front = 0;
    uint64_t size_front = 0;
    if (drm_create_dumb_buffer(drm_fd, width, height, 32, &bo_front, &pitch_front, &size_front) < 0) {
        free(ctx);
        return NULL;
    }
    ctx->front_vram = drm_map_dumb_buffer(drm_fd, bo_front, size_front);
    drmModeAddFB(drm_fd, width, height, 24, 32, pitch_front, bo_front, &ctx->front_fb);

    /* Allocate Back Dumb Buffer */
    uint32_t bo_back = 0, pitch_back = 0;
    uint64_t size_back = 0;
    if (drm_create_dumb_buffer(drm_fd, width, height, 32, &bo_back, &pitch_back, &size_back) < 0) {
        free(ctx);
        return NULL;
    }
    ctx->back_vram = drm_map_dumb_buffer(drm_fd, bo_back, size_back);
    drmModeAddFB(drm_fd, width, height, 24, 32, pitch_back, bo_back, &ctx->back_fb);

    drmModeModeInfo mode;
    memset(&mode, 0, sizeof(mode));
    mode.hdisplay = (uint16_t)width;
    mode.vdisplay = (uint16_t)height;
    mode.vrefresh = 60;
    drmModeSetCrtc(drm_fd, 1, ctx->front_fb, 0, 0, NULL, 0, &mode);

    ctx->depth_buffer = (float *)malloc(width * height * sizeof(float));
    ctx->is_front_active = true;

    /* Bind back buffer as current render target */
    gl_set_target((uint32_t *)ctx->back_vram, ctx->depth_buffer, ctx->width, ctx->height);
    g_current_ctx = ctx;

    return (GLXContext)ctx;
}

GLXContext glCreateDuiContext(DuiConnection *conn, DuiWindow win, int width, int height) {
    if (!conn) return NULL;

    struct dunix_gl_context *ctx = (struct dunix_gl_context *)malloc(sizeof(struct dunix_gl_context));
    if (!ctx) return NULL;
    memset(ctx, 0, sizeof(struct dunix_gl_context));

    ctx->backend = GL_BACKEND_DUI;
    ctx->dui_conn = conn;
    ctx->dui_win = win;
    ctx->width = width;
    ctx->height = height;

    /* DRI3 / Hardware DRM Acceleration: Try allocating a GPU dumb buffer in VRAM */
    int drm_fd = drmOpen("/dev/dri/renderD128", NULL);
    if (drm_fd < 0) drm_fd = drmOpen("/dev/dri/card0", NULL);

    if (drm_fd >= 0) {
        uint32_t bo = 0, pitch = 0; uint64_t size = 0;
        if (drm_create_dumb_buffer(drm_fd, (uint32_t)width, (uint32_t)height, 32, &bo, &pitch, &size) == 0) {
            void *vram = drm_map_dumb_buffer(drm_fd, bo, size);
            if (vram) {
                struct drm_mode_map_dumb m;
                memset(&m, 0, sizeof(m));
                m.handle = bo;
                if (ioctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &m) == 0) {
                    ctx->drm_fd = drm_fd;
                    ctx->bo_handle = bo;
                    ctx->vram_offset = m.offset;
                    ctx->target_buffer = (uint32_t *)vram;
                    ctx->hardware_dri = true;
                }
            }
        }
        if (!ctx->hardware_dri) close(drm_fd);
    }

    if (!ctx->target_buffer) {
        ctx->target_buffer = (uint32_t *)malloc(width * height * sizeof(uint32_t));
    }
    ctx->depth_buffer  = (float *)malloc(width * height * sizeof(float));

    if (!ctx->target_buffer || !ctx->depth_buffer) {
        if (!ctx->hardware_dri && ctx->target_buffer) free(ctx->target_buffer);
        if (ctx->depth_buffer) free(ctx->depth_buffer);
        free(ctx);
        return NULL;
    }

    gl_set_target(ctx->target_buffer, ctx->depth_buffer, width, height);
    g_current_ctx = ctx;

    return (GLXContext)ctx;
}

int glXMakeCurrent(void *dpy, GLXDrawable drawable, GLXContext ctx) {
    (void)dpy; (void)drawable;
    if (!ctx) {
        g_current_ctx = NULL;
        return 0;
    }
    struct dunix_gl_context *c = (struct dunix_gl_context *)ctx;
    g_current_ctx = c;

    if (c->backend == GL_BACKEND_DRM) {
        uint32_t *target = c->is_front_active ? (uint32_t *)c->back_vram : (uint32_t *)c->front_vram;
        gl_set_target(target, c->depth_buffer, c->width, c->height);
    } else {
        gl_set_target(c->target_buffer, c->depth_buffer, c->width, c->height);
    }
    return 1;
}

void glXSwapBuffers(void *dpy, GLXDrawable drawable) {
    (void)dpy; (void)drawable;
    if (!g_current_ctx) return;
    struct dunix_gl_context *c = g_current_ctx;

    if (c->backend == GL_BACKEND_DRM) {
        /* Hardware Page Flip! */
        uint32_t next_fb = c->is_front_active ? c->back_fb : c->front_fb;
        drmModePageFlip(c->drm_fd, c->crtc_id, next_fb, DRM_MODE_PAGE_FLIP_EVENT, NULL);

        c->is_front_active = !c->is_front_active;
        uint32_t *new_target = c->is_front_active ? (uint32_t *)c->back_vram : (uint32_t *)c->front_vram;
        gl_set_target(new_target, c->depth_buffer, c->width, c->height);
    } else if (c->backend == GL_BACKEND_DUI && c->dui_conn) {
        if (c->hardware_dri && c->vram_offset > 0) {
            /* Zero-Copy Hardware DRI Presentation */
            struct dws_message msg;
            memset(&msg, 0, sizeof(msg));
            msg.type = DWS_REQ_BLIT_BUFFER;
            msg.window_id = c->dui_win;
            msg.rect.x = 0;
            msg.rect.y = 0;
            msg.rect.width = (uint32_t)c->width;
            msg.rect.height = (uint32_t)c->height;
            msg.rect.color = (uint32_t)c->vram_offset;
            send(c->dui_conn->fd, &msg, sizeof(msg), 0);
        } else {
            dui_blit_buffer(c->dui_conn, c->dui_win, c->target_buffer, c->width, c->height);
        }
    }
}
