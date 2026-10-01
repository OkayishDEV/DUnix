#include <arch/x86_64/cpu/gdt.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

extern void gdt_flush(struct gdt_ptr *gdt_ptr_addr, uint16_t code_sel, uint16_t data_sel);
extern void tss_flush(uint16_t tss_sel);

static struct {
    struct gdt_entry entries[5];
    struct tss_entry_64 tss;
} __attribute__((packed)) gdt_table;

static struct tss_64 tss;
static struct gdt_ptr gdt_descriptor;

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_table.entries[num].base_low    = (uint16_t)(base & 0xFFFF);
    gdt_table.entries[num].base_middle = (uint8_t)((base >> 16) & 0xFF);
    gdt_table.entries[num].base_high   = (uint8_t)((base >> 24) & 0xFF);
    gdt_table.entries[num].limit_low   = (uint16_t)(limit & 0xFFFF);
    gdt_table.entries[num].granularity = (uint8_t)((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt_table.entries[num].access      = access;
}

static void gdt_set_tss(uint64_t base, uint32_t limit) {
    gdt_table.tss.length       = (uint16_t)(limit & 0xFFFF);
    gdt_table.tss.base_low     = (uint16_t)(base & 0xFFFF);
    gdt_table.tss.base_mid     = (uint8_t)((base >> 16) & 0xFF);
    gdt_table.tss.flags1       = 0x89; /* Present, Ring 0, 64-bit TSS (Available) */
    gdt_table.tss.flags2       = (uint8_t)((limit >> 16) & 0x0F);
    gdt_table.tss.base_high    = (uint8_t)((base >> 24) & 0xFF);
    gdt_table.tss.base_upper32 = (uint32_t)(base >> 32);
    gdt_table.tss.reserved     = 0;
}

void gdt_init(void) {
    memset(&gdt_table, 0, sizeof(gdt_table));
    memset(&tss, 0, sizeof(tss));

    tss.iomap_base = sizeof(struct tss_64);

    /* 0x00: Null descriptor */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* 0x08: Kernel Code Segment (64-bit, Ring 0) */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xA0);

    /* 0x10: Kernel Data Segment (Ring 0) */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xC0);

    /* 0x18: User Data Segment (Ring 3) */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xF2, 0xC0);

    /* 0x20: User Code Segment (64-bit, Ring 3) */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xFA, 0xA0);

    /* 0x28: TSS Segment (16 bytes) */
    gdt_set_tss((uint64_t)&tss, sizeof(struct tss_64) - 1);

    gdt_descriptor.limit = sizeof(gdt_table) - 1;
    gdt_descriptor.base  = (uint64_t)&gdt_table;

    gdt_flush(&gdt_descriptor, GDT_KERNEL_CODE_SEL, GDT_KERNEL_DATA_SEL);
    tss_flush(GDT_TSS_SEL);

    klog(KLOG_INFO, "GDT and TSS initialized successfully\n");
}

uint64_t current_kernel_rsp = 0;

void gdt_set_kernel_stack(uint64_t stack) {
    tss.rsp0 = stack;
    current_kernel_rsp = stack;
}

uint64_t gdt_get_kernel_stack(void) {
    return tss.rsp0;
}
