#ifndef _BOOT_BOOT_INFO_H
#define _BOOT_BOOT_INFO_H

#include <dunix/types.h>
#include <boot/multiboot.h>

#define MAX_MEMORY_REGIONS 64

struct memory_region {
    uint64_t base;
    uint64_t length;
    uint32_t type; /* 1 = Usable RAM, 2 = Reserved, 3 = ACPI, etc. */
};

struct boot_info {
    char                 bootloader_name[64];
    char                 cmdline[256];
    uint64_t             total_memory_bytes;
    uint64_t             usable_memory_bytes;
    size_t               region_count;
    struct memory_region regions[MAX_MEMORY_REGIONS];
    uint64_t             kernel_phys_start;
    uint64_t             kernel_phys_end;
    uint64_t             kernel_virt_start;
    uint64_t             kernel_virt_end;
    bool                 has_framebuffer;
    uint64_t             fb_addr;
    uint32_t             fb_pitch;
    uint32_t             fb_width;
    uint32_t             fb_height;
    uint8_t              fb_bpp;
};

extern struct boot_info g_boot_info;

void boot_info_init(uint32_t magic, uint32_t mb_addr);
void boot_info_dump(void);
void boot_info_save_data_snapshot(void);
ssize_t kernel_read_clean_image(uint64_t offset, size_t size, void *buffer);
size_t kernel_get_clean_image_size(void);

#endif /* _BOOT_BOOT_INFO_H */
