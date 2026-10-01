#include <arch/x86_64/cpu/idt.h>
#include <arch/x86_64/cpu/gdt.h>
#include <arch/x86_64/drivers/pic.h>
#include <arch/x86_64/io.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

extern void idt_load(struct idt_ptr *ptr);

/* ISR Stub Declarations from isr_asm.S */
extern void isr0(void);
extern void isr1(void);
extern void isr2(void);
extern void isr3(void);
extern void isr4(void);
extern void isr5(void);
extern void isr6(void);
extern void isr7(void);
extern void isr8(void);
extern void isr9(void);
extern void isr10(void);
extern void isr11(void);
extern void isr12(void);
extern void isr13(void);
extern void isr14(void);
extern void isr15(void);
extern void isr16(void);
extern void isr17(void);
extern void isr18(void);
extern void isr19(void);
extern void isr20(void);
extern void isr21(void);
extern void isr22(void);
extern void isr23(void);
extern void isr24(void);
extern void isr25(void);
extern void isr26(void);
extern void isr27(void);
extern void isr28(void);
extern void isr29(void);
extern void isr30(void);
extern void isr31(void);

extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

extern void isr128(void);

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idt_descriptor;
static isr_handler_t interrupt_handlers[IDT_ENTRIES];

static const char *exception_messages[] = {
    "Divide Error (#DE)",
    "Debug (#DB)",
    "Non-Maskable Interrupt (NMI)",
    "Breakpoint (#BP)",
    "Overflow (#OF)",
    "Bound Range Exceeded (#BR)",
    "Invalid Opcode (#UD)",
    "Device Not Available (#NM)",
    "Double Fault (#DF)",
    "Coprocessor Segment Overrun",
    "Invalid TSS (#TS)",
    "Segment Not Present (#NP)",
    "Stack-Segment Fault (#SS)",
    "General Protection Fault (#GP)",
    "Page Fault (#PF)",
    "Reserved",
    "x87 FPU Floating-Point Error (#MF)",
    "Alignment Check (#AC)",
    "Machine Check (#MC)",
    "SIMD Floating-Point Exception (#XM)",
    "Virtualization Exception (#VE)",
    "Control Protection Exception (#CP)",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception (#HV)",
    "VMM Communication Exception (#VC)",
    "Security Exception (#SX)",
    "Reserved"
};

void idt_set_gate(uint8_t vector, uint64_t isr_addr, uint16_t selector, uint8_t flags, uint8_t ist) {
    idt[vector].offset_low  = (uint16_t)(isr_addr & 0xFFFF);
    idt[vector].selector    = selector;
    idt[vector].ist         = ist & 0x07;
    idt[vector].type_attr   = flags;
    idt[vector].offset_mid  = (uint16_t)((isr_addr >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)((isr_addr >> 32) & 0xFFFFFFFF);
    idt[vector].reserved    = 0;
}

void register_interrupt_handler(uint8_t vector, isr_handler_t handler) {
    interrupt_handlers[vector] = handler;
}

void unregister_interrupt_handler(uint8_t vector) {
    interrupt_handlers[vector] = NULL;
}

void idt_init(void) {
    memset(&idt, 0, sizeof(idt));
    memset(interrupt_handlers, 0, sizeof(interrupt_handlers));

    /* Populate CPU Exceptions (0..31) */
    void (*exceptions[])(void) = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };

    for (int i = 0; i < 32; i++) {
        /* Present, Ring 0, 64-bit Interrupt Gate (0x8E) */
        idt_set_gate((uint8_t)i, (uint64_t)exceptions[i], GDT_KERNEL_CODE_SEL, 0x8E, 0);
    }

    /* Populate Hardware IRQs (32..47) */
    void (*irqs[])(void) = {
        irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7,
        irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15
    };

    for (int i = 0; i < 16; i++) {
        idt_set_gate((uint8_t)(32 + i), (uint64_t)irqs[i], GDT_KERNEL_CODE_SEL, 0x8E, 0);
    }

    /* Syscall gate (0x80) - Present, Ring 3 (0xEE) */
    idt_set_gate(128, (uint64_t)isr128, GDT_KERNEL_CODE_SEL, 0xEE, 0);

    idt_descriptor.limit = sizeof(idt) - 1;
    idt_descriptor.base  = (uint64_t)&idt;

    idt_load(&idt_descriptor);

    klog(KLOG_INFO, "IDT initialized with 256 descriptors\n");
}

void isr_handler(struct interrupt_frame *frame) {
    if (frame->vector < IDT_ENTRIES && interrupt_handlers[frame->vector]) {
        interrupt_handlers[frame->vector](frame);
        return;
    }

    /* Unhandled Exception: Dump registers and panic */
    cli();

    const char *desc = (frame->vector < 32) ? exception_messages[frame->vector] : "Unhandled Interrupt";

    kpanic("UNHANDLED CPU EXCEPTION\n"
           " Vector:   0x%02lx (%s)\n"
           " Error:    0x%016lx\n"
           " RIP:      0x%016lx  CS:     0x%04lx\n"
           " RFLAGS:   0x%016lx  RSP:    0x%016lx  SS: 0x%04lx\n"
           " RAX:      0x%016lx  RBX:    0x%016lx\n"
           " RCX:      0x%016lx  RDX:    0x%016lx\n"
           " RSI:      0x%016lx  RDI:    0x%016lx\n"
           " RBP:      0x%016lx  R8:     0x%016lx\n"
           " R9:       0x%016lx  R10:    0x%016lx\n"
           " R11:      0x%016lx  R12:    0x%016lx\n"
           " R13:      0x%016lx  R14:    0x%016lx\n"
           " R15:      0x%016lx  CR2:    0x%016lx\n",
           frame->vector, desc,
           frame->error_code,
           frame->rip, frame->cs,
           frame->rflags, frame->rsp, frame->ss,
           frame->rax, frame->rbx,
           frame->rcx, frame->rdx,
           frame->rsi, frame->rdi,
           frame->rbp, frame->r8,
           frame->r9, frame->r10,
           frame->r11, frame->r12,
           frame->r13, frame->r14,
           frame->r15, read_cr2());
}

void irq_handler(struct interrupt_frame *frame) {
    /* Send End of Interrupt (EOI) to PIC before handler dispatch */
    pic_send_eoi((uint8_t)(frame->vector - 32));

    if (frame->vector < IDT_ENTRIES && interrupt_handlers[frame->vector]) {
        interrupt_handlers[frame->vector](frame);
    }
}
