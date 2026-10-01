#include <boot/boot_info.h>
#include <arch/x86_64/mm/paging.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

struct boot_info g_boot_info;

extern char _kernel_start[];
extern char _kernel_end[];
extern char _kernel_phys_start[];
extern char _kernel_phys_end[];
extern char _boot_start[];
extern char _boot_bss_start[];
extern char _boot_bss_end[];
extern char _data_start[];
extern char _data_end[];

static uint8_t g_clean_data_snapshot[4096];
static size_t  g_clean_data_size = 0;

void boot_info_save_data_snapshot(void) {
    size_t data_len = (size_t)(_data_end - _data_start);
    if (data_len > sizeof(g_clean_data_snapshot)) {
        data_len = sizeof(g_clean_data_snapshot);
    }
    memcpy(g_clean_data_snapshot, _data_start, data_len);
    g_clean_data_size = data_len;
}

void boot_info_init(uint32_t magic, uint32_t mb_addr) {
    boot_info_save_data_snapshot();
    memset(&g_boot_info, 0, sizeof(struct boot_info));

    g_boot_info.kernel_virt_start = (uint64_t)_kernel_start;
    g_boot_info.kernel_virt_end   = (uint64_t)_kernel_end;
    g_boot_info.kernel_phys_start = (uint64_t)_kernel_phys_start;
    g_boot_info.kernel_phys_end   = (uint64_t)_kernel_phys_end;

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        klog(KLOG_WARN, "Unknown bootloader magic: 0x%08x (expected 0x2BADB002)\n", magic);
        return;
    }

    /* Access multiboot info struct via higher-half mapping */
    multiboot_info_t *mbi = (multiboot_info_t *)PHYS_TO_VIRT(mb_addr);

    if (mbi->flags & MULTIBOOT_INFO_BOOT_LOADER && mbi->boot_loader_name) {
        const char *name = (const char *)PHYS_TO_VIRT(mbi->boot_loader_name);
        strncpy(g_boot_info.bootloader_name, name, sizeof(g_boot_info.bootloader_name) - 1);
    } else {
        strcpy(g_boot_info.bootloader_name, "Multiboot Compliant Loader");
    }

    if (mbi->flags & MULTIBOOT_INFO_CMDLINE && mbi->cmdline) {
        const char *cmd = (const char *)PHYS_TO_VIRT(mbi->cmdline);
        strncpy(g_boot_info.cmdline, cmd, sizeof(g_boot_info.cmdline) - 1);
    }

    if (mbi->flags & MULTIBOOT_INFO_MEM_MAP && mbi->mmap_addr && mbi->mmap_length > 0) {
        uint64_t mmap_cur = (uint64_t)PHYS_TO_VIRT(mbi->mmap_addr);
        uint64_t mmap_end = mmap_cur + mbi->mmap_length;

        while (mmap_cur < mmap_end && g_boot_info.region_count < MAX_MEMORY_REGIONS) {
            struct multiboot_mmap_entry *entry = (struct multiboot_mmap_entry *)mmap_cur;

            struct memory_region *r = &g_boot_info.regions[g_boot_info.region_count++];
            r->base   = entry->addr;
            r->length = entry->len;
            r->type   = entry->type;

            g_boot_info.total_memory_bytes += entry->len;
            if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
                g_boot_info.usable_memory_bytes += entry->len;
            }

            mmap_cur += entry->size + sizeof(uint32_t);
        }
    } else if (mbi->flags & MULTIBOOT_INFO_MEMORY) {
        /* Fallback to basic lower + upper memory */
        g_boot_info.usable_memory_bytes = ((uint64_t)mbi->mem_upper + 1024) * 1024;
        g_boot_info.total_memory_bytes  = g_boot_info.usable_memory_bytes;
        g_boot_info.regions[0].base   = 0x100000;
        g_boot_info.regions[0].length = (uint64_t)mbi->mem_upper * 1024;
        g_boot_info.regions[0].type   = MULTIBOOT_MEMORY_AVAILABLE;
        g_boot_info.region_count = 1;
    }

    /* Check Multiboot Framebuffer */
    if (mbi->flags & MULTIBOOT_INFO_FRAMEBUFFER) {
        g_boot_info.has_framebuffer = true;
        g_boot_info.fb_addr   = mbi->framebuffer_addr;
        g_boot_info.fb_pitch  = mbi->framebuffer_pitch;
        g_boot_info.fb_width  = mbi->framebuffer_width;
        g_boot_info.fb_height = mbi->framebuffer_height;
        g_boot_info.fb_bpp    = mbi->framebuffer_bpp;
    }
}

