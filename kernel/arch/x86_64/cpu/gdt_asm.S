.code64
.section .text
.global gdt_flush
.type gdt_flush, @function
gdt_flush:
    /* %rdi = pointer to gdt_ptr */
    /* %rsi = 64-bit code segment selector (0x08) */
    /* %rdx = 64-bit data segment selector (0x10) */
    lgdt (%rdi)

    movw %dx, %ds
    movw %dx, %es
    movw %dx, %fs
    movw %dx, %gs
    movw %dx, %ss

    /* Far return to reload %cs */
    pushq %rsi                  /* New CS selector */
    leaq .reload_cs(%rip), %rax /* Return target address */
    pushq %rax
    lretq

.reload_cs:
    ret

.global tss_flush
.type tss_flush, @function
tss_flush:
    /* %rdi = TSS selector (0x28) */
    ltr %di
    ret
