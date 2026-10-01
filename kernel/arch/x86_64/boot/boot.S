.code32

/* ==========================================================================
 * MULTIBOOT 1 HEADER
 * ========================================================================== */
.section .multiboot, "a"
.align 4
#define MB1_MAGIC    0x1BADB002
#define MB1_FLAGS    0x00000003 /* ALIGN | MEMINFO */
#define MB1_CHECKSUM -(MB1_MAGIC + MB1_FLAGS)

.long MB1_MAGIC
.long MB1_FLAGS
.long MB1_CHECKSUM

/* ==========================================================================
 * MULTIBOOT 2 HEADER
 * ========================================================================== */
.align 8
mb2_header_start:
.long 0xE85250D6                /* Multiboot 2 magic */
.long 0                         /* Architecture 0 (i386 32-bit protected) */
.long mb2_header_end - mb2_header_start /* Header length */
.long -(0xE85250D6 + 0 + (mb2_header_end - mb2_header_start)) /* Checksum */

/* Multiboot 2 End Tag */
.short 0
.short 0
.long 8
mb2_header_end:

/* ==========================================================================
 * 32-BIT EARLY BOOT ENTRY POINT
 * ========================================================================== */
.section .boot.text, "ax"
.global _start32
.type _start32, @function
_start32:
    cli

    /* Save Multiboot Magic and Info Pointer */
    movl %eax, (saved_magic)
    movl %ebx, (saved_mbi_ptr)

    /* Set up early 32-bit stack */
    movl $boot_stack_top, %esp

    /* Reset EFLAGS */
    pushl $0
    popfl

    /* Check for CPUID support */
    pushfl
    popl %eax
    movl %eax, %ecx
    xorl $(1 << 21), %eax       /* Toggle ID bit */
    pushl %eax
    popfl
    pushfl
    popl %eax
    pushl %ecx
    popfl
    cmpl %eax, %ecx
    je .no_cpuid

    /* Check for Extended CPUID Functions */
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb .no_long_mode

    /* Check for 64-bit Long Mode support */
    movl $0x80000001, %eax
    cpuid
    testl $(1 << 29), %edx      /* Bit 29: Long Mode */
    jz .no_long_mode

    /* ======================================================================
     * BUILD EARLY PAGE TABLES
     * Map first 1GB physical memory to both:
     *   1) 0x0000000000000000 - 0x000000003FFFFFFF (Identity)
     *   2) 0xFFFFFFFF80000000 - 0xFFFFFFFFFFFFFFFF (Higher Half, -2GB)
     * ====================================================================== */

    /* Zero out page table structures */
    movl $boot_pml4, %edi
    movl $4096, %ecx
    xorl %eax, %eax
    rep stosl

    /* boot_pml4[0] -> boot_pdpt_identity (Flags: Present | Writable = 0x03) */
    movl $boot_pdpt_identity, %eax
    orl  $0x03, %eax
    movl %eax, (boot_pml4)

    /* boot_pml4[511] -> boot_pdpt_higher_half (Flags: Present | Writable = 0x03) */
    movl $boot_pdpt_higher_half, %eax
    orl  $0x03, %eax
    movl %eax, (boot_pml4 + 511 * 8)

    /* boot_pdpt_identity[0] -> boot_pd (Flags: Present | Writable = 0x03) */
    movl $boot_pd, %eax
    orl  $0x03, %eax
    movl %eax, (boot_pdpt_identity)

    /* boot_pdpt_higher_half[510] -> boot_pd (maps 0xFFFFFFFF80000000) */
    movl $boot_pd, %eax
    orl  $0x03, %eax
    movl %eax, (boot_pdpt_higher_half + 510 * 8)

    /* Fill boot_pd with 512 entries of 2MB huge pages (mapping 0..1GB) */
    movl $0, %ecx               /* Entry index: 0..511 */
    movl $0x83, %eax            /* Base address 0x00000000 | Present(1) | Writable(2) | Huge(0x80) */
