#ifndef _LIBC_SYS_IOCTL_H
#define _LIBC_SYS_IOCTL_H

#define TIOCGWINSZ  0x5413
#define TIOCSWINSZ  0x5414
#define TCGETS      0x5401
#define TCSETS      0x5402
#define TCSETSW     0x5403
#define TCSETSF     0x5404

#define FBIOGET_VSCREENINFO 0x4600
#define FBIOPUT_VSCREENINFO 0x4601
#define FBIOGET_FSCREENINFO 0x4602

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

struct fb_var_screeninfo {
    unsigned int xres;
    unsigned int yres;
    unsigned int xres_virtual;
    unsigned int yres_virtual;
    unsigned int xoffset;
    unsigned int yoffset;
    unsigned int bits_per_pixel;
    unsigned int grayscale;
    unsigned int red_offset;
    unsigned int red_length;
    unsigned int green_offset;
    unsigned int green_length;
    unsigned int blue_offset;
    unsigned int blue_length;
    unsigned int transp_offset;
    unsigned int transp_length;
};

struct fb_fix_screeninfo {
    char id[16];
    unsigned long smem_start;
    unsigned int smem_len;
    unsigned int type;
    unsigned int visual;
    unsigned int line_length;
    unsigned long mmio_start;
    unsigned int mmio_len;
};

int ioctl(int fd, unsigned long request, ...);

#endif /* _LIBC_SYS_IOCTL_H */
