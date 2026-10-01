#include <arch/x86_64/drivers/bga.h>
#include <arch/x86_64/drivers/pci.h>
#include <arch/x86_64/io.h>
#include <mm/vmm.h>
#include <mm/heap.h>
#include <boot/boot_info.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

static bool bga_hardware_active = false;
static bool fb_active = false;
static uint32_t fb_width = FB_DEFAULT_WIDTH;
static uint32_t fb_height = FB_DEFAULT_HEIGHT;
static uint32_t fb_bpp = FB_DEFAULT_BPP;
static uint64_t fb_phys_addr = 0xFD000000ULL;
static uint32_t *fb_virt_addr = (uint32_t *)FB_VIRT_BASE;

static inline void bga_write_register(uint16_t index, uint16_t data) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, data);
}

static inline uint16_t bga_read_register(uint16_t index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

void bga_init(void) {
    /* 1. Check for Bochs / QEMU or VirtualBox BGA hardware */
    struct pci_device *pci_dev = pci_find_device(0x1234, 0x1111); /* Bochs / QEMU */
    if (!pci_dev) {
        pci_dev = pci_find_device(0x80ee, 0xbeef); /* VirtualBox */
    }

    if (pci_dev) {
        bga_write_register(VBE_DISPI_INDEX_ID, 0xB0C5);
        uint16_t id = bga_read_register(VBE_DISPI_INDEX_ID);

        if (id >= 0xB0C0 && id <= 0xB0C6) {
            pci_enable_bus_mastering(pci_dev);
            if (pci_dev->bar0) {
                fb_phys_addr = pci_dev->bar0;
            }
            bga_hardware_active = true;
            fb_active = true;

            size_t map_size = 16 * 1024 * 1024;
            vmm_map_range(vmm_get_kernel_pml4(), FB_VIRT_BASE, fb_phys_addr, map_size,
                          PTE_PRESENT | PTE_WRITABLE | PTE_PCD);

            klog(KLOG_INFO, "BGA Graphics Driver initialized: Phys 0x%lx -> Virt 0x%lx (16 MB)\n",
                 fb_phys_addr, FB_VIRT_BASE);
            return;
        }
    }

    /* 2. Check for Multiboot linear framebuffer (VESA / GOP) */
    if (g_boot_info.has_framebuffer && g_boot_info.fb_addr) {
        fb_phys_addr = g_boot_info.fb_addr;
        fb_width = g_boot_info.fb_width ? g_boot_info.fb_width : FB_DEFAULT_WIDTH;
        fb_height = g_boot_info.fb_height ? g_boot_info.fb_height : FB_DEFAULT_HEIGHT;
        fb_bpp = g_boot_info.fb_bpp ? g_boot_info.fb_bpp : FB_DEFAULT_BPP;

        size_t map_size = (size_t)fb_width * fb_height * (fb_bpp / 8);
        if (map_size < 16 * 1024 * 1024) map_size = 16 * 1024 * 1024;
        vmm_map_range(vmm_get_kernel_pml4(), FB_VIRT_BASE, fb_phys_addr, map_size,
                      PTE_PRESENT | PTE_WRITABLE | PTE_PCD);

        fb_virt_addr = (uint32_t *)FB_VIRT_BASE;
        fb_active = true;
        klog(KLOG_INFO, "Multiboot Framebuffer active: %ux%ux%u at Phys 0x%lx -> Virt 0x%lx\n",
             fb_width, fb_height, fb_bpp, fb_phys_addr, FB_VIRT_BASE);
        return;
    }

    /* 3. Fallback in-memory framebuffer (bare metal GPU text console mode) */
    size_t fb_size = (size_t)FB_DEFAULT_WIDTH * FB_DEFAULT_HEIGHT * (FB_DEFAULT_BPP / 8);
    void *mem_fb = kmalloc(fb_size);
    if (mem_fb) {
        memset(mem_fb, 0, fb_size);
        fb_virt_addr = (uint32_t *)mem_fb;
        fb_phys_addr = VIRT_TO_PHYS(mem_fb);
        fb_width = FB_DEFAULT_WIDTH;
        fb_height = FB_DEFAULT_HEIGHT;
        fb_bpp = FB_DEFAULT_BPP;
        fb_active = true;
        klog(KLOG_INFO, "Virtual Framebuffer allocated in RAM (%ux%ux%u) at Virt 0x%lx\n",
             fb_width, fb_height, fb_bpp, (uint64_t)mem_fb);
        return;
    }

    klog(KLOG_WARN, "Framebuffer initialization failed (no display hardware or memory)\n");
}

bool bga_is_available(void) {
    return fb_active;
}

static uint32_t fb_y_offset = 0;

void bga_set_video_mode(uint32_t width, uint32_t height, uint32_t bpp) {
    if (!fb_active) return;

    fb_width = width;
    fb_height = height;
    fb_bpp = bpp;
    fb_y_offset = 0;

    if (bga_hardware_active) {
        bga_write_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
        bga_write_register(VBE_DISPI_INDEX_XRES, (uint16_t)width);
        bga_write_register(VBE_DISPI_INDEX_YRES, (uint16_t)height);
        bga_write_register(VBE_DISPI_INDEX_BPP, (uint16_t)bpp);
        bga_write_register(VBE_DISPI_INDEX_VIRT_WIDTH, (uint16_t)width);
        bga_write_register(VBE_DISPI_INDEX_VIRT_HEIGHT, (uint16_t)(height * 2));
        bga_write_register(VBE_DISPI_INDEX_X_OFFSET, 0);
        bga_write_register(VBE_DISPI_INDEX_Y_OFFSET, 0);
        bga_write_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
    }
}

void bga_set_display_offset(uint32_t x_offset, uint32_t y_offset) {
    fb_y_offset = y_offset;
    if (bga_hardware_active) {
        bga_write_register(VBE_DISPI_INDEX_X_OFFSET, (uint16_t)x_offset);
        bga_write_register(VBE_DISPI_INDEX_Y_OFFSET, (uint16_t)y_offset);
    }
}

uint32_t bga_get_display_y_offset(void) {
    return fb_y_offset;
}

void bga_set_virtual_height(uint32_t virt_height) {
    if (bga_hardware_active) {
        bga_write_register(VBE_DISPI_INDEX_VIRT_HEIGHT, (uint16_t)virt_height);
    }
}

uint64_t bga_get_phys_addr(void) {
    return fb_phys_addr;
}

void bga_disable(void) {
    if (bga_hardware_active) {
        bga_write_register(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    }
}

uint32_t *bga_get_framebuffer(void) {
    return fb_virt_addr;
}

uint32_t bga_get_width(void) {
    return fb_width;
}

uint32_t bga_get_height(void) {
    return fb_height;
}

uint32_t bga_get_bpp(void) {
    return fb_bpp;
}

ssize_t fb_device_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node;
    if (!fb_active || !buffer || !fb_virt_addr) return -1;
    size_t total_fb_bytes = (size_t)fb_width * fb_height * (fb_bpp / 8);
    if (offset >= total_fb_bytes) return 0;
    if (offset + size > total_fb_bytes) size = total_fb_bytes - offset;

    uint8_t *fb_ptr = (uint8_t *)fb_virt_addr + offset;
    memcpy(buffer, fb_ptr, size);
    return (ssize_t)size;
}

ssize_t fb_device_write(struct vfs_node *node, uint64_t offset, size_t size, const void *buffer) {
    (void)node;
    if (!fb_active || !buffer || !fb_virt_addr) return -1;
    size_t total_fb_bytes = (size_t)fb_width * fb_height * (fb_bpp / 8);
    if (offset >= total_fb_bytes) return 0;
    if (offset + size > total_fb_bytes) size = total_fb_bytes - offset;

    uint8_t *fb_ptr = (uint8_t *)fb_virt_addr + offset;
    memcpy(fb_ptr, buffer, size);
    return (ssize_t)size;
}

int fb_device_ioctl(struct vfs_node *node, unsigned long request, void *arg) {
    (void)node;
    if (!fb_active) return -19; /* -ENODEV */

    if (request == FBIOGET_VSCREENINFO) {
        if (!arg) return -14; /* -EFAULT */
        struct fb_var_screeninfo *var = (struct fb_var_screeninfo *)arg;
        memset(var, 0, sizeof(struct fb_var_screeninfo));
        var->xres = fb_width;
        var->yres = fb_height;
        var->xres_virtual = fb_width;
        var->yres_virtual = fb_height;
        var->bits_per_pixel = fb_bpp;
        var->red_offset = 16;
        var->red_length = 8;
        var->green_offset = 8;
        var->green_length = 8;
        var->blue_offset = 0;
        var->blue_length = 8;
        return 0;
    } else if (request == FBIOPUT_VSCREENINFO) {
        if (!arg) return -14;
        struct fb_var_screeninfo *var = (struct fb_var_screeninfo *)arg;
        bga_set_video_mode(var->xres, var->yres, var->bits_per_pixel);
        return 0;
    } else if (request == FBIOGET_FSCREENINFO) {
        if (!arg) return -14;
        struct fb_fix_screeninfo *fix = (struct fb_fix_screeninfo *)arg;
        memset(fix, 0, sizeof(struct fb_fix_screeninfo));
        strncpy(fix->id, "dunix-fb", sizeof(fix->id) - 1);
        fix->smem_start = fb_phys_addr;
        fix->smem_len = 16 * 1024 * 1024;
        fix->line_length = fb_width * (fb_bpp / 8);
        return 0;
    }

    return -22; /* -EINVAL */
}