.fill_pd:
    movl %eax, boot_pd(, %ecx, 8)
    movl $0, boot_pd + 4(, %ecx, 8)
    addl $0x00200000, %eax      /* +2 MB */
    incl %ecx
    cmpl $512, %ecx
    jne .fill_pd

    /* Enable PAE (Physical Address Extension) in CR4 */
    movl %cr4, %eax
    orl  $(1 << 5), %eax
    movl %eax, %cr4

    /* Load CR3 with physical address of boot_pml4 */
    movl $boot_pml4, %eax
    movl %eax, %cr3

    /* Enable Long Mode (LME, bit 8), NXE (bit 11), and SYSCALL (SCE, bit 0) in EFER MSR */
    movl $0xC0000080, %ecx
    rdmsr
    orl  $((1 << 11) | (1 << 8) | (1 << 0)), %eax
    wrmsr

    /* Enable Paging (PG, bit 31) and Protection (PE, bit 0) in CR0 */
    movl %cr0, %eax
    orl  $((1 << 31) | (1 << 0)), %eax
    movl %eax, %cr0

    /* Load early 64-bit GDT */
    lgdt (early_gdt_ptr)

    /* Far jump to 64-bit submode */
    ljmp $0x08, $long_mode_entry_low

.no_cpuid:
    movl $0x4F524F45, 0xB8000   /* 'ER' in VGA red/white */
    hlt
    jmp .no_cpuid

.no_long_mode:
    movl $0x4F4E4F4C, 0xB8000   /* 'LN' in VGA red/white */
    hlt
    jmp .no_long_mode

/* ==========================================================================
 * 64-BIT MODE TRANSITION
 * ========================================================================== */
.code64
long_mode_entry_low:
    /* Reload data segment selectors */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movw %ax, %fs
    movw %ax, %gs

    /* Absolute 64-bit jump to higher-half kernel */
    movabsq $long_mode_entry_high, %rax
    jmp *%rax

.section .text
long_mode_entry_high:
    /* Set up 64-bit higher-half kernel stack */
    movabsq $kernel_stack_top, %rsp

    /* Zero out .bss section */
    movabsq $_bss_start, %rdi
    movabsq $_bss_end, %rcx
    subq %rdi, %rcx
    shrq $3, %rcx
    xorq %rax, %rax
    rep stosq

    /* Pass Multiboot arguments to kmain(uint32_t magic, uint32_t mbi_addr) */
    movl (saved_magic), %edi
    movl (saved_mbi_ptr), %esi

    /* Call 64-bit Kernel Main */
    call kmain

    /* Kernel main should never return; if it does, halt */
.halt_loop:
    cli
    hlt
    jmp .halt_loop

/* ==========================================================================
 * EARLY BOOT DATA & PAGE TABLES (Located in .boot section)
 * ========================================================================== */
.section .boot.data, "aw"
.align 8
early_gdt:
    .quad 0x0000000000000000    /* Null descriptor */
    .quad 0x00209A0000000000    /* 64-bit Kernel Code (DPL 0, L=1) */
    .quad 0x0000920000000000    /* 64-bit Kernel Data (DPL 0, W=1) */
early_gdt_end:

early_gdt_ptr:
    .short early_gdt_end - early_gdt - 1
    .long early_gdt

.section .boot.bss, "aw", @nobits
.align 4096
boot_pml4:
    .skip 4096
boot_pdpt_identity:
    .skip 4096
boot_pdpt_higher_half:
    .skip 4096
boot_pd:
    .skip 4096

saved_magic:
    .skip 4
saved_mbi_ptr:
    .skip 4

.align 16
boot_stack_bottom:
    .skip 4096
boot_stack_top:

/* ==========================================================================
 * KERNEL 64-BIT RUNTIME STACK (Located in .bss)
 * ========================================================================== */
.section .bss, "aw", @nobits
.align 16
kernel_stack_bottom:
    .skip 65536                 /* 64 KB Kernel Stack */
.global kernel_stack_top
kernel_stack_top:
