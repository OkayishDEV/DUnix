#include <arch/x86_64/drivers/console.h>
#include <arch/x86_64/drivers/vga.h>
#include <arch/x86_64/drivers/serial.h>
#include <arch/x86_64/drivers/keyboard.h>
#include <arch/x86_64/drivers/speaker.h>
#include <arch/x86_64/io.h>
#include <sched/sched.h>
#include <process/process.h>
#include <ipc/signal.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

/* Raw input ring buffer (fed by Keyboard & Serial ISRs) */
static volatile char raw_input_buf[CONSOLE_BUF_SIZE];
static volatile uint32_t raw_head = 0;
static volatile uint32_t raw_tail = 0;

/* Canonical mode line buffer */
static char canon_line_buf[TTY_LINE_BUF_SIZE];
static uint32_t canon_line_len = 0;

/* Line ready buffer for canonical read() delivery */
static char canon_ready_buf[TTY_LINE_BUF_SIZE];
static uint32_t canon_ready_len = 0;
static uint32_t canon_ready_pos = 0;

static struct termios_kernel console_termios;

void console_init(void) {
    raw_head = 0;
    raw_tail = 0;
    canon_line_len = 0;
    canon_ready_len = 0;
    canon_ready_pos = 0;

    memset(&console_termios, 0, sizeof(console_termios));
    console_termios.c_iflag = ICRNL;
    console_termios.c_oflag = OPOST | ONLCR;
    console_termios.c_lflag = ICANON | ECHO | ECHOE | ECHOK | ISIG;
    console_termios.c_cc[0] = 0x04; /* VEOF: Ctrl+D */
    console_termios.c_cc[1] = 0x03; /* VINTR: Ctrl+C */
    console_termios.c_cc[2] = 0x7F; /* VERASE: Backspace */
    console_termios.c_cc[3] = 0x15; /* VKILL: Ctrl+U */

    klog(KLOG_INFO, "Console & TTY subsystem initialized\n");
}

void console_putc(char c) {
    if (c == '\a') {
        speaker_beep(750, 100);
        return;
    }
    vga_putchar(c);
    if (c == '\n') {
        serial_putchar('\r');
    }
    serial_putchar(c);
}

void console_write(const char *buf, size_t size) {
    if (!buf || size == 0) return;
    for (size_t i = 0; i < size; i++) {
        console_putc(buf[i]);
    }
}

void console_push_input(char c) {
    uint32_t next = (raw_head + 1) % CONSOLE_BUF_SIZE;
    if (next != raw_tail) {
        raw_input_buf[raw_head] = c;
        raw_head = next;
    }
}

void console_push_string(const char *str) {
    if (!str) return;
    while (*str) {
        console_push_input(*str++);
    }
}

int console_has_input(void) {
    if (raw_head != raw_tail) return 1;
    if (inb(KBD_STATUS_PORT) & 1) return 1;
    if (serial_is_present() && (inb(COM1_PORT + 5) & 1)) return 1;
    return 0;
}

char console_getc(void) {
    for (;;) {
        cli();
        if (raw_head != raw_tail) {
            char c = raw_input_buf[raw_tail];
            raw_tail = (raw_tail + 1) % CONSOLE_BUF_SIZE;
            sti();
            return c;
        }

        /* Fallback poll for PS/2 keyboard in case IRQ 1 was missed or edge-triggered */
        if (inb(KBD_STATUS_PORT) & 1) {
            uint8_t scancode = inb(KBD_DATA_PORT);
            char c = keyboard_scancode_to_char(scancode);
            if (c != 0) {
                sti();
                return c;
            }
        }

        /* Fallback poll for COM1 serial port ONLY if genuinely present */
        if (serial_is_present() && (inb(COM1_PORT + 5) & 1)) {
            char c = (char)inb(COM1_PORT);
            if ((unsigned char)c != 0xFF && (unsigned char)c != 0) {
                sti();
                return c;
            }
        }

        sti();
        sched_sleep(5);
    }
}

