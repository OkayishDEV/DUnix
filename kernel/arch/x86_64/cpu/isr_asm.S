.code64
.section .text

/* Macro for exceptions without error code */
.macro ISR_NOERRCODE num
.global isr\num
.type isr\num, @function
isr\num:
    pushq $0                /* Dummy error code */
    pushq $\num             /* Vector number */
    jmp isr_common_stub
.endm

/* Macro for exceptions with error code */
.macro ISR_ERRCODE num
.global isr\num
.type isr\num, @function
isr\num:
    pushq $\num             /* Vector number (error code already pushed by CPU) */
    jmp isr_common_stub
.endm

/* Macro for hardware IRQs */
.macro IRQ_STUB irq_num, vec_num
.global irq\irq_num
.type irq\irq_num, @function
irq\irq_num:
    pushq $0                /* Dummy error code */
    pushq $\vec_num          /* Vector number */
    jmp irq_common_stub
.endm

/* CPU Exceptions (0 - 31) */
ISR_NOERRCODE 0   /* #DE Divide by Zero */
ISR_NOERRCODE 1   /* #DB Debug */
ISR_NOERRCODE 2   /* NMI Non-Maskable Interrupt */
ISR_NOERRCODE 3   /* #BP Breakpoint */
ISR_NOERRCODE 4   /* #OF Overflow */
ISR_NOERRCODE 5   /* #BR Bound Range Exceeded */
ISR_NOERRCODE 6   /* #UD Invalid Opcode */
ISR_NOERRCODE 7   /* #NM Device Not Available (No Math Coprocessor) */
ISR_ERRCODE   8   /* #DF Double Fault */
ISR_NOERRCODE 9   /* Coprocessor Segment Overrun (reserved) */
ISR_ERRCODE   10  /* #TS Invalid TSS */
ISR_ERRCODE   11  /* #NP Segment Not Present */
ISR_ERRCODE   12  /* #SS Stack-Segment Fault */
ISR_ERRCODE   13  /* #GP General Protection Fault */
ISR_ERRCODE   14  /* #PF Page Fault */
ISR_NOERRCODE 15  /* Reserved */
ISR_NOERRCODE 16  /* #MF x87 FPU Floating-Point Error */
ISR_ERRCODE   17  /* #AC Alignment Check */
ISR_NOERRCODE 18  /* #MC Machine Check */
ISR_NOERRCODE 19  /* #XM SIMD Floating-Point Exception */
ISR_NOERRCODE 20  /* #VE Virtualization Exception */
ISR_NOERRCODE 21  /* #CP Control Protection Exception */
ISR_NOERRCODE 22  /* Reserved */
ISR_NOERRCODE 23  /* Reserved */
ISR_NOERRCODE 24  /* Reserved */
ISR_NOERRCODE 25  /* Reserved */
ISR_NOERRCODE 26  /* Reserved */
ISR_NOERRCODE 27  /* Reserved */
ISR_NOERRCODE 28  /* #HV Hypervisor Injection Exception */
ISR_NOERRCODE 29  /* #VC VMM Communication Exception */
ISR_ERRCODE   30  /* #SX Security Exception */
ISR_NOERRCODE 31  /* Reserved */

/* Hardware IRQ Stubs (32 - 47) */
IRQ_STUB 0, 32    /* IRQ 0: PIT Timer */
IRQ_STUB 1, 33    /* IRQ 1: Keyboard */
IRQ_STUB 2, 34    /* IRQ 2: Cascade (used internally by PIC) */
IRQ_STUB 3, 35    /* IRQ 3: COM2 / Serial Port 2 */
IRQ_STUB 4, 36    /* IRQ 4: COM1 / Serial Port 1 */
IRQ_STUB 5, 37    /* IRQ 5: LPT2 / Sound Card */
IRQ_STUB 6, 38    /* IRQ 6: Floppy Disk */
IRQ_STUB 7, 39    /* IRQ 7: LPT1 / Spurious Interrupt */
IRQ_STUB 8, 40    /* IRQ 8: CMOS Real Time Clock */
IRQ_STUB 9, 41    /* IRQ 9: Free / Open */
IRQ_STUB 10, 42   /* IRQ 10: Free / Open */
IRQ_STUB 11, 43   /* IRQ 11: Free / Open */
IRQ_STUB 12, 44   /* IRQ 12: PS/2 Mouse */
IRQ_STUB 13, 45   /* IRQ 13: FPU / Coprocessor */
IRQ_STUB 14, 46   /* IRQ 14: Primary ATA Hard Disk */
IRQ_STUB 15, 47   /* IRQ 15: Secondary ATA Hard Disk */

/* Syscall stub (0x80) */
.global isr128
.type isr128, @function
isr128:
    pushq $0
    pushq $128
    jmp isr_common_stub

/* Common ISR assembly handler */
isr_common_stub:
    /* Save general-purpose registers */
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax

    /* First argument to isr_handler is pointer to interrupt_frame on stack */
    movq %rsp, %rdi
    call isr_handler

    /* Restore registers */
    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    /* Remove vector and error code from stack */
    addq $16, %rsp
    iretq

/* Common IRQ assembly handler */
irq_common_stub:
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rbp
    pushq %rdi
    pushq %rsi
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax

    movq %rsp, %rdi
    call irq_handler

    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %rsi
    popq %rdi
    popq %rbp
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    addq $16, %rsp
    iretq

/* idt_load helper */
.global idt_load
.type idt_load, @function
idt_load:
    lidt (%rdi)
    ret
