#ifndef _GLX_H
#define _GLX_H

#include <GL/gl.h>
#include <dui/dui.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *GLXContext;
typedef uint32_t GLXDrawable;

#define GLX_USE_GL           1
#define GLX_BUFFER_SIZE      2
#define GLX_LEVEL            3
#define GLX_RGBA             4
#define GLX_DOUBLEBUFFER     5
#define GLX_STEREO           6
#define GLX_AUX_BUFFERS      7
#define GLX_RED_SIZE         8
#define GLX_GREEN_SIZE       9
#define GLX_BLUE_SIZE        10
#define GLX_ALPHA_SIZE       11
#define GLX_DEPTH_SIZE       12
#define GLX_STENCIL_SIZE     13
#define GLX_ACCUM_RED_SIZE   14
#define GLX_ACCUM_GREEN_SIZE 15
#define GLX_ACCUM_BLUE_SIZE  16
#define GLX_ACCUM_ALPHA_SIZE 17

/* Internal backend types */
#define GL_BACKEND_DUI 1
#define GL_BACKEND_DRM 2

/* DUnix GL Context configuration */
struct dunix_gl_context {
    int backend;           /* GL_BACKEND_DUI or GL_BACKEND_DRM */
    int drm_fd;
    uint32_t crtc_id;
    uint32_t front_fb;
    uint32_t back_fb;
    void *front_vram;
    void *back_vram;
    bool is_front_active;

    /* DUI Window target */
    DuiConnection *dui_conn;
    DuiWindow dui_win;
    uint32_t *target_buffer;
    int width, height;
    float *depth_buffer;
    bool hardware_dri;
    uint32_t bo_handle;
    uint64_t vram_offset;
};

GLXContext glXCreateContext(void *dpy, void *vis, GLXContext shareList, int direct);
void       glXDestroyContext(void *dpy, GLXContext ctx);
int        glXMakeCurrent(void *dpy, GLXDrawable drawable, GLXContext ctx);
void       glXSwapBuffers(void *dpy, GLXDrawable drawable);

/* DUnix extensions for DRM & DUI */
GLXContext glCreateDrmContext(int drm_fd, uint32_t width, uint32_t height);
GLXContext glCreateDuiContext(DuiConnection *conn, DuiWindow win, int width, int height);

#ifdef __cplusplus
}
#endif

#endif /* _GLX_H */