ssize_t console_read(void *buf, size_t size) {
    if (!buf || size == 0) return 0;
    char *out = (char *)buf;

    /* If not canonical (raw mode) */
    if (!(console_termios.c_lflag & ICANON)) {
        size_t count = 0;
        /* Block for at least 1 character (standard VMIN >= 1) */
        char first = console_getc();
        out[count++] = first;
        if (console_termios.c_lflag & ECHO) {
            console_putc(first);
        }

        /* Pick up any additional already-buffered characters up to size */
        while (count < size && console_has_input()) {
            char c = console_getc();
            out[count++] = c;
            if (console_termios.c_lflag & ECHO) {
                console_putc(c);
            }
        }
        return (ssize_t)count;
    }

    /* Canonical Mode (Line Buffering) */
    for (;;) {
        /* If we have remaining data from a previously completed line */
        if (canon_ready_pos < canon_ready_len) {
            size_t available = canon_ready_len - canon_ready_pos;
            size_t to_copy = (size < available) ? size : available;
            memcpy(out, &canon_ready_buf[canon_ready_pos], to_copy);
            canon_ready_pos += to_copy;
            if (canon_ready_pos >= canon_ready_len) {
                canon_ready_pos = 0;
                canon_ready_len = 0;
            }
            return (ssize_t)to_copy;
        }

        /* Otherwise, build the canonical line until Enter or EOF */
        char c = console_getc();

        /* Discard non-character / open-bus noise */
        if ((unsigned char)c == 0xFF || (unsigned char)c == 0) {
            continue;
        }

        /* Convert CR to NL if ICRNL is enabled */
        if (c == '\r' && (console_termios.c_iflag & ICRNL)) {
            c = '\n';
        }

        /* Signal handling: Ctrl+C (VINTR = 0x03) */
        if ((console_termios.c_lflag & ISIG) && c == 0x03) {
            if (console_termios.c_lflag & ECHO) {
                console_write("^C\n", 3);
            }
            canon_line_len = 0;
            struct process *curr = process_get_current();
            if (curr && curr->pid > 1) {
                sys_kill(curr->pid, SIGINT);
            }
            continue;
        }

        /* Signal handling: Ctrl+Z (VSUSP = 0x1A) */
        if ((console_termios.c_lflag & ISIG) && c == 0x1A) {
            if (console_termios.c_lflag & ECHO) {
                console_write("^Z\n", 3);
            }
            continue;
        }

        /* EOF handling: Ctrl+D (VEOF = 0x04) */
        if (c == 0x04) {
            if (canon_line_len == 0) {
                return 0; /* EOF */
            }
            /* Deliver current line without newline */
            memcpy(canon_ready_buf, canon_line_buf, canon_line_len);
            canon_ready_len = canon_line_len;
            canon_ready_pos = 0;
            canon_line_len = 0;
            continue;
        }

        /* Kill line: Ctrl+U (VKILL = 0x15) */
        if (c == 0x15) {
            if (console_termios.c_lflag & ECHO) {
                while (canon_line_len > 0) {
                    console_write("\b \b", 3);
                    canon_line_len--;
                }
            } else {
                canon_line_len = 0;
            }
            continue;
        }

        /* Backspace / Delete */
        if (c == 0x7F || c == 0x08 || c == '\b') {
            if (canon_line_len > 0) {
                canon_line_len--;
                if (console_termios.c_lflag & ECHO) {
                    console_write("\b \b", 3);
                }
            }
            continue;
        }

        /* Enter / Newline */
        if (c == '\n') {
            if (canon_line_len < TTY_LINE_BUF_SIZE - 2) {
                canon_line_buf[canon_line_len++] = '\n';
            }
            if (console_termios.c_lflag & ECHO) {
                console_putc('\n');
            }

            /* Transfer line to ready buffer */
            memcpy(canon_ready_buf, canon_line_buf, canon_line_len);
            canon_ready_len = canon_line_len;
            canon_ready_pos = 0;
            canon_line_len = 0;
            continue;
        }

        /* Normal printable / regular characters */
        if (canon_line_len < TTY_LINE_BUF_SIZE - 2) {
            canon_line_buf[canon_line_len++] = c;
            if (console_termios.c_lflag & ECHO) {
                console_putc(c);
            }
        }
    }
}

int console_ioctl(unsigned long request, void *arg) {
    if (request == TCGETS) {
        if (!arg) return -14; /* -EFAULT */
        memcpy(arg, &console_termios, sizeof(struct termios_kernel));
        return 0;
    } else if (request == TCSETS || request == TCSETSW || request == TCSETSF) {
        if (!arg) return -14; /* -EFAULT */
        memcpy(&console_termios, arg, sizeof(struct termios_kernel));
        return 0;
    } else if (request == TIOCGWINSZ) {
        if (!arg) return -14;
        struct {
            uint16_t ws_row;
            uint16_t ws_col;
            uint16_t ws_xpixel;
            uint16_t ws_ypixel;
        } *ws = arg;
        ws->ws_row = 25;
        ws->ws_col = 80;
        ws->ws_xpixel = 640;
        ws->ws_ypixel = 400;
        return 0;
    } else if (request == TIOCSWINSZ) {
        return 0;
    } else if (request == KIOCSOUND) {
        unsigned long div = (unsigned long)(uintptr_t)arg;
        if (div == 0) {
            speaker_off();
        } else {
            speaker_tone((uint32_t)(PIT_BASE_FREQUENCY / div));
        }
        return 0;
    } else if (request == KDMKTONE) {
        unsigned long val = (unsigned long)(uintptr_t)arg;
        uint32_t ticks = (uint32_t)((val >> 16) & 0xFFFF);
        uint32_t div = (uint32_t)(val & 0xFFFF);
        uint32_t ms = ticks * 10;
        uint32_t freq = (div > 0) ? (uint32_t)(PIT_BASE_FREQUENCY / div) : 0;
        speaker_beep(freq, ms ? ms : 100);
        return 0;
    }

    return -25; /* -ENOTTY */
}
