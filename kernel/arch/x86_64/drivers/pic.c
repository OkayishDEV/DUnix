#include <arch/x86_64/drivers/pic.h>
#include <arch/x86_64/io.h>
#include <dunix/kprintf.h>

#define ICW1_ICW4       0x01
#define ICW1_SINGLE     0x02
#define ICW1_INTERVAL4  0x04
#define ICW1_LEVEL      0x08
#define ICW1_INIT       0x10

#define ICW4_8086       0x01
#define ICW4_AUTO       0x02
#define ICW4_BUF_SLAVE  0x08
#define ICW4_BUF_MASTER 0x0C
#define ICW4_SFNM       0x10

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_init(void) {
    /* Save masks */
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);
    (void)a1;
    (void)a2;

    /* Start initialization sequence in cascade mode */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
    io_wait();

    /* ICW2: Master PIC vector offset (0x20 = 32) */
    outb(PIC1_DATA, 0x20);
    io_wait();
    /* ICW2: Slave PIC vector offset (0x28 = 40) */
    outb(PIC2_DATA, 0x28);
    io_wait();

    /* ICW3: Master PIC tell it that there is a slave PIC at IRQ2 (0000 0100) */
    outb(PIC1_DATA, 0x04);
    io_wait();
    /* ICW3: Slave PIC tell it its cascade identity (0000 0010) */
    outb(PIC2_DATA, 0x02);
    io_wait();

    /* ICW4: Set 8086 mode */
    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    /* Enable IRQ0 (Timer) and IRQ1 (Keyboard) and IRQ2 (Cascade to Slave), mask others for now */
    outb(PIC1_DATA, 0xF8); /* 1111 1000 -> IRQ0, IRQ1, IRQ2 enabled */
    outb(PIC2_DATA, 0xFF); /* All slave IRQs masked */

    klog(KLOG_INFO, "8259 PIC remapped to vectors 32..47\n");
}

void pic_set_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) | (uint8_t)(1 << irq);
    outb(port, value);
}

void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    value = inb(port) & (uint8_t)~(1 << irq);
    outb(port, value);
}

void pic_disable(void) {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}
