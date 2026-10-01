#include <arch/x86_64/drivers/mouse.h>
#include <arch/x86_64/drivers/pic.h>
#include <arch/x86_64/cpu/idt.h>
#include <arch/x86_64/io.h>
#include <sched/sched.h>
#include <dunix/kprintf.h>
#include <dunix/string.h>

#define MOUSE_BUF_SIZE 256

static uint8_t mouse_cycle = 0;
static uint8_t mouse_bytes[3];
static int mouse_x = 512;
static int mouse_y = 384;
static int mouse_max_x = 1024;
static int mouse_max_y = 768;
static uint8_t mouse_buttons = 0;
static volatile uint32_t mouse_irq_count = 0;

static uint8_t mouse_ring_buf[MOUSE_BUF_SIZE];
static volatile uint32_t mouse_ring_head = 0;
static volatile uint32_t mouse_ring_tail = 0;

static inline void mouse_wait_write(void) {
    int timeout = 100000;
    while ((inb(0x64) & 2) && --timeout > 0);
}

static inline void mouse_wait_read(void) {
    int timeout = 100000;
    while (!(inb(0x64) & 1) && --timeout > 0);
}

static void mouse_write_cmd(uint8_t cmd) {
    mouse_wait_write();
    outb(0x64, 0xD4);
    mouse_wait_write();
    outb(0x60, cmd);
}

static uint8_t mouse_read_data(void) {
    mouse_wait_read();
    return inb(0x60);
}

void mouse_set_bounds(int width, int height) {
    mouse_max_x = width;
    mouse_max_y = height;
    if (mouse_x >= width) mouse_x = width - 1;
    if (mouse_y >= height) mouse_y = height - 1;
}

void mouse_get_state(int *x, int *y, int *buttons) {
    if (x) *x = mouse_x;
    if (y) *y = mouse_y;
    if (buttons) *buttons = mouse_buttons;
}

bool mouse_has_event(void) {
    return mouse_ring_head != mouse_ring_tail;
}

void mouse_irq_handler(struct interrupt_frame *frame) {
    (void)frame;
    mouse_irq_count++;
    uint8_t status = inb(0x64);
    while (status & 0x01) {
        if (!(status & 0x20)) {
            /* Not mouse data — leave it for the keyboard handler */
            break;
        }

        uint8_t data = inb(0x60);

        switch (mouse_cycle) {
            case 0:
                /* Bit 3 of byte 0 must be 1 in standard PS/2 protocol */
                if (data & 0x08) {
                    mouse_bytes[0] = data;
                    mouse_cycle = 1;
                }
                break;
            case 1:
                mouse_bytes[1] = data;
                mouse_cycle = 2;
                break;
            case 2:
                mouse_bytes[2] = data;
                mouse_cycle = 0;

                /* Decode deltas */
                int dx = (mouse_bytes[0] & 0x10) ? (int)(mouse_bytes[1] - 256) : (int)mouse_bytes[1];
                int dy = (mouse_bytes[0] & 0x20) ? (int)(mouse_bytes[2] - 256) : (int)mouse_bytes[2];

                /* Invert dy because PS/2 Y goes upwards */
                dy = -dy;

                mouse_x += dx;
                mouse_y += dy;

                if (mouse_x < 0) mouse_x = 0;
                if (mouse_x >= mouse_max_x) mouse_x = mouse_max_x - 1;
                if (mouse_y < 0) mouse_y = 0;
                if (mouse_y >= mouse_max_y) mouse_y = mouse_max_y - 1;

                mouse_buttons = mouse_bytes[0] & 0x07;

                /* Push 3 bytes into ring buffer */
                for (int i = 0; i < 3; i++) {
                    uint32_t next = (mouse_ring_head + 1) % MOUSE_BUF_SIZE;
                    if (next != mouse_ring_tail) {
                        mouse_ring_buf[mouse_ring_head] = mouse_bytes[i];
                        mouse_ring_head = next;
                    }
                }
                break;
        }
        /* Re-read status for next iteration */
        status = inb(0x64);
    }
}

void mouse_init(void) {
    mouse_cycle = 0;
    mouse_ring_head = 0;
    mouse_ring_tail = 0;

    /* Flush any stale output bytes */
    int flush_limit = 100;
    while ((inb(0x64) & 1) && --flush_limit > 0) {
        inb(0x60);
    }

    /* 1. Enable Auxiliary Device */
    mouse_wait_write();
    outb(0x64, 0xA8);

    /* 2. Read Controller Configuration Byte */
    mouse_wait_write();
    outb(0x64, 0x20);
    mouse_wait_read();
    uint8_t config = inb(0x60);

    /* Enable IRQ 12 (bit 1), enable mouse clock (clear bit 5), preserve IRQ 1 (bit 0) and translation (bit 6) */
    config |= 0x03 | 0x40;
    config &= ~0x30;

    /* Write back configuration */
    mouse_wait_write();
    outb(0x64, 0x60);
    mouse_wait_write();
    outb(0x60, config);

    /* 3. Send Reset (0xFF) to ensure clean mouse state */
    mouse_write_cmd(0xFF);
    uint8_t r_ack = mouse_read_data();  /* Should be 0xFA */
    if (r_ack != 0xFA) {
        klog(KLOG_INFO, "PS/2 Mouse not detected (ACK: 0x%02x), mouse driver disabled\n", r_ack);
        return;
    }
    uint8_t r_bat = mouse_read_data();  /* Should be 0xAA (Self-test passed) */
    uint8_t r_id  = mouse_read_data();  /* Should be 0x00 (Mouse ID) */

    /* 4. Send Defaults (0xF6) */
    mouse_write_cmd(0xF6);
    uint8_t ack1 = mouse_read_data(); /* Read ACK 0xFA */

    /* 5. Enable Packet Streaming (0xF4) */
    mouse_write_cmd(0xF4);
    uint8_t ack2 = mouse_read_data(); /* Read ACK 0xFA */

    /* 6. Register IRQ 12 (Vector 44) in IDT and unmask on PIC */
    register_interrupt_handler(44, mouse_irq_handler);
    pic_clear_mask(2);  /* Cascade IRQ 2 */
    pic_clear_mask(12); /* Mouse IRQ 12 */

    klog(KLOG_INFO, "PS/2 Mouse driver initialized (cfg=0x%02x reset=[0x%02x,0x%02x,0x%02x] ack1=0x%02x ack2=0x%02x)\n",
         config, r_ack, r_bat, r_id, ack1, ack2);
}

ssize_t mouse_device_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node;
    (void)offset;
    if (!buffer || size == 0) return 0;

    uint8_t *out = (uint8_t *)buffer;
    size_t count = 0;

    while (count < size && mouse_ring_head != mouse_ring_tail) {
        out[count++] = mouse_ring_buf[mouse_ring_tail];
        mouse_ring_tail = (mouse_ring_tail + 1) % MOUSE_BUF_SIZE;
    }

    return (ssize_t)count;
}