void boot_info_dump(void) {
    klog(KLOG_INFO, "Bootloader:    %s\n", g_boot_info.bootloader_name);
    if (g_boot_info.cmdline[0]) {
        klog(KLOG_INFO, "Cmdline:       %s\n", g_boot_info.cmdline);
    }
    if (g_boot_info.has_framebuffer) {
        klog(KLOG_INFO, "Framebuffer:   %ux%ux%u at Phys 0x%lx (Pitch: %u)\n",
             g_boot_info.fb_width, g_boot_info.fb_height, g_boot_info.fb_bpp,
             g_boot_info.fb_addr, g_boot_info.fb_pitch);
    }
    klog(KLOG_INFO, "Kernel Virtual:  0x%016lx - 0x%016lx (%lu KB)\n",
         g_boot_info.kernel_virt_start, g_boot_info.kernel_virt_end,
         (g_boot_info.kernel_virt_end - g_boot_info.kernel_virt_start) / 1024);
    klog(KLOG_INFO, "Kernel Physical: 0x%016lx - 0x%016lx\n",
         g_boot_info.kernel_phys_start, g_boot_info.kernel_phys_end);

    klog(KLOG_INFO, "Total RAM:     %lu MB (Usable: %lu MB)\n",
         g_boot_info.total_memory_bytes / (1024 * 1024),
         g_boot_info.usable_memory_bytes / (1024 * 1024));

    klog(KLOG_INFO, "Physical Memory Map (%lu entries):\n", g_boot_info.region_count);
    for (size_t i = 0; i < g_boot_info.region_count; i++) {
        struct memory_region *r = &g_boot_info.regions[i];
        const char *type_str = "Unknown";
        if (r->type == MULTIBOOT_MEMORY_AVAILABLE)        type_str = "Usable RAM";
        else if (r->type == MULTIBOOT_MEMORY_RESERVED)   type_str = "Reserved";
        else if (r->type == MULTIBOOT_MEMORY_ACPI_RECLAIMABLE) type_str = "ACPI Reclaimable";
        else if (r->type == MULTIBOOT_MEMORY_NVS)        type_str = "ACPI NVS";
        else if (r->type == MULTIBOOT_MEMORY_BADRAM)     type_str = "Bad RAM";

        kprintf("  [%02lu] 0x%016lx - 0x%016lx (%8lu KB) : %s\n",
                i, r->base, r->base + r->length - 1, r->length / 1024, type_str);
    }
}

size_t kernel_get_clean_image_size(void) {
    uint64_t phys_start = (uint64_t)_kernel_phys_start;
    uint64_t data_end_phys = (uint64_t)_data_end - 0xFFFFFFFF80000000;
    if (data_end_phys <= phys_start) return 0;
    return (size_t)(data_end_phys - phys_start);
}

ssize_t kernel_read_clean_image(uint64_t offset, size_t size, void *buffer) {
    uint64_t phys_start = (uint64_t)_kernel_phys_start;
    uint64_t data_end_phys = (uint64_t)_data_end - 0xFFFFFFFF80000000;
    if (data_end_phys <= phys_start) return 0;
    size_t total_len = (size_t)(data_end_phys - phys_start);

    if (offset >= total_len) return 0;
    if (offset + size > total_len) size = total_len - offset;

    uint8_t *out = (uint8_t *)buffer;
    uint64_t bss_start_off = 0x1000; /* 0x101000 - 0x100000 */
    uint64_t bss_end_off   = 0x7000; /* 0x107000 - 0x100000 */

    uint64_t data_phys = (uint64_t)_data_start - 0xFFFFFFFF80000000;
    uint64_t data_start_off = data_phys - phys_start;
    uint64_t data_end_off   = data_start_off + g_clean_data_size;

    for (size_t i = 0; i < size; i++) {
        uint64_t cur = offset + i;
        if (cur >= bss_start_off && cur < bss_end_off) {
            out[i] = 0;
        } else if (cur >= data_start_off && cur < data_end_off) {
            out[i] = g_clean_data_snapshot[cur - data_start_off];
        } else {
            out[i] = *((const uint8_t *)PHYS_TO_VIRT(phys_start + cur));
        }
    }
    return (ssize_t)size;
}
