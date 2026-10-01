/*
 * glgears - 3D Hardware Accelerated OpenGL Gear / Cube Demo for DUnix
 *
 * Demonstrates the DUnix 3D Graphics Stack:
 * Kernel DRM/KMS -> libdrm -> libGL (OpenGL 1.x) -> DWS/DRM Scanout.
 *
 * Copyright (c) 2026 DUnix Project. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>
#include <xf86drm.h>
#include <dui/dui.h>

static volatile bool g_running = true;
static void handle_signal(int sig) {
    (void)sig;
    g_running = false;
}

#define WIN_W 400
#define WIN_H 360

/* Draw a lit 3D gear or faceted cube face with proper normals */
static void draw_box(float size) {
    float h = size * 0.5f;

    /* Front Face (Red) */
    glNormal3f(0.0f, 0.0f, 1.0f);
    glColor3f(0.9f, 0.2f, 0.2f);
    glBegin(GL_QUADS);
    glVertex3f(-h, -h,  h);
    glVertex3f( h, -h,  h);
    glVertex3f( h,  h,  h);
    glVertex3f(-h,  h,  h);
    glEnd();

    /* Back Face (Green) */
    glNormal3f(0.0f, 0.0f, -1.0f);
    glColor3f(0.2f, 0.8f, 0.2f);
    glBegin(GL_QUADS);
    glVertex3f(-h, -h, -h);
    glVertex3f(-h,  h, -h);
    glVertex3f( h,  h, -h);
    glVertex3f( h, -h, -h);
    glEnd();

    /* Top Face (Blue) */
    glNormal3f(0.0f, 1.0f, 0.0f);
    glColor3f(0.2f, 0.4f, 0.9f);
    glBegin(GL_QUADS);
    glVertex3f(-h,  h, -h);
    glVertex3f(-h,  h,  h);
    glVertex3f( h,  h,  h);
    glVertex3f( h,  h, -h);
    glEnd();

    /* Bottom Face (Yellow) */
    glNormal3f(0.0f, -1.0f, 0.0f);
    glColor3f(0.9f, 0.8f, 0.2f);
    glBegin(GL_QUADS);
    glVertex3f(-h, -h, -h);
    glVertex3f( h, -h, -h);
    glVertex3f( h, -h,  h);
    glVertex3f(-h, -h,  h);
    glEnd();

    /* Right Face (Cyan) */
    glNormal3f(1.0f, 0.0f, 0.0f);
    glColor3f(0.2f, 0.8f, 0.9f);
    glBegin(GL_QUADS);
    glVertex3f( h, -h, -h);
    glVertex3f( h,  h, -h);
    glVertex3f( h,  h,  h);
    glVertex3f( h, -h,  h);
    glEnd();

    /* Left Face (Magenta) */
    glNormal3f(-1.0f, 0.0f, 0.0f);
    glColor3f(0.9f, 0.2f, 0.8f);
    glBegin(GL_QUADS);
    glVertex3f(-h, -h, -h);
    glVertex3f(-h, -h,  h);
    glVertex3f(-h,  h,  h);
    glVertex3f(-h,  h, -h);
    glEnd();
}

