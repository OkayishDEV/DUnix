/*
 * drm_info - DUnix DRM/KMS Diagnostic & Inspection Tool
 *
 * Copyright (c) 2026 DUnix Project. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

int main(int argc, char **argv) {
    const char *card = "/dev/dri/card0";
    if (argc > 1) card = argv[1];

    printf("=====================================================\n");
    printf("   DUnix Direct Rendering Manager & KMS Inspector    \n");
    printf("=====================================================\n");
    printf("Opening Device: %s\n", card);

    int fd = drmOpen(card, NULL);
    if (fd < 0) {
        perror("drmOpen failed");
        printf("Error: Could not open DRM device %s\n", card);
        return 1;
    }

    /* 1. Query Driver Version */
    drmVersionPtr ver = drmGetVersion(fd);
    if (ver) {
        printf("\n[DRM Driver Information]\n");
        printf("  Driver Name:    %s\n", ver->name);
        printf("  Driver Date:    %s\n", ver->date);
        printf("  Driver Desc:    %s\n", ver->desc);
        printf("  Driver Version: %d.%d.%d\n", ver->version_major, ver->version_minor, ver->version_patchlevel);
        drmFreeVersion(ver);
    } else {
        printf("Failed to retrieve DRM driver version\n");
    }

    /* 2. Query KMS Mode Resources */
    drmModeResPtr res = drmModeGetResources(fd);
    if (res) {
        printf("\n[KMS Card Resources]\n");
        printf("  CRTC Count:       %d\n", res->count_crtcs);
        printf("  Connector Count:  %d\n", res->count_connectors);
        printf("  Encoder Count:    %d\n", res->count_encoders);
        printf("  Framebuffer Count:%d\n", res->count_fbs);
        printf("  Resolution Range: %ux%u min, %ux%u max\n",
               res->min_width, res->min_height, res->max_width, res->max_height);

        /* Inspect Connectors */
        for (int i = 0; i < res->count_connectors; i++) {
            drmModeConnectorPtr conn = drmModeGetConnector(fd, res->connectors[i]);
            if (conn) {
                printf("\n  [Connector %u]\n", conn->connector_id);
                printf("    Type:       %s (ID %u)\n",
                       conn->connector_type == DRM_MODE_CONNECTOR_VGA ? "VGA / BGA" : "Other",
                       conn->connector_type);
                printf("    Status:     %s\n",
                       conn->connection == DRM_MODE_CONNECTED ? "Connected" : "Disconnected");
                printf("    Physical:   %umm x %umm\n", conn->mmWidth, conn->mmHeight);
                printf("    Modes:      %d available\n", conn->count_modes);

                for (int m = 0; m < conn->count_modes; m++) {
                    printf("      Mode %d: %s (%ux%u @ %u Hz)\n",
                           m + 1,
                           conn->modes[m].name,
                           conn->modes[m].hdisplay,
                           conn->modes[m].vdisplay,
                           conn->modes[m].vrefresh);
                }
                drmModeFreeConnector(conn);
            }
        }

        /* Inspect CRTCs */
        for (int i = 0; i < res->count_crtcs; i++) {
            drmModeCrtcPtr crtc = drmModeGetCrtc(fd, res->crtcs[i]);
            if (crtc) {
                printf("\n  [CRTC %u]\n", crtc->crtc_id);
                printf("    Current Mode: %s (%ux%u)\n",
                       crtc->mode.name, crtc->width, crtc->height);
                printf("    FB Attached:  %u\n", crtc->buffer_id);
                printf("    Offset:       (%u, %u)\n", crtc->x, crtc->y);
                drmModeFreeCrtc(crtc);
            }
        }

        drmModeFreeResources(res);
    } else {
        printf("Failed to retrieve KMS resources\n");
    }

    /* 3. Test GEM Dumb Buffer Allocation */
    printf("\n[GEM Dumb Buffer Allocation Test]\n");
    uint32_t handle = 0, pitch = 0;
    uint64_t size = 0;
    int ret = drm_create_dumb_buffer(fd, 1024, 768, 32, &handle, &pitch, &size);
    if (ret == 0) {
        printf("  Allocation: Success! Handle=%u, Pitch=%u bytes, Size=%llu bytes\n",
               handle, pitch, (unsigned long long)size);

        void *map_ptr = drm_map_dumb_buffer(fd, handle, size);
        if (map_ptr) {
            printf("  Direct VRAM Memory Mapping (mmap): Success at %p (Zero-Copy VRAM Access)\n", map_ptr);
        } else {
            printf("  Direct VRAM Memory Mapping: Failed\n");
        }
        drm_destroy_dumb_buffer(fd, handle);
    } else {
        printf("  Allocation: Failed\n");
    }

    printf("\n=====================================================\n");
    drmClose(fd);
    return 0;
}
