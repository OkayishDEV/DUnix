#include <GL/gl.h>
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_VERTICES 4096
#define MATRIX_STACK_DEPTH 16

struct gl_vertex {
    float x, y, z;
    float nx, ny, nz;
    float r, g, b, a;
    float u, v;
};

struct gl_screen_vertex {
    float sx, sy, sz;
    float r, g, b, a;
};

struct gl_internal_state {
    /* Matrices */
    GLenum matrix_mode;
    float modelview_stack[MATRIX_STACK_DEPTH][16];
    int modelview_top;
    float projection_stack[MATRIX_STACK_DEPTH][16];
    int projection_top;

    /* Viewport */
    int vp_x, vp_y, vp_w, vp_h;

    /* Clear values */
    float clear_r, clear_g, clear_b, clear_a;
    float clear_depth;

    /* Enables */
    bool depth_test;
    bool lighting;
    bool light0_enabled;
    bool cull_face;
    GLenum shade_model;

    /* Current attributes */
    float cur_r, cur_g, cur_b, cur_a;
    float cur_nx, cur_ny, cur_nz;
    float cur_u, cur_v;

    /* Light 0 */
    float light0_pos[4];
    float light0_diffuse[4];
    float light0_ambient[4];

    /* Material */
    float mat_diffuse[4];
    float mat_ambient[4];

    /* Vertex accumulation */
    GLenum current_mode;
    bool in_begin;
    struct gl_vertex vertex_buf[MAX_VERTICES];
    int vertex_count;

    /* Bound render target */
    uint32_t *color_buffer;
    float *depth_buffer;
    int target_width;
    int target_height;
};

static struct gl_internal_state g_gl;
static bool g_gl_initialized = false;

/* 4x4 Matrix math helpers */
static void mat4_identity(float *m) {
    memset(m, 0, 16 * sizeof(float));
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat4_copy(float *dst, const float *src) {
    memcpy(dst, src, 16 * sizeof(float));
}

static void mat4_mult(float *out, const float *a, const float *b) {
    float res[16];
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            res[r * 4 + c] = a[r * 4 + 0] * b[0 * 4 + c] +
                             a[r * 4 + 1] * b[1 * 4 + c] +
                             a[r * 4 + 2] * b[2 * 4 + c] +
                             a[r * 4 + 3] * b[3 * 4 + c];
        }
    }
    memcpy(out, res, 16 * sizeof(float));
}

static float *current_matrix(void) {
    if (g_gl.matrix_mode == GL_PROJECTION) {
        return g_gl.projection_stack[g_gl.projection_top];
    }
    return g_gl.modelview_stack[g_gl.modelview_top];
}

static void gl_init_defaults(void) {
    if (g_gl_initialized) return;
    memset(&g_gl, 0, sizeof(g_gl));

    g_gl.matrix_mode = GL_MODELVIEW;
    mat4_identity(g_gl.modelview_stack[0]);
    g_gl.modelview_top = 0;
    mat4_identity(g_gl.projection_stack[0]);
    g_gl.projection_top = 0;

    g_gl.vp_x = 0;
    g_gl.vp_y = 0;
    g_gl.vp_w = 640;
    g_gl.vp_h = 480;

    g_gl.clear_r = g_gl.clear_g = g_gl.clear_b = 0.0f;
    g_gl.clear_a = 1.0f;
    g_gl.clear_depth = 1.0f;

    g_gl.cur_r = g_gl.cur_g = g_gl.cur_b = g_gl.cur_a = 1.0f;
    g_gl.cur_nx = 0.0f; g_gl.cur_ny = 0.0f; g_gl.cur_nz = 1.0f;

    g_gl.light0_pos[0] = 5.0f; g_gl.light0_pos[1] = 5.0f; g_gl.light0_pos[2] = 10.0f; g_gl.light0_pos[3] = 1.0f;
    g_gl.light0_diffuse[0] = 1.0f; g_gl.light0_diffuse[1] = 1.0f; g_gl.light0_diffuse[2] = 1.0f; g_gl.light0_diffuse[3] = 1.0f;
    g_gl.light0_ambient[0] = 0.2f; g_gl.light0_ambient[1] = 0.2f; g_gl.light0_ambient[2] = 0.2f; g_gl.light0_ambient[3] = 1.0f;

    g_gl.mat_diffuse[0] = 0.8f; g_gl.mat_diffuse[1] = 0.8f; g_gl.mat_diffuse[2] = 0.8f; g_gl.mat_diffuse[3] = 1.0f;
    g_gl.mat_ambient[0] = 0.2f; g_gl.mat_ambient[1] = 0.2f; g_gl.mat_ambient[2] = 0.2f; g_gl.mat_ambient[3] = 1.0f;

    g_gl.depth_test = true;
    g_gl.shade_model = GL_SMOOTH;

    g_gl_initialized = true;
}

