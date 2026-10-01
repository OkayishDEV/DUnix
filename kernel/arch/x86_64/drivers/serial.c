#include <arch/x86_64/drivers/serial.h>
#include <arch/x86_64/drivers/console.h>
#include <arch/x86_64/drivers/pic.h>
#include <arch/x86_64/cpu/idt.h>
#include <arch/x86_64/io.h>
#include <sched/sched.h>

static bool serial_present = false;

void serial_irq_handler(struct interrupt_frame *frame) {
    (void)frame;
    if (!serial_present) return;
    int limit = 256;
    while ((inb(COM1_PORT + 5) & 1) && --limit > 0) {
        char c = (char)inb(COM1_PORT);
        if ((unsigned char)c != 0xFF && (unsigned char)c != 0) {
            console_push_input(c);
        }
    }
}

int serial_init(void) {
    /* 1. Test for UART scratchpad register (offset 7) */
    outb(COM1_PORT + 7, 0x55);
    if (inb(COM1_PORT + 7) != 0x55) {
        serial_present = false;
        return -1;
    }
    outb(COM1_PORT + 7, 0xAA);
    if (inb(COM1_PORT + 7) != 0xAA) {
        serial_present = false;
        return -1;
    }

    /* 2. Loopback test: Put UART into loopback mode */
    outb(COM1_PORT + 4, 0x1E); /* Loopback mode, RTS/DTR active */
    outb(COM1_PORT + 0, 0xAE); /* Send byte 0xAE */
    if (inb(COM1_PORT + 0) != 0xAE) {
        serial_present = false;
        return -1;
    }

    /* Serial port is genuinely present! Restore normal operation */
    outb(COM1_PORT + 4, 0x0F);
    outb(COM1_PORT + 1, 0x00);    /* Disable all interrupts */
    outb(COM1_PORT + 3, 0x80);    /* Enable DLAB (set baud rate divisor) */
    outb(COM1_PORT + 0, 0x03);    /* Set divisor to 3 (38400 baud) */
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);    /* 8 bits, no parity, one stop bit */
    outb(COM1_PORT + 2, 0x07);    /* Enable FIFO, clear TX/RX FIFOs, 1-byte threshold */
    outb(COM1_PORT + 4, 0x0B);    /* IRQs enabled, RTS/DSR set, OUT2 enabled */

    /* Enable Received Data Available Interrupt (IER bit 0) */
    outb(COM1_PORT + 1, 0x01);

    serial_present = true;

    /* Register IRQ 4 (vector 36) */
    register_interrupt_handler(36, serial_irq_handler);
    pic_clear_mask(4);

    return 0;
}

bool serial_is_present(void) {
    return serial_present;
}

int serial_received(void) {
    if (!serial_present) return 0;
    return (inb(COM1_PORT + 5) & 1);
}

char serial_getchar(void) {
    return console_getc();
}

int serial_is_transmit_empty(void) {
    if (!serial_present) return 1;
    return inb(COM1_PORT + 5) & 0x20;
}

void serial_putchar(char c) {
    if (!serial_present) return;
    int timeout = 10000;
    while (serial_is_transmit_empty() == 0 && --timeout > 0) {
        /* wait */
    }
    if (timeout > 0) {
        outb(COM1_PORT, (uint8_t)c);
    }
}

void serial_puts(const char *str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(*str++);
    }
}

void serial_write(const char *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(data[i]);
    }
}
