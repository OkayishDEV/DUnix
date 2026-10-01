#ifndef _ARCH_X86_64_DRIVERS_KEYBOARD_H
#define _ARCH_X86_64_DRIVERS_KEYBOARD_H

#include <dunix/types.h>
#include <arch/x86_64/cpu/idt.h>

#define KBD_DATA_PORT    0x60
#define KBD_STATUS_PORT  0x64
#define KBD_COMMAND_PORT 0x64

/* PS/2 Keyboard initialization and interrupt handler */
void keyboard_init(void);
void keyboard_irq_handler(struct interrupt_frame *frame);
char keyboard_scancode_to_char(uint8_t scancode);

#endif /* _ARCH_X86_64_DRIVERS_KEYBOARD_H */
