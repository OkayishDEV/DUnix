#include <arch/x86_64/cpu/power.h>
#include <arch/x86_64/io.h>
#include <fs/block.h>
#include <dunix/kprintf.h>

void system_reboot(void) {
    klog(KLOG_INFO, "System reboot requested. Syncing storage devices...\n");
    block_device_sync_all();

    klog(KLOG_INFO, "Restarting system...\n");
    __asm__ volatile("cli");

    /* 1. Try 8042 Keyboard Controller pulse reset line (Standard x86/PC-AT) */
    uint8_t temp;
    int timeout = 100000;
    do {
        temp = inb(0x64);
        if ((temp & 0x01) != 0) {
            inb(0x60); /* flush output buffer */
        }
    } while ((temp & 0x02) != 0 && --timeout > 0);

    outb(0x64, 0xFE); /* Pulse reset line */

    /* Wait a moment for reset to take effect */
    for (int i = 0; i < 1000000; i++) {
        __asm__ volatile("pause" ::: "memory");
    }

    /* 2. Try PCI reset register (port 0xCF9: bit 2=reset, bit 1=system reset) */
    outb(0xCF9, 0x06);

    for (int i = 0; i < 1000000; i++) {
        __asm__ volatile("pause" ::: "memory");
    }

    /* 3. Fallback: Triple fault CPU by loading null IDT and issuing interrupt */
    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) null_idt = { 0, 0 };

    __asm__ volatile("lidt %0; int3" : : "m"(null_idt));

    /* 4. Infinite halt */
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void system_poweroff(void) {
    klog(KLOG_INFO, "System power off requested. Syncing storage devices...\n");
    block_device_sync_all();

    klog(KLOG_INFO, "Powering off...\n");
    __asm__ volatile("cli");

    /* 1. QEMU / Bochs older ACPI poweroff port */
    outw(0x604, 0x2000);

    /* 2. VirtualBox poweroff port */
    outw(0x4004, 0x3400);

    /* 3. QEMU debug exit port */
    outb(0x501, 0x31);

    /* 4. Cloud-Hypervisor / Firecracker poweroff port */
    outw(0x600, 0x34);

    /* 5. Fallback halt */
    klog(KLOG_INFO, "System halted. It is now safe to turn off your computer.\n");
    for (;;) {
        __asm__ volatile("hlt");
    }
}

void system_halt(void) {
    klog(KLOG_INFO, "System halt requested. Syncing storage devices...\n");
    block_device_sync_all();

    klog(KLOG_INFO, "System halted.\n");
    __asm__ volatile("cli");
    for (;;) {
        __asm__ volatile("hlt");
    }
}