void gl_set_target(uint32_t *color_buf, float *depth_buf, int w, int h) {
    gl_init_defaults();
    g_gl.color_buffer = color_buf;
    g_gl.depth_buffer = depth_buf;
    g_gl.target_width = w;
    g_gl.target_height = h;
    if (g_gl.vp_w == 640 && g_gl.vp_h == 480) {
        g_gl.vp_w = w;
        g_gl.vp_h = h;
    }
}

void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) {
    gl_init_defaults();
    g_gl.clear_r = red; g_gl.clear_g = green; g_gl.clear_b = blue; g_gl.clear_a = alpha;
}

void glClearDepth(GLclampd depth) {
    gl_init_defaults();
    g_gl.clear_depth = (float)depth;
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    gl_init_defaults();
    g_gl.vp_x = x; g_gl.vp_y = y; g_gl.vp_w = width; g_gl.vp_h = height;
}

void glClear(GLbitfield mask) {
    gl_init_defaults();
    if (!g_gl.color_buffer || g_gl.target_width <= 0 || g_gl.target_height <= 0) return;

    int total_pixels = g_gl.target_width * g_gl.target_height;

    if (mask & GL_COLOR_BUFFER_BIT) {
        uint32_t r = (uint32_t)(g_gl.clear_r * 255.0f);
        uint32_t g = (uint32_t)(g_gl.clear_g * 255.0f);
        uint32_t b = (uint32_t)(g_gl.clear_b * 255.0f);
        uint32_t col = (r << 16) | (g << 8) | b;
        for (int i = 0; i < total_pixels; i++) {
            g_gl.color_buffer[i] = col;
        }
    }

    if ((mask & GL_DEPTH_BUFFER_BIT) && g_gl.depth_buffer) {
        for (int i = 0; i < total_pixels; i++) {
            g_gl.depth_buffer[i] = g_gl.clear_depth;
        }
    }
}

void glMatrixMode(GLenum mode) {
    gl_init_defaults();
    g_gl.matrix_mode = mode;
}

void glPushMatrix(void) {
    gl_init_defaults();
    if (g_gl.matrix_mode == GL_PROJECTION) {
        if (g_gl.projection_top < MATRIX_STACK_DEPTH - 1) {
            mat4_copy(g_gl.projection_stack[g_gl.projection_top + 1], g_gl.projection_stack[g_gl.projection_top]);
            g_gl.projection_top++;
        }
    } else {
        if (g_gl.modelview_top < MATRIX_STACK_DEPTH - 1) {
            mat4_copy(g_gl.modelview_stack[g_gl.modelview_top + 1], g_gl.modelview_stack[g_gl.modelview_top]);
            g_gl.modelview_top++;
        }
    }
}

void glPopMatrix(void) {
    gl_init_defaults();
    if (g_gl.matrix_mode == GL_PROJECTION) {
        if (g_gl.projection_top > 0) g_gl.projection_top--;
    } else {
        if (g_gl.modelview_top > 0) g_gl.modelview_top--;
    }
}

void glLoadIdentity(void) {
    gl_init_defaults();
    mat4_identity(current_matrix());
}

void glLoadMatrixf(const GLfloat *m) {
    gl_init_defaults();
    mat4_copy(current_matrix(), m);
}

void glMultMatrixf(const GLfloat *m) {
    gl_init_defaults();
    float *cur = current_matrix();
    mat4_mult(cur, cur, m);
}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z) {
    float t[16];
    mat4_identity(t);
    t[3] = x;
    t[7] = y;
    t[11] = z;
    glMultMatrixf(t);
}

