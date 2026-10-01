#include <arch/x86_64/drivers/keyboard.h>
#include <arch/x86_64/drivers/console.h>
#include <arch/x86_64/drivers/pic.h>
#include <arch/x86_64/cpu/idt.h>
#include <arch/x86_64/io.h>
#include <dunix/kprintf.h>

static bool shift_pressed = false;
static bool ctrl_pressed  = false;
static bool alt_pressed   = false;
static bool caps_locked   = false;
static bool e0_prefix     = false;

/* US QWERTY Scancode Set 1 standard mapping */
static const char kbd_us_normal[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

static const char kbd_us_shifted[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,   ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

char keyboard_scancode_to_char(uint8_t scancode) {
    if (scancode == 0xE0) {
        e0_prefix = true;
        return 0;
    }

    /* Key release (break code) */
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) {
            shift_pressed = false;
        } else if (released == 0x1D) {
            ctrl_pressed = false;
        } else if (released == 0x38) {
            alt_pressed = false;
        }
        e0_prefix = false;
        return 0;
    }

    /* Extended scancodes (arrows, navigation) */
    if (e0_prefix) {
        e0_prefix = false;
        switch (scancode) {
            case 0x48: console_push_string("\033[A"); return 0; /* Up Arrow */
            case 0x50: console_push_string("\033[B"); return 0; /* Down Arrow */
            case 0x4D: console_push_string("\033[C"); return 0; /* Right Arrow */
            case 0x4B: console_push_string("\033[D"); return 0; /* Left Arrow */
            case 0x47: console_push_string("\033[H"); return 0; /* Home */
            case 0x4F: console_push_string("\033[F"); return 0; /* End */
            case 0x53: return 0x7F; /* Delete */
            case 0x1D: ctrl_pressed = true; return 0; /* Right Ctrl */
            case 0x38: alt_pressed = true; return 0;  /* Right Alt */
            default: return 0;
        }
    }

    /* Modifiers */
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
        return 0;
    }
    if (scancode == 0x1D) {
        ctrl_pressed = true;
        return 0;
    }
    if (scancode == 0x38) {
        alt_pressed = true;
        return 0;
    }
    if (scancode == 0x3A) {
        caps_locked = !caps_locked;
        return 0;
    }

    char c = 0;
    bool use_shift = shift_pressed;

    /* Handle alphabetic characters with Caps Lock */
    if (scancode < 128) {
        char norm = kbd_us_normal[scancode];
        if (norm >= 'a' && norm <= 'z') {
            if (caps_locked) use_shift = !use_shift;
            c = use_shift ? kbd_us_shifted[scancode] : norm;
        } else {
            c = use_shift ? kbd_us_shifted[scancode] : norm;
        }
    }

    /* Handle Ctrl combinations: Ctrl+A=1 .. Ctrl+Z=26 */
    if (ctrl_pressed && c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 1);
    } else if (ctrl_pressed && c >= 'A' && c <= 'Z') {
        c = (char)(c - 'A' + 1);
    }

    return c;
}

void keyboard_irq_handler(struct interrupt_frame *frame) {
    (void)frame;
    int limit = 64;
    while (--limit > 0) {
        uint8_t status = inb(KBD_STATUS_PORT);
        if (!(status & 0x01)) {
            break;
        }
        if (status & 0x20) {
            /* Bit 5 set indicates mouse data in port 0x60; leave for mouse ISR */
            break;
        }
        uint8_t scancode = inb(KBD_DATA_PORT);
        char c = keyboard_scancode_to_char(scancode);
        if (c != 0) {
            console_push_input(c);
        }
    }
}

void keyboard_init(void) {
    /* 1. Flush any stale bytes */
    int flush_limit = 100;
    while ((inb(KBD_STATUS_PORT) & 1) && --flush_limit > 0) {
        inb(KBD_DATA_PORT);
    }

    /* 2. Read Controller Configuration Byte */
    outb(KBD_COMMAND_PORT, 0x20);
    int timeout = 10000;
    while (!(inb(KBD_STATUS_PORT) & 1) && --timeout > 0);
    if (timeout > 0) {
        uint8_t config = inb(KBD_DATA_PORT);
        /* Enable IRQ 1 (bit 0), enable translation (bit 6), enable clock (bit 4 = 0) */
        config |= 0x01 | 0x40;
        config &= ~0x10;

        /* Write back configuration */
        outb(KBD_COMMAND_PORT, 0x60);
        timeout = 10000;
        while ((inb(KBD_STATUS_PORT) & 2) && --timeout > 0);
        outb(KBD_DATA_PORT, config);
    }

    /* 3. Enable First PS/2 Port */
    outb(KBD_COMMAND_PORT, 0xAE);

    /* 4. Flush again */
    flush_limit = 100;
    while ((inb(KBD_STATUS_PORT) & 1) && --flush_limit > 0) {
        inb(KBD_DATA_PORT);
    }

    /* 5. Register IRQ 1 (vector 33 in IDT) and clear PIC mask */
    register_interrupt_handler(33, keyboard_irq_handler);
    pic_clear_mask(1);

    klog(KLOG_INFO, "PS/2 Keyboard driver initialized (IRQ 1 / Vector 33 active)\n");
}
