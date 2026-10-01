#ifndef _ARCH_X86_64_GDT_H
#define _ARCH_X86_64_GDT_H

#include <dunix/types.h>

#define GDT_KERNEL_CODE_SEL 0x08
#define GDT_KERNEL_DATA_SEL 0x10
#define GDT_USER_DATA_SEL   0x18
#define GDT_USER_CODE_SEL   0x20
#define GDT_TSS_SEL         0x28

/* Segment descriptor (8 bytes) */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

/* TSS Descriptor in 64-bit mode (16 bytes) */
struct tss_entry_64 {
    uint16_t length;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  flags1;
    uint8_t  flags2;
    uint8_t  base_high;
    uint32_t base_upper32;
    uint32_t reserved;
} __attribute__((packed));

/* x86_64 TSS Structure */
struct tss_64 {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
} __attribute__((packed));

/* GDTR register pointer */
struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

extern uint64_t current_kernel_rsp;

void     gdt_init(void);
void     gdt_set_kernel_stack(uint64_t stack);
uint64_t gdt_get_kernel_stack(void);

#endif /* _ARCH_X86_64_GDT_H */