void glScalef(GLfloat x, GLfloat y, GLfloat z) {
    float s[16];
    mat4_identity(s);
    s[0] = x;
    s[5] = y;
    s[10] = z;
    glMultMatrixf(s);
}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) {
    float len = sqrtf(x * x + y * y + z * z);
    if (len < 1e-6f) return;
    x /= len; y /= len; z /= len;

    float rad = angle * (3.1415926535f / 180.0f);
    float c = cosf(rad);
    float s = sinf(rad);
    float t = 1.0f - c;

    float r[16];
    mat4_identity(r);
    r[0]  = t * x * x + c;
    r[1]  = t * x * y - s * z;
    r[2]  = t * x * z + s * y;

    r[4]  = t * x * y + s * z;
    r[5]  = t * y * y + c;
    r[6]  = t * y * z - s * x;

    r[8]  = t * x * z - s * y;
    r[9]  = t * y * z + s * x;
    r[10] = t * z * z + c;

    glMultMatrixf(r);
}

void glOrtho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val) {
    float o[16];
    mat4_identity(o);
    float rl = (float)(right - left);
    float tb = (float)(top - bottom);
    float fn = (float)(far_val - near_val);
    if (rl == 0.0f || tb == 0.0f || fn == 0.0f) return;

    o[0]  = 2.0f / rl;
    o[3]  = -(float)(right + left) / rl;
    o[5]  = 2.0f / tb;
    o[7]  = -(float)(top + bottom) / tb;
    o[10] = -2.0f / fn;
    o[11] = -(float)(far_val + near_val) / fn;

    glMultMatrixf(o);
}

void glFrustum(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val) {
    float p[16];
    memset(p, 0, sizeof(p));
    float rl = (float)(right - left);
    float tb = (float)(top - bottom);
    float fn = (float)(far_val - near_val);
    if (rl == 0.0f || tb == 0.0f || fn == 0.0f) return;

    p[0]  = (2.0f * (float)near_val) / rl;
    p[2]  = (float)(right + left) / rl;
    p[5]  = (2.0f * (float)near_val) / tb;
    p[6]  = (float)(top + bottom) / tb;
    p[10] = -(float)(far_val + near_val) / fn;
    p[11] = -(2.0f * (float)far_val * (float)near_val) / fn;
    p[14] = -1.0f;

    glMultMatrixf(p);
}

void glEnable(GLenum cap) {
    gl_init_defaults();
    if (cap == GL_DEPTH_TEST) g_gl.depth_test = true;
    else if (cap == GL_LIGHTING) g_gl.lighting = true;
    else if (cap == GL_LIGHT0) g_gl.light0_enabled = true;
    else if (cap == GL_CULL_FACE) g_gl.cull_face = true;
}

void glDisable(GLenum cap) {
    gl_init_defaults();
    if (cap == GL_DEPTH_TEST) g_gl.depth_test = false;
    else if (cap == GL_LIGHTING) g_gl.lighting = false;
    else if (cap == GL_LIGHT0) g_gl.light0_enabled = false;
    else if (cap == GL_CULL_FACE) g_gl.cull_face = false;
}

GLboolean glIsEnabled(GLenum cap) {
    gl_init_defaults();
    if (cap == GL_DEPTH_TEST) return g_gl.depth_test;
    if (cap == GL_LIGHTING) return g_gl.lighting;
    if (cap == GL_LIGHT0) return g_gl.light0_enabled;
    if (cap == GL_CULL_FACE) return g_gl.cull_face;
    return GL_FALSE;
}

void glDepthFunc(GLenum func) { (void)func; }
void glDepthMask(GLboolean flag) { (void)flag; }
void glShadeModel(GLenum mode) { g_gl.shade_model = mode; }
void glCullFace(GLenum mode) { (void)mode; }
void glFrontFace(GLenum mode) { (void)mode; }

void glLightfv(GLenum light, GLenum pname, const GLfloat *params) {
    gl_init_defaults();
    if (light == GL_LIGHT0 && params) {
        if (pname == GL_POSITION) memcpy(g_gl.light0_pos, params, 4 * sizeof(float));
        else if (pname == GL_DIFFUSE) memcpy(g_gl.light0_diffuse, params, 4 * sizeof(float));
        else if (pname == GL_AMBIENT) memcpy(g_gl.light0_ambient, params, 4 * sizeof(float));
    }
}

