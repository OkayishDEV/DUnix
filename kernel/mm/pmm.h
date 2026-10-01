#ifndef _MM_PMM_H
#define _MM_PMM_H

#include <dunix/types.h>
#include <boot/boot_info.h>

#define PMM_FRAME_SIZE 4096ULL

void    pmm_init(const struct boot_info *boot);
paddr_t pmm_alloc_frame(void);
paddr_t pmm_alloc_frames(size_t count);
void    pmm_free_frame(paddr_t paddr);
void    pmm_free_frames(paddr_t paddr, size_t count);

size_t  pmm_get_total_frames(void);
size_t  pmm_get_free_frames(void);
size_t  pmm_get_used_frames(void);
size_t  pmm_get_free_memory_kb(void);
size_t  pmm_get_total_memory_kb(void);

#endif /* _MM_PMM_H */