/* Draw an interlocking 3D solid gear with hub and radial teeth */
static void draw_gear(float inner_radius, float outer_radius, float width, int teeth, float tooth_depth) {
    float r0 = inner_radius;
    float r1 = outer_radius - tooth_depth * 0.5f;
    float r2 = outer_radius + tooth_depth * 0.5f;
    float hw = width * 0.5f;
    float da = 2.0f * 3.14159265f / (float)teeth;

    /* Front and Back faces */
    for (int side = -1; side <= 1; side += 2) {
        float z = (float)side * hw;
        glNormal3f(0.0f, 0.0f, (float)side);

        glBegin(GL_QUADS);
        for (int i = 0; i < teeth; i++) {
            float a0 = (float)i * da;
            float a1 = a0 + da * 0.5f;
            float a2 = a0 + da;

            /* 1. Hub quad */
            if (side > 0) {
                glVertex3f(r0 * cosf(a0), r0 * sinf(a0), z);
                glVertex3f(r1 * cosf(a0), r1 * sinf(a0), z);
                glVertex3f(r1 * cosf(a2), r1 * sinf(a2), z);
                glVertex3f(r0 * cosf(a2), r0 * sinf(a2), z);

                /* 2. Tooth quad */
                glVertex3f(r1 * cosf(a0), r1 * sinf(a0), z);
                glVertex3f(r2 * cosf(a0), r2 * sinf(a0), z);
                glVertex3f(r2 * cosf(a1), r2 * sinf(a1), z);
                glVertex3f(r1 * cosf(a1), r1 * sinf(a1), z);
            } else {
                glVertex3f(r0 * cosf(a2), r0 * sinf(a2), z);
                glVertex3f(r1 * cosf(a2), r1 * sinf(a2), z);
                glVertex3f(r1 * cosf(a0), r1 * sinf(a0), z);
                glVertex3f(r0 * cosf(a0), r0 * sinf(a0), z);

                glVertex3f(r1 * cosf(a1), r1 * sinf(a1), z);
                glVertex3f(r2 * cosf(a1), r2 * sinf(a1), z);
                glVertex3f(r2 * cosf(a0), r2 * sinf(a0), z);
                glVertex3f(r1 * cosf(a0), r1 * sinf(a0), z);
            }
        }
        glEnd();
    }

    /* Outer rim / tooth tip sides */
    glBegin(GL_QUADS);
    for (int i = 0; i < teeth; i++) {
        float a0 = (float)i * da;
        float a1 = a0 + da * 0.5f;

        /* Tooth tip normal */
        float mid_a = a0 + da * 0.25f;
        glNormal3f(cosf(mid_a), sinf(mid_a), 0.0f);

        glVertex3f(r2 * cosf(a0), r2 * sinf(a0), -hw);
        glVertex3f(r2 * cosf(a1), r2 * sinf(a1), -hw);
        glVertex3f(r2 * cosf(a1), r2 * sinf(a1),  hw);
        glVertex3f(r2 * cosf(a0), r2 * sinf(a0),  hw);
    }
    glEnd();
}

