#ifndef _ARCH_X86_64_IDT_H
#define _ARCH_X86_64_IDT_H

#include <dunix/types.h>

#define IDT_ENTRIES 256

/* IDT Gate descriptor in 64-bit mode (16 bytes) */
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

/* IDTR register pointer */
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* Saved register context for interrupts and exceptions */
struct interrupt_frame {
    /* Saved GP registers (pushed by isr_stub) */
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;

    /* Vector and error code (pushed by isr stub) */
    uint64_t vector;
    uint64_t error_code;

    /* Pushed automatically by CPU */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed));

typedef void (*isr_handler_t)(struct interrupt_frame *frame);

void idt_init(void);
void idt_set_gate(uint8_t vector, uint64_t isr_addr, uint16_t selector, uint8_t flags, uint8_t ist);
void register_interrupt_handler(uint8_t vector, isr_handler_t handler);
void unregister_interrupt_handler(uint8_t vector);

/* C dispatchers called from assembly stubs */
void isr_handler(struct interrupt_frame *frame);
void irq_handler(struct interrupt_frame *frame);

#endif /* _ARCH_X86_64_IDT_H */
