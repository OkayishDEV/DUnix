#ifndef _ARCH_X86_64_MM_PAGING_H
#define _ARCH_X86_64_MM_PAGING_H

#include <dunix/types.h>

#define KERNEL_VIRTUAL_BASE  0xFFFFFFFF80000000ULL
#define KERNEL_PHYSICAL_BASE 0x0000000000100000ULL /* 1 MB */

#define PAGE_SIZE_4K 4096ULL
#define PAGE_SIZE_2M (2 * 1024 * 1024ULL)
#define PAGE_SIZE_1G (1024 * 1024 * 1024ULL)

#define PAGE_SHIFT_4K 12
#define PAGE_SHIFT_2M 21
#define PAGE_SHIFT_1G 30

#define PTE_PRESENT   (1ULL << 0)
#define PTE_WRITABLE  (1ULL << 1)
#define PTE_USER      (1ULL << 2)
#define PTE_PWT       (1ULL << 3)
#define PTE_PCD       (1ULL << 4)
#define PTE_ACCESSED  (1ULL << 5)
#define PTE_DIRTY     (1ULL << 6)
#define PTE_HUGE      (1ULL << 7)
#define PTE_GLOBAL    (1ULL << 8)
#define PTE_NX        (1ULL << 63)

#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

#define PHYS_TO_VIRT(p) ((vaddr_t)((paddr_t)(p) + KERNEL_VIRTUAL_BASE))
#define VIRT_TO_PHYS(v) ((paddr_t)((vaddr_t)(v) - KERNEL_VIRTUAL_BASE))

#define PML4_INDEX(v) (((vaddr_t)(v) >> 39) & 0x1FF)
#define PDPT_INDEX(v) (((vaddr_t)(v) >> 30) & 0x1FF)
#define PD_INDEX(v)   (((vaddr_t)(v) >> 21) & 0x1FF)
#define PT_INDEX(v)   (((vaddr_t)(v) >> 12) & 0x1FF)

static inline void invlpg(vaddr_t vaddr) {
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

#endif /* _ARCH_X86_64_MM_PAGING_H */