void glLightf(GLenum light, GLenum pname, GLfloat param) {
    float p[4] = {param, param, param, 1.0f};
    glLightfv(light, pname, p);
}

void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params) {
    (void)face;
    gl_init_defaults();
    if (params) {
        if (pname == GL_DIFFUSE) memcpy(g_gl.mat_diffuse, params, 4 * sizeof(float));
        else if (pname == GL_AMBIENT) memcpy(g_gl.mat_ambient, params, 4 * sizeof(float));
    }
}

void glMaterialf(GLenum face, GLenum pname, GLfloat param) {
    float p[4] = {param, param, param, 1.0f};
    glMaterialfv(face, pname, p);
}

void glColor3f(GLfloat r, GLfloat g, GLfloat b) {
    gl_init_defaults();
    g_gl.cur_r = r; g_gl.cur_g = g; g_gl.cur_b = b; g_gl.cur_a = 1.0f;
}

void glColor3ub(GLubyte r, GLubyte g, GLubyte b) {
    glColor3f(r / 255.0f, g / 255.0f, b / 255.0f);
}

void glColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    gl_init_defaults();
    g_gl.cur_r = r; g_gl.cur_g = g; g_gl.cur_b = b; g_gl.cur_a = a;
}

void glColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) {
    glColor4f(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) {
    gl_init_defaults();
    float len = sqrtf(nx * nx + ny * ny + nz * nz);
    if (len > 1e-6f) {
        g_gl.cur_nx = nx / len;
        g_gl.cur_ny = ny / len;
        g_gl.cur_nz = nz / len;
    } else {
        g_gl.cur_nx = 0.0f; g_gl.cur_ny = 0.0f; g_gl.cur_nz = 1.0f;
    }
}

void glNormal3fv(const GLfloat *v) {
    if (v) glNormal3f(v[0], v[1], v[2]);
}

void glTexCoord2f(GLfloat s, GLfloat t) {
    g_gl.cur_u = s; g_gl.cur_v = t;
}

void glBegin(GLenum mode) {
    gl_init_defaults();
    g_gl.current_mode = mode;
    g_gl.in_begin = true;
    g_gl.vertex_count = 0;
}

void glVertex2f(GLfloat x, GLfloat y) {
    glVertex3f(x, y, 0.0f);
}

void glVertex3f(GLfloat x, GLfloat y, GLfloat z) {
    if (!g_gl.in_begin || g_gl.vertex_count >= MAX_VERTICES) return;
    struct gl_vertex *v = &g_gl.vertex_buf[g_gl.vertex_count++];
    v->x = x; v->y = y; v->z = z;
    v->nx = g_gl.cur_nx; v->ny = g_gl.cur_ny; v->nz = g_gl.cur_nz;
    v->r = g_gl.cur_r; v->g = g_gl.cur_g; v->b = g_gl.cur_b; v->a = g_gl.cur_a;
    v->u = g_gl.cur_u; v->v = g_gl.cur_v;
}

void glVertex3fv(const GLfloat *v) {
    if (v) glVertex3f(v[0], v[1], v[2]);
}

/* Transform object-space vertex to screen coordinates with Gouraud lighting */
static bool transform_vertex(const struct gl_vertex *in, struct gl_screen_vertex *out) {
    float *mv = g_gl.modelview_stack[g_gl.modelview_top];
    float *pj = g_gl.projection_stack[g_gl.projection_top];

    /* Eye space position */
    float ex = mv[0]*in->x + mv[1]*in->y + mv[2]*in->z + mv[3];
    float ey = mv[4]*in->x + mv[5]*in->y + mv[6]*in->z + mv[7];
    float ez = mv[8]*in->x + mv[9]*in->y + mv[10]*in->z + mv[11];
    float ew = mv[12]*in->x + mv[13]*in->y + mv[14]*in->z + mv[15];
    if (ew == 0.0f) ew = 1.0f;
    ex /= ew; ey /= ew; ez /= ew;

    /* Gouraud Lighting calculation */
    float r = in->r, g = in->g, b = in->b;
    if (g_gl.lighting && g_gl.light0_enabled) {
        /* Transform normal to eye space (upper 3x3) */
        float enx = mv[0]*in->nx + mv[1]*in->ny + mv[2]*in->nz;
        float eny = mv[4]*in->nx + mv[5]*in->ny + mv[6]*in->nz;
        float enz = mv[8]*in->nx + mv[9]*in->ny + mv[10]*in->nz;
        float nlen = sqrtf(enx*enx + eny*eny + enz*enz);
        if (nlen > 1e-6f) { enx /= nlen; eny /= nlen; enz /= nlen; }

        /* Light vector */
        float lx = g_gl.light0_pos[0] - ex;
        float ly = g_gl.light0_pos[1] - ey;
        float lz = g_gl.light0_pos[2] - ez;
        float llen = sqrtf(lx*lx + ly*ly + lz*lz);
        if (llen > 1e-6f) { lx /= llen; ly /= llen; lz /= llen; }

        float dot = enx*lx + eny*ly + enz*lz;
        if (dot < 0.0f) dot = 0.0f;

        r = (g_gl.mat_ambient[0] * g_gl.light0_ambient[0]) +
            (in->r * g_gl.light0_diffuse[0] * dot);
        g = (g_gl.mat_ambient[1] * g_gl.light0_ambient[1]) +
            (in->g * g_gl.light0_diffuse[1] * dot);
        b = (g_gl.mat_ambient[2] * g_gl.light0_ambient[2]) +
            (in->b * g_gl.light0_diffuse[2] * dot);

        if (r > 1.0f) r = 1.0f;
        if (g > 1.0f) g = 1.0f;
        if (b > 1.0f) b = 1.0f;
    }

    /* Clip space position */
    float cx = pj[0]*ex + pj[1]*ey + pj[2]*ez + pj[3];
    float cy = pj[4]*ex + pj[5]*ey + pj[6]*ez + pj[7];
    float cz = pj[8]*ex + pj[9]*ey + pj[10]*ez + pj[11];
    float cw = pj[12]*ex + pj[13]*ey + pj[14]*ez + pj[15];

    if (cw <= 0.001f) return false; /* Near plane clip */

    /* Normalized Device Coordinates */
    float nx = cx / cw;
    float ny = cy / cw;
    float nz = cz / cw;

    /* Viewport transform */
    out->sx = (float)g_gl.vp_x + (nx + 1.0f) * 0.5f * (float)g_gl.vp_w;
    out->sy = (float)g_gl.vp_y + (1.0f - (ny + 1.0f) * 0.5f) * (float)g_gl.vp_h;
    out->sz = (nz + 1.0f) * 0.5f;
    out->r = r; out->g = g; out->b = b; out->a = in->a;
    return true;
}

/* Sub-pixel accurate triangle rasterizer with Z-buffer */
static void rasterize_triangle(const struct gl_screen_vertex *v0,
                               const struct gl_screen_vertex *v1,
                               const struct gl_screen_vertex *v2) {
    if (!g_gl.color_buffer || g_gl.target_width <= 0 || g_gl.target_height <= 0) return;

    /* Backface culling check */
    float cross = (v1->sx - v0->sx) * (v2->sy - v0->sy) - (v1->sy - v0->sy) * (v2->sx - v0->sx);
    if (g_gl.cull_face && cross <= 0.0f) return;

    /* Bounding box of triangle */
    int min_x = (int)fminf(v0->sx, fminf(v1->sx, v2->sx));
    int max_x = (int)fmaxf(v0->sx, fmaxf(v1->sx, v2->sx)) + 1;
    int min_y = (int)fminf(v0->sy, fminf(v1->sy, v2->sy));
    int max_y = (int)fmaxf(v0->sy, fmaxf(v1->sy, v2->sy)) + 1;

    /* Clamp to target render buffer */
    if (min_x < 0) min_x = 0;
    if (max_x > g_gl.target_width) max_x = g_gl.target_width;
    if (min_y < 0) min_y = 0;
    if (max_y > g_gl.target_height) max_y = g_gl.target_height;
    if (min_x >= max_x || min_y >= max_y) return;

    float x0 = v0->sx, y0 = v0->sy;
    float x1 = v1->sx, y1 = v1->sy;
    float x2 = v2->sx, y2 = v2->sy;

    float denom = (x0 - x2) * (y1 - y2) - (x1 - x2) * (y0 - y2);
    if (fabsf(denom) < 1e-6f) return;
    float inv_denom = 1.0f / denom;

    for (int y = min_y; y < max_y; y++) {
        float py = (float)y + 0.5f;
        int row_offset = y * g_gl.target_width;

        for (int x = min_x; x < max_x; x++) {
            float px = (float)x + 0.5f;
            float dx = px - x2;
            float dy = py - y2;

            float w0 = (dx * (y1 - y2) - dy * (x1 - x2)) * inv_denom;
            float w1 = ((x0 - x2) * dy - (y0 - y2) * dx) * inv_denom;
            float w2 = 1.0f - w0 - w1;

            if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                float z = w0 * v0->sz + w1 * v1->sz + w2 * v2->sz;
                int idx = row_offset + x;

                if (!g_gl.depth_test || !g_gl.depth_buffer || z < g_gl.depth_buffer[idx]) {
                    if (g_gl.depth_buffer) g_gl.depth_buffer[idx] = z;

                    float r = w0 * v0->r + w1 * v1->r + w2 * v2->r;
                    float g = w0 * v0->g + w1 * v1->g + w2 * v2->g;
                    float b = w0 * v0->b + w1 * v1->b + w2 * v2->b;

                    uint32_t ir = (uint32_t)(fminf(fmaxf(r, 0.0f), 1.0f) * 255.0f);
                    uint32_t ig = (uint32_t)(fminf(fmaxf(g, 0.0f), 1.0f) * 255.0f);
                    uint32_t ib = (uint32_t)(fminf(fmaxf(b, 0.0f), 1.0f) * 255.0f);

                    g_gl.color_buffer[idx] = (ir << 16) | (ig << 8) | ib;
                }
            }
        }
    }
}