int main(int argc, char **argv) {
    bool fullscreen_drm = false;
    int max_frames = 0;
    int max_seconds = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--drm") == 0 || strcmp(argv[i], "-d") == 0) {
            fullscreen_drm = true;
        } else if ((strcmp(argv[i], "--frames") == 0 || strcmp(argv[i], "-f") == 0) && i + 1 < argc) {
            max_frames = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--seconds") == 0 || strcmp(argv[i], "-s") == 0) && i + 1 < argc) {
            max_seconds = atoi(argv[++i]);
        }
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("[glgears] Initializing DUnix 3D OpenGL Engine...\n");

    GLXContext gl_ctx = NULL;
    DuiConnection *conn = NULL;
    DuiWindow win = 0;
    int width = WIN_W, height = WIN_H;

    if (fullscreen_drm) {
        int drm_fd = drmOpen("/dev/dri/card0", NULL);
        if (drm_fd < 0) {
            fprintf(stderr, "[glgears] Failed to open /dev/dri/card0\n");
            return 1;
        }
        width = 1024;
        height = 768;
        gl_ctx = glCreateDrmContext(drm_fd, width, height);
        if (!gl_ctx) {
            fprintf(stderr, "[glgears] Failed to create DRM hardware GL context\n");
            close(drm_fd);
            return 1;
        }
        printf("[glgears] Running in Direct DRM/KMS Fullscreen Mode (%dx%d, Hardware Page-Flipping)\n", width, height);
    } else {
        conn = dui_connect();
        if (!conn) {
            printf("[glgears] No DWS display server found; attempting fullscreen DRM fallback...\n");
            int drm_fd = drmOpen("/dev/dri/card0", NULL);
            if (drm_fd >= 0) {
                width = 1024; height = 768;
                gl_ctx = glCreateDrmContext(drm_fd, width, height);
            }
            if (!gl_ctx) {
                fprintf(stderr, "[glgears] Error: Neither DWS nor /dev/dri/card0 could be initialized\n");
                return 1;
            }
        } else {
            win = dui_create_window(conn, 100, 80, WIN_W, WIN_H, "OpenGL 3D Gears & Cube Demo", 0x0014171F, 0);
            dui_show(conn, win);
            gl_ctx = glCreateDuiContext(conn, win, WIN_W, WIN_H);
            printf("[glgears] Running inside DWS Window via DUI (%dx%d, 3D Accelerated)\n", WIN_W, WIN_H);
        }
    }

    /* Initialize OpenGL states */
    glViewport(0, 0, width, height);
    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    float light_pos[4] = {4.0f, 6.0f, 8.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, (double)width / (double)height, 1.0, 100.0);

    glMatrixMode(GL_MODELVIEW);

    float rot_x = 20.0f;
    float rot_y = 30.0f;
    float rot_z = 0.0f;

    int frames = 0;
    int total_frames = 0;
    time_t start_time = time(NULL);
    time_t last_time = start_time;

    printf("[glgears] 3D Animation running. Rendering 3D meshes...\n");

    /* Continuous render loop */
    while (g_running) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -7.0f);

        glRotatef(rot_x, 1.0f, 0.0f, 0.0f);
        glRotatef(rot_y, 0.0f, 1.0f, 0.0f);
        glRotatef(rot_z, 0.0f, 0.0f, 1.0f);

        /* 1. Draw central rotating 3D box */
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, 0.0f);
        draw_box(1.8f);
        glPopMatrix();

        /* 2. Draw surrounding planetary 3D gears */
        glPushMatrix();
        glTranslatef(2.6f, 0.0f, 0.0f);
        glRotatef(rot_y * 2.0f, 0.0f, 0.0f, 1.0f);
        glColor3f(0.8f, 0.3f, 0.2f);
        draw_gear(0.3f, 1.0f, 0.4f, 12, 0.3f);
        glPopMatrix();

        glPushMatrix();
        glTranslatef(-2.6f, 0.0f, 0.0f);
        glRotatef(-rot_y * 2.0f, 0.0f, 0.0f, 1.0f);
        glColor3f(0.2f, 0.6f, 0.9f);
        draw_gear(0.3f, 1.0f, 0.4f, 12, 0.3f);
        glPopMatrix();

        glXSwapBuffers(NULL, win);

        rot_x += 1.5f;
        rot_y += 2.5f;
        rot_z += 0.8f;
        frames++;
        total_frames++;

        time_t now = time(NULL);
        if (now - last_time >= 2) {
            int fps = (int)(frames / (now - last_time));
            printf("[glgears] Rendered %d frames (%d FPS) - Hardware Z-Buffer & Shading Active\n", frames, fps);
            frames = 0;
            last_time = now;
        }

        /* Check frame or duration limits */
        if (max_frames > 0 && total_frames >= max_frames) {
            break;
        }
        if (max_seconds > 0 && (now - start_time) >= max_seconds) {
            break;
        }

        /* Handle DUI events if in windowed mode */
        if (conn) {
            DuiEvent ev;
            while (dui_poll_event(conn, &ev)) {
                if (ev.type == DWS_EV_CLOSE_REQ) {
                    goto cleanup;
                } else if (ev.type == DWS_EV_KEY_DOWN) {
                    char c = ev.key.ch;
                    if (c == 'q' || c == 'Q' || c == 27 || c == 3 || c == 'x' || c == 'X') {
                        goto cleanup;
                    }
                }
            }
        }
        usleep(16000); /* ~60 FPS cap */
    }

cleanup:
    printf("[glgears] Completed 3D execution successfully.\n");
    if (gl_ctx) glXDestroyContext(NULL, gl_ctx);
    if (conn && win) {
        dui_destroy_window(conn, win);
    }
    if (conn) dui_disconnect(conn);
    return 0;
}
