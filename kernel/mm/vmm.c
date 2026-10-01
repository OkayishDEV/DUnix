#include <mm/vmm.h>
#include <mm/pmm.h>
#include <process/process.h>
#include <arch/x86_64/io.h>
#include <arch/x86_64/drivers/bga.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

static uint64_t *kernel_pml4 = NULL;
static paddr_t   kernel_pml4_phys = 0;

static uint64_t *get_or_create_table(uint64_t *parent_table, size_t index, uint64_t flags) {
    if (parent_table[index] & PTE_PRESENT) {
        if (parent_table[index] & PTE_HUGE) {
            return NULL; /* Cannot traverse into huge page */
        }
        paddr_t table_phys = parent_table[index] & PTE_ADDR_MASK;
        return (uint64_t *)PHYS_TO_VIRT(table_phys);
    }

    paddr_t new_table_phys = pmm_alloc_frame();
    if (!new_table_phys) {
        return NULL;
    }

    uint64_t *new_table_virt = (uint64_t *)PHYS_TO_VIRT(new_table_phys);
    memset(new_table_virt, 0, PAGE_SIZE_4K);

    /* Set table entry in parent with Present, Writable, and User if requested */
    parent_table[index] = new_table_phys | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    return new_table_virt;
}

bool vmm_map_page(uint64_t *pml4, vaddr_t vaddr, paddr_t paddr, uint64_t flags) {
    size_t pml4_idx = PML4_INDEX(vaddr);
    size_t pdpt_idx = PDPT_INDEX(vaddr);
    size_t pd_idx   = PD_INDEX(vaddr);
    size_t pt_idx   = PT_INDEX(vaddr);

    uint64_t *pdpt = get_or_create_table(pml4, pml4_idx, flags);
    if (!pdpt) return false;

    uint64_t *pd = get_or_create_table(pdpt, pdpt_idx, flags);
    if (!pd) return false;

    uint64_t *pt = get_or_create_table(pd, pd_idx, flags);
    if (!pt) return false;

    pt[pt_idx] = (paddr & PTE_ADDR_MASK) | flags | PTE_PRESENT;
    invlpg(vaddr);
    return true;
}

void vmm_unmap_page(uint64_t *pml4, vaddr_t vaddr) {
    size_t pml4_idx = PML4_INDEX(vaddr);
    size_t pdpt_idx = PDPT_INDEX(vaddr);
    size_t pd_idx   = PD_INDEX(vaddr);
    size_t pt_idx   = PT_INDEX(vaddr);

    if (!(pml4[pml4_idx] & PTE_PRESENT)) return;
    uint64_t *pdpt = (uint64_t *)PHYS_TO_VIRT(pml4[pml4_idx] & PTE_ADDR_MASK);

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) return;
    uint64_t *pd = (uint64_t *)PHYS_TO_VIRT(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    if (!(pd[pd_idx] & PTE_PRESENT) || (pd[pd_idx] & PTE_HUGE)) return;
    uint64_t *pt = (uint64_t *)PHYS_TO_VIRT(pd[pd_idx] & PTE_ADDR_MASK);

    pt[pt_idx] = 0;
    invlpg(vaddr);
}