void glEnd(void) {
    if (!g_gl.in_begin) return;
    g_gl.in_begin = false;

    if (g_gl.vertex_count < 3) return;

    struct gl_screen_vertex sv[MAX_VERTICES];
    bool valid[MAX_VERTICES];
    for (int i = 0; i < g_gl.vertex_count; i++) {
        valid[i] = transform_vertex(&g_gl.vertex_buf[i], &sv[i]);
    }

    if (g_gl.current_mode == GL_TRIANGLES) {
        for (int i = 0; i + 2 < g_gl.vertex_count; i += 3) {
            if (valid[i] && valid[i+1] && valid[i+2]) {
                rasterize_triangle(&sv[i], &sv[i+1], &sv[i+2]);
            }
        }
    } else if (g_gl.current_mode == GL_QUADS) {
        for (int i = 0; i + 3 < g_gl.vertex_count; i += 4) {
            if (valid[i] && valid[i+1] && valid[i+2]) {
                rasterize_triangle(&sv[i], &sv[i+1], &sv[i+2]);
            }
            if (valid[i] && valid[i+2] && valid[i+3]) {
                rasterize_triangle(&sv[i], &sv[i+2], &sv[i+3]);
            }
        }
    } else if (g_gl.current_mode == GL_TRIANGLE_FAN) {
        for (int i = 1; i + 1 < g_gl.vertex_count; i++) {
            if (valid[0] && valid[i] && valid[i+1]) {
                rasterize_triangle(&sv[0], &sv[i], &sv[i+1]);
            }
        }
    } else if (g_gl.current_mode == GL_TRIANGLE_STRIP) {
        for (int i = 0; i + 2 < g_gl.vertex_count; i++) {
            if (valid[i] && valid[i+1] && valid[i+2]) {
                if (i % 2 == 0) {
                    rasterize_triangle(&sv[i], &sv[i+1], &sv[i+2]);
                } else {
                    rasterize_triangle(&sv[i+1], &sv[i], &sv[i+2]);
                }
            }
        }
    }
}

void glFlush(void) {}
void glFinish(void) {}
