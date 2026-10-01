#ifndef _DRIVERS_BGA_H
#define _DRIVERS_BGA_H

#include <dunix/types.h>
#include <fs/vfs.h>

#define VBE_DISPI_IOPORT_INDEX 0x01CE
#define VBE_DISPI_IOPORT_DATA  0x01CF

#define VBE_DISPI_INDEX_ID          0x0
#define VBE_DISPI_INDEX_XRES        0x1
#define VBE_DISPI_INDEX_YRES        0x2
#define VBE_DISPI_INDEX_BPP         0x3
#define VBE_DISPI_INDEX_ENABLE      0x4
#define VBE_DISPI_INDEX_BANK        0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH  0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 0x7
#define VBE_DISPI_INDEX_X_OFFSET    0x8
#define VBE_DISPI_INDEX_Y_OFFSET    0x9

#define VBE_DISPI_DISABLED      0x00
#define VBE_DISPI_ENABLED       0x01
#define VBE_DISPI_LFB_ENABLED   0x40
#define VBE_DISPI_NOCLEARMEM    0x80

#define FB_DEFAULT_WIDTH  1024
#define FB_DEFAULT_HEIGHT 768
#define FB_DEFAULT_BPP    32

#define FB_VIRT_BASE 0xFFFFFFFFC0000000ULL

/* Linux framebuffer ioctl requests */
#define FBIOGET_VSCREENINFO 0x4600
#define FBIOPUT_VSCREENINFO 0x4601
#define FBIOGET_FSCREENINFO 0x4602

struct fb_var_screeninfo {
    uint32_t xres;
    uint32_t yres;
    uint32_t xres_virtual;
    uint32_t yres_virtual;
    uint32_t xoffset;
    uint32_t yoffset;
    uint32_t bits_per_pixel;
    uint32_t grayscale;
    uint32_t red_offset;
    uint32_t red_length;
    uint32_t green_offset;
    uint32_t green_length;
    uint32_t blue_offset;
    uint32_t blue_length;
    uint32_t transp_offset;
    uint32_t transp_length;
};

struct fb_fix_screeninfo {
    char id[16];
    uint64_t smem_start;
    uint32_t smem_len;
    uint32_t type;
    uint32_t visual;
    uint32_t line_length;
    uint64_t mmio_start;
    uint32_t mmio_len;
};

void bga_init(void);
bool bga_is_available(void);
void bga_set_video_mode(uint32_t width, uint32_t height, uint32_t bpp);
void bga_disable(void);
uint32_t *bga_get_framebuffer(void);
uint32_t bga_get_width(void);
uint32_t bga_get_height(void);
uint32_t bga_get_bpp(void);
uint64_t bga_get_phys_addr(void);
void bga_set_display_offset(uint32_t x_offset, uint32_t y_offset);
uint32_t bga_get_display_y_offset(void);
void bga_set_virtual_height(uint32_t virt_height);

ssize_t fb_device_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer);
ssize_t fb_device_write(struct vfs_node *node, uint64_t offset, size_t size, const void *buffer);
int fb_device_ioctl(struct vfs_node *node, unsigned long request, void *arg);

#endif /* _DRIVERS_BGA_H */