bool vmm_map_range(uint64_t *pml4, vaddr_t vaddr, paddr_t paddr, size_t size, uint64_t flags) {
    size_t pages = (size + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
    for (size_t i = 0; i < pages; i++) {
        if (!vmm_map_page(pml4, vaddr + i * PAGE_SIZE_4K, paddr + i * PAGE_SIZE_4K, flags)) {
            return false;
        }
    }
    return true;
}

void vmm_unmap_range(uint64_t *pml4, vaddr_t vaddr, size_t size) {
    size_t pages = (size + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
    for (size_t i = 0; i < pages; i++) {
        vmm_unmap_page(pml4, vaddr + i * PAGE_SIZE_4K);
    }
}

paddr_t vmm_get_phys(uint64_t *pml4, vaddr_t vaddr) {
    size_t pml4_idx = PML4_INDEX(vaddr);
    size_t pdpt_idx = PDPT_INDEX(vaddr);
    size_t pd_idx   = PD_INDEX(vaddr);
    size_t pt_idx   = PT_INDEX(vaddr);

    if (!(pml4[pml4_idx] & PTE_PRESENT)) return 0;
    uint64_t *pdpt = (uint64_t *)PHYS_TO_VIRT(pml4[pml4_idx] & PTE_ADDR_MASK);

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) return 0;
    uint64_t *pd = (uint64_t *)PHYS_TO_VIRT(pdpt[pdpt_idx] & PTE_ADDR_MASK);

    if (!(pd[pd_idx] & PTE_PRESENT)) return 0;
    /* Check for 2MB huge page */
    if (pd[pd_idx] & PTE_HUGE) {
        paddr_t base_2m = pd[pd_idx] & 0x000FFFFFFFE00000ULL;
        return base_2m + (vaddr & 0x1FFFFFULL);
    }

    uint64_t *pt = (uint64_t *)PHYS_TO_VIRT(pd[pd_idx] & PTE_ADDR_MASK);
    if (!(pt[pt_idx] & PTE_PRESENT)) return 0;

    return (pt[pt_idx] & PTE_ADDR_MASK) | (vaddr & 0xFFFULL);
}

uint64_t *vmm_get_kernel_pml4(void) {
    return kernel_pml4;
}

void vmm_switch_pml4(uint64_t *pml4) {
    paddr_t pml4_phys = VIRT_TO_PHYS(pml4);
    write_cr3(pml4_phys);
}

uint64_t *vmm_create_address_space(void) {
    paddr_t pml4_phys = pmm_alloc_frame();
    if (!pml4_phys) return NULL;

    uint64_t *pml4_virt = (uint64_t *)PHYS_TO_VIRT(pml4_phys);
    memset(pml4_virt, 0, PAGE_SIZE_4K);

    /* Copy kernel space mappings (entries 256..511) */
    for (size_t i = 256; i < 512; i++) {
        pml4_virt[i] = kernel_pml4[i];
    }

    return pml4_virt;
}

void vmm_destroy_address_space(uint64_t *pml4) {
    if (!pml4 || pml4 == kernel_pml4) return;

    paddr_t vram_base = bga_get_phys_addr();
    paddr_t vram_end = vram_base + 16 * 1024 * 1024;
    paddr_t max_ram_paddr = (paddr_t)pmm_get_total_frames() * PAGE_SIZE_4K;

    /* Free all user space entries (0..255) */
    for (size_t pml4_i = 0; pml4_i < 256; pml4_i++) {
        if (!(pml4[pml4_i] & PTE_PRESENT)) continue;
        uint64_t *pdpt = (uint64_t *)PHYS_TO_VIRT(pml4[pml4_i] & PTE_ADDR_MASK);

        for (size_t pdpt_i = 0; pdpt_i < 512; pdpt_i++) {
            if (!(pdpt[pdpt_i] & PTE_PRESENT)) continue;
            uint64_t *pd = (uint64_t *)PHYS_TO_VIRT(pdpt[pdpt_i] & PTE_ADDR_MASK);

            for (size_t pd_i = 0; pd_i < 512; pd_i++) {
                if (!(pd[pd_i] & PTE_PRESENT)) continue;
                if (pd[pd_i] & PTE_HUGE) continue;

                uint64_t *pt = (uint64_t *)PHYS_TO_VIRT(pd[pd_i] & PTE_ADDR_MASK);
                for (size_t pt_i = 0; pt_i < 512; pt_i++) {
                    if (pt[pt_i] & PTE_PRESENT) {
                        uint64_t flags = pt[pt_i] & ~PTE_ADDR_MASK;
                        paddr_t page_frame = pt[pt_i] & PTE_ADDR_MASK;
                        /* Never free hardware GPU VRAM or MMIO frames */
                        if (!(flags & PTE_PCD) &&
                            (page_frame < max_ram_paddr) &&
                            (!vram_base || page_frame < vram_base || page_frame >= vram_end)) {
                            pmm_free_frame(page_frame);
                        }
                    }
                }
                pmm_free_frame(pd[pd_i] & PTE_ADDR_MASK);
            }
            pmm_free_frame(pdpt[pdpt_i] & PTE_ADDR_MASK);
        }
        pmm_free_frame(pml4[pml4_i] & PTE_ADDR_MASK);
    }

    pmm_free_frame(VIRT_TO_PHYS(pml4));
}

uint64_t *vmm_clone_address_space(uint64_t *src_pml4) {
    uint64_t *dst_pml4 = vmm_create_address_space();
    if (!dst_pml4) return NULL;

    paddr_t vram_base = bga_get_phys_addr();
    paddr_t vram_end = vram_base + 16 * 1024 * 1024;
    paddr_t max_ram_paddr = (paddr_t)pmm_get_total_frames() * PAGE_SIZE_4K;

    /* Clone user space mappings (0..255) */
    for (size_t pml4_i = 0; pml4_i < 256; pml4_i++) {
        if (!(src_pml4[pml4_i] & PTE_PRESENT)) continue;
        uint64_t *src_pdpt = (uint64_t *)PHYS_TO_VIRT(src_pml4[pml4_i] & PTE_ADDR_MASK);

        for (size_t pdpt_i = 0; pdpt_i < 512; pdpt_i++) {
            if (!(src_pdpt[pdpt_i] & PTE_PRESENT)) continue;
            uint64_t *src_pd = (uint64_t *)PHYS_TO_VIRT(src_pdpt[pdpt_i] & PTE_ADDR_MASK);

            for (size_t pd_i = 0; pd_i < 512; pd_i++) {
                if (!(src_pd[pd_i] & PTE_PRESENT)) continue;
                if (src_pd[pd_i] & PTE_HUGE) continue;

                uint64_t *src_pt = (uint64_t *)PHYS_TO_VIRT(src_pd[pd_i] & PTE_ADDR_MASK);
                for (size_t pt_i = 0; pt_i < 512; pt_i++) {
                    if (src_pt[pt_i] & PTE_PRESENT) {
                        vaddr_t vaddr = ((vaddr_t)pml4_i << 39) |
                                        ((vaddr_t)pdpt_i << 30) |
                                        ((vaddr_t)pd_i << 21) |
                                        ((vaddr_t)pt_i << 12);
                        uint64_t flags = src_pt[pt_i] & ~PTE_ADDR_MASK;
                        paddr_t src_frame = src_pt[pt_i] & PTE_ADDR_MASK;

                        /* If this page is MMIO or Hardware GPU VRAM, share mapping instead of deep-copying into RAM */
                        if ((flags & PTE_PCD) ||
                            (src_frame >= max_ram_paddr) ||
                            (vram_base && src_frame >= vram_base && src_frame < vram_end)) {
                            vmm_map_page(dst_pml4, vaddr, src_frame, flags);
                            continue;
                        }

                        paddr_t new_frame = pmm_alloc_frame();
                        if (!new_frame) {
                            vmm_destroy_address_space(dst_pml4);
                            return NULL;
                        }

                        memcpy((void *)PHYS_TO_VIRT(new_frame), (void *)PHYS_TO_VIRT(src_frame), PAGE_SIZE_4K);

                        vmm_map_page(dst_pml4, vaddr, new_frame, flags);
                    }
                }
            }
        }
    }

    return dst_pml4;
}

void page_fault_handler(struct interrupt_frame *frame) {
    uint64_t fault_addr = read_cr2();
    bool present = (frame->error_code & (1 << 0)) != 0;
    bool write   = (frame->error_code & (1 << 1)) != 0;
    bool user    = (frame->error_code & (1 << 2)) != 0;
    bool insn    = (frame->error_code & (1 << 4)) != 0;

    /* Demand-paged User Stack Expansion (auto-grow stack downwards) */
    if (user && !present && fault_addr < USER_STACK_TOP && fault_addr >= (USER_STACK_TOP - 8 * 1024 * 1024ULL)) {
        struct process *proc = process_get_current();
        if (proc && proc->pml4) {
            uint64_t page_vaddr = ALIGN_DOWN(fault_addr, PAGE_SIZE_4K);
            paddr_t frame_phys = pmm_alloc_frame();
            if (frame_phys) {
                memset((void *)PHYS_TO_VIRT(frame_phys), 0, PAGE_SIZE_4K);
                vmm_map_page(proc->pml4, page_vaddr, frame_phys, VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER);
                return; /* Successfully resolved via demand-paged stack expansion */
            }
        }
    }

    kpanic("PAGE FAULT (#PF) at 0x%016lx\n"
           " Cause: [%s] [%s] [%s] [%s]\n"
           " RIP:   0x%016lx  RSP: 0x%016lx\n"
           " CR3:   0x%016lx  RFLAGS: 0x%016lx\n",
           fault_addr,
           present ? "Page Protection Violation" : "Non-Present Page",
           write ? "Write" : "Read",
           user ? "User Mode" : "Kernel Mode",
           insn ? "Instruction Fetch" : "Data Access",
           frame->rip, frame->rsp,
           read_cr3(), frame->rflags);
}

void vmm_init(void) {
    /* Register Page Fault handler (Vector 14) */
    register_interrupt_handler(14, page_fault_handler);

    /* Allocate clean kernel PML4 table */
    kernel_pml4_phys = pmm_alloc_frame();
    kernel_pml4 = (uint64_t *)PHYS_TO_VIRT(kernel_pml4_phys);
    memset(kernel_pml4, 0, PAGE_SIZE_4K);

    /* Allocate higher-half PDPT */
    paddr_t pdpt_phys = pmm_alloc_frame();
    uint64_t *pdpt = (uint64_t *)PHYS_TO_VIRT(pdpt_phys);
    memset(pdpt, 0, PAGE_SIZE_4K);

    /* Entry 511 in PML4 points to higher-half PDPT */
    kernel_pml4[511] = pdpt_phys | PTE_PRESENT | PTE_WRITABLE;

    paddr_t pd_phys = pmm_alloc_frame();
    uint64_t *pd = (uint64_t *)PHYS_TO_VIRT(pd_phys);
    memset(pd, 0, PAGE_SIZE_4K);

    /* Entry 510 in higher-half PDPT maps 0xFFFFFFFF80000000 (-2GB) */
    pdpt[510] = pd_phys | PTE_PRESENT | PTE_WRITABLE;

    /* Map first 128 MB (entries 0..63) with 2MB huge pages for Kernel image & low memory */
    for (size_t i = 0; i < 64; i++) {
        paddr_t paddr = (paddr_t)i * PAGE_SIZE_2M;
        pd[i] = paddr | PTE_PRESENT | PTE_WRITABLE | PTE_HUGE | PTE_GLOBAL;
    }

    /* Switch CR3 to clean kernel PML4 */
    vmm_switch_pml4(kernel_pml4);

    klog(KLOG_INFO, "VMM initialized: clean 4-level paging active, CR3=0x%016lx\n", kernel_pml4_phys);
}
