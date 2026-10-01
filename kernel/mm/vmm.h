#ifndef _MM_VMM_H
#define _MM_VMM_H

#include <dunix/types.h>
#include <dunix/stdbool.h>
#include <arch/x86_64/mm/paging.h>
#include <arch/x86_64/cpu/idt.h>

#define VMM_FLAG_PRESENT   PTE_PRESENT
#define VMM_FLAG_WRITABLE  PTE_WRITABLE
#define VMM_FLAG_USER      PTE_USER
#define VMM_FLAG_NX        PTE_NX
#define VMM_FLAG_GLOBAL    PTE_GLOBAL

/* Standard Userspace Memory Boundaries */
#define USER_SPACE_START   0x0000000000400000ULL /* 4 MB (avoid NULL page) */
#define USER_SPACE_END     0x00007FFFFFFFFFFFULL
#define USER_STACK_TOP     0x00007FFFFFFFE000ULL
#define USER_STACK_PAGES   512                   /* 2 MB default user stack */

void      vmm_init(void);
uint64_t *vmm_get_kernel_pml4(void);
uint64_t *vmm_create_address_space(void);
void      vmm_destroy_address_space(uint64_t *pml4);
uint64_t *vmm_clone_address_space(uint64_t *src_pml4);

bool      vmm_map_page(uint64_t *pml4, vaddr_t vaddr, paddr_t paddr, uint64_t flags);
void      vmm_unmap_page(uint64_t *pml4, vaddr_t vaddr);
bool      vmm_map_range(uint64_t *pml4, vaddr_t vaddr, paddr_t paddr, size_t size, uint64_t flags);
void      vmm_unmap_range(uint64_t *pml4, vaddr_t vaddr, size_t size);

paddr_t   vmm_get_phys(uint64_t *pml4, vaddr_t vaddr);
void      vmm_switch_pml4(uint64_t *pml4);

void      page_fault_handler(struct interrupt_frame *frame);

#endif /* _MM_VMM_H */
