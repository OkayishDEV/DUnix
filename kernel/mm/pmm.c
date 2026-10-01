#include <mm/pmm.h>
#include <arch/x86_64/mm/paging.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>
#include <dunix/stdbool.h>

static uint8_t *pmm_bitmap = NULL;
static size_t   pmm_total_frames = 0;
static size_t   pmm_total_ram_frames = 0;
static size_t   pmm_free_frames_count = 0;
static size_t   pmm_bitmap_size_bytes = 0;
static size_t   pmm_last_alloc_idx = 0;

static inline void bitmap_set(size_t frame) {
    pmm_bitmap[frame / 8] |= (uint8_t)(1 << (frame % 8));
}

static inline void bitmap_clear(size_t frame) {
    pmm_bitmap[frame / 8] &= (uint8_t)~(1 << (frame % 8));
}

static inline bool bitmap_test(size_t frame) {
    return (pmm_bitmap[frame / 8] & (uint8_t)(1 << (frame % 8))) != 0;
}

static void mark_region_used(paddr_t base, size_t length) {
    size_t start_frame = base / PMM_FRAME_SIZE;
    size_t end_frame = (base + length + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;

    if (end_frame > pmm_total_frames) {
        end_frame = pmm_total_frames;
    }

    for (size_t f = start_frame; f < end_frame; f++) {
        if (!bitmap_test(f)) {
            bitmap_set(f);
            if (pmm_free_frames_count > 0) {
                pmm_free_frames_count--;
            }
        }
    }
}

static void mark_region_free(paddr_t base, size_t length) {
    size_t start_frame = (base + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE;
    size_t end_frame = (base + length) / PMM_FRAME_SIZE;

    if (end_frame > pmm_total_frames) {
        end_frame = pmm_total_frames;
    }

    for (size_t f = start_frame; f < end_frame; f++) {
        if (bitmap_test(f)) {
            bitmap_clear(f);
            pmm_free_frames_count++;
        }
    }
}

void pmm_init(const struct boot_info *boot) {
    paddr_t max_phys_addr = 0;

    for (size_t i = 0; i < boot->region_count; i++) {
        paddr_t top = boot->regions[i].base + boot->regions[i].length;
        if (top > max_phys_addr) {
            max_phys_addr = top;
        }
    }

    /* Cap physical address to 4 GB for early physical allocator if huge */
    if (max_phys_addr == 0 || max_phys_addr > (4ULL * 1024 * 1024 * 1024)) {
        max_phys_addr = 4ULL * 1024 * 1024 * 1024;
    }

    pmm_total_frames = max_phys_addr / PMM_FRAME_SIZE;
    pmm_bitmap_size_bytes = (pmm_total_frames + 7) / 8;
    pmm_free_frames_count = 0;

    /* Place bitmap immediately after kernel physical end, aligned to page boundary */
    paddr_t bitmap_phys_addr = ALIGN_UP(boot->kernel_phys_end, PMM_FRAME_SIZE);
    pmm_bitmap = (uint8_t *)PHYS_TO_VIRT(bitmap_phys_addr);

    /* Initially mark all frames as USED */
    memset(pmm_bitmap, 0xFF, pmm_bitmap_size_bytes);

    /* Mark all available regions as FREE */
    for (size_t i = 0; i < boot->region_count; i++) {
        if (boot->regions[i].type == MULTIBOOT_MEMORY_AVAILABLE) {
            mark_region_free(boot->regions[i].base, boot->regions[i].length);
        }
    }

    /* Mark lower 1 MB as USED (BIOS, VGA, Real mode IVT) */
    mark_region_used(0x00000000, 0x00100000);

    /* Mark kernel physical image as USED */
    mark_region_used(boot->kernel_phys_start, boot->kernel_phys_end - boot->kernel_phys_start);

    /* Mark PMM bitmap memory itself as USED */
    mark_region_used(bitmap_phys_addr, pmm_bitmap_size_bytes);

    paddr_t max_ram_addr = 0;
    for (size_t i = 0; i < boot->region_count; i++) {
        if (boot->regions[i].type == MULTIBOOT_MEMORY_AVAILABLE) {
            paddr_t top = boot->regions[i].base + boot->regions[i].length;
            if (top > max_ram_addr) {
                max_ram_addr = top;
            }
        }
    }

    if (max_ram_addr > 0) {
        paddr_t ram_bytes = ALIGN_UP(max_ram_addr, 1024ULL * 1024ULL);
        pmm_total_ram_frames = ram_bytes / PMM_FRAME_SIZE;
    } else {
        pmm_total_ram_frames = pmm_total_frames;
    }

    klog(KLOG_INFO, "PMM initialized: %lu RAM frames (%lu MB), %lu free (%lu MB free)\n",
         pmm_total_ram_frames,
         (pmm_total_ram_frames * PMM_FRAME_SIZE) / (1024 * 1024),
         pmm_free_frames_count,
         (pmm_free_frames_count * PMM_FRAME_SIZE) / (1024 * 1024));
}

paddr_t pmm_alloc_frame(void) {
    for (size_t i = 0; i < pmm_total_frames; i++) {
        size_t idx = (pmm_last_alloc_idx + i) % pmm_total_frames;
        if (!bitmap_test(idx)) {
            bitmap_set(idx);
            pmm_free_frames_count--;
            pmm_last_alloc_idx = (idx + 1) % pmm_total_frames;

            paddr_t paddr = (paddr_t)idx * PMM_FRAME_SIZE;
            /* Zero newly allocated frame */
            memset((void *)PHYS_TO_VIRT(paddr), 0, PMM_FRAME_SIZE);
            return paddr;
        }
    }

    kpanic("PMM: Out of physical memory frames! total=%lu free=%lu last=%lu\n",
           pmm_total_frames, pmm_free_frames_count, pmm_last_alloc_idx);
    return 0;
}

paddr_t pmm_alloc_frames(size_t count) {
    if (count == 0) {
        return 0;
    }
    if (count == 1) {
        return pmm_alloc_frame();
    }

    size_t consecutive = 0;
    size_t start_frame = 0;

    for (size_t i = 0; i < pmm_total_frames; i++) {
        if (!bitmap_test(i)) {
            if (consecutive == 0) {
                start_frame = i;
            }
            consecutive++;
            if (consecutive == count) {
                for (size_t f = start_frame; f < start_frame + count; f++) {
                    bitmap_set(f);
                }
                pmm_free_frames_count -= count;
                paddr_t paddr = (paddr_t)start_frame * PMM_FRAME_SIZE;
                memset((void *)PHYS_TO_VIRT(paddr), 0, count * PMM_FRAME_SIZE);
                return paddr;
            }
        } else {
            consecutive = 0;
        }
    }

    kpanic("PMM: Failed to allocate %lu contiguous physical frames!\n", count);
    return 0;
}

void pmm_free_frame(paddr_t paddr) {
    size_t frame = paddr / PMM_FRAME_SIZE;
    if (frame < pmm_total_frames) {
        if (bitmap_test(frame)) {
            bitmap_clear(frame);
            pmm_free_frames_count++;
        }
    }
}

void pmm_free_frames(paddr_t paddr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        pmm_free_frame(paddr + i * PMM_FRAME_SIZE);
    }
}

size_t pmm_get_total_frames(void) {
    return pmm_total_frames;
}

size_t pmm_get_free_frames(void) {
    return pmm_free_frames_count;
}

size_t pmm_get_used_frames(void) {
    if (pmm_total_ram_frames > 0 && pmm_total_ram_frames >= pmm_free_frames_count) {
        return pmm_total_ram_frames - pmm_free_frames_count;
    }
    return pmm_total_frames - pmm_free_frames_count;
}

size_t pmm_get_free_memory_kb(void) {
    return (pmm_free_frames_count * PMM_FRAME_SIZE) / 1024;
}

size_t pmm_get_total_memory_kb(void) {
    if (pmm_total_ram_frames > 0) {
        return (pmm_total_ram_frames * PMM_FRAME_SIZE) / 1024;
    }
    return (pmm_total_frames * PMM_FRAME_SIZE) / 1024;
}
