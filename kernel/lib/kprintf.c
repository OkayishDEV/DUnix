#include <dunix/kprintf.h>
#include <dunix/string.h>
#include <arch/x86_64/drivers/vga.h>
#include <arch/x86_64/drivers/serial.h>
#include <arch/x86_64/io.h>

static void console_putchar(char c) {
    vga_putchar(c);
    if (c == '\n') {
        serial_putchar('\r');
    }
    serial_putchar(c);
}

static void console_puts(const char *s) {
    while (*s) {
        console_putchar(*s++);
    }
}

static int format_uint(char *buf, size_t size, uint64_t val, int base, int uppercase, int min_width, char pad_char) {
    char tmp[65];
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;

    if (val == 0) {
        tmp[i++] = '0';
    } else {
        while (val > 0) {
            tmp[i++] = digits[val % (unsigned int)base];
            val /= (unsigned int)base;
        }
    }

    int len = i;
    int pad_len = (min_width > len) ? (min_width - len) : 0;
    int written = 0;

    for (int p = 0; p < pad_len; p++) {
        if ((size_t)written + 1 < size && buf) {
            buf[written] = pad_char;
        }
        written++;
    }

    for (int j = i - 1; j >= 0; j--) {
        if ((size_t)written + 1 < size && buf) {
            buf[written] = tmp[j];
        }
        written++;
    }

    return written;
}

static int format_int(char *buf, size_t size, int64_t val, int min_width, char pad_char) {
    int written = 0;
    uint64_t uval;

    if (val < 0) {
        if ((size_t)written + 1 < size && buf) {
            buf[written] = '-';
        }
        written++;
        uval = (uint64_t)(-val);
        if (min_width > 0) {
            min_width--;
        }
    } else {
        uval = (uint64_t)val;
    }

    written += format_uint(buf ? buf + written : NULL,
                           (buf && (size_t)written < size) ? (size - written) : 0,
                           uval, 10, 0, min_width, pad_char);
    return written;
}

int kvsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    size_t written = 0;

    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            if (written + 1 < size && buf) {
                buf[written] = *p;
            }
            written++;
            continue;
        }

        p++; /* Skip '%' */
        if (*p == '\0') {
            break;
        }

        /* Check for literal %% */
        if (*p == '%') {
            if (written + 1 < size && buf) {
                buf[written] = '%';
            }
            written++;
            continue;
        }

        /* Flags */
        char pad_char = ' ';
        int left_align = 0;
        for (;;) {
            if (*p == '0') {
                pad_char = '0';
                p++;
            } else if (*p == '-') {
                left_align = 1;
                p++;
            } else {
                break;
            }
        }

        /* Width */
        int min_width = 0;
        while (*p >= '0' && *p <= '9') {
            min_width = min_width * 10 + (*p - '0');
            p++;
        }

        /* Length modifier */
        int is_long = 0;
        int is_long_long = 0;
        if (*p == 'l') {
            is_long = 1;
            p++;
            if (*p == 'l') {
                is_long_long = 1;
                p++;
            }
        } else if (*p == 'z') {
            is_long = 1;
            p++;
        }

        /* Specifier */
        switch (*p) {
            case 'd':
            case 'i': {
                int64_t val;
                if (is_long_long || is_long) {
                    val = va_arg(ap, int64_t);
                } else {
                    val = va_arg(ap, int32_t);
                }
                char num_buf[64];
                int nlen = format_int(num_buf, sizeof(num_buf), val, min_width, pad_char);
                for (int i = 0; i < nlen; i++) {
                    if (written + 1 < size && buf) {
                        buf[written] = num_buf[i];
                    }
                    written++;
                }
                break;
            }
            case 'u': {
                uint64_t val;
                if (is_long_long || is_long) {
                    val = va_arg(ap, uint64_t);
                } else {
                    val = va_arg(ap, uint32_t);
                }
                char num_buf[64];
                int nlen = format_uint(num_buf, sizeof(num_buf), val, 10, 0, min_width, pad_char);
                for (int i = 0; i < nlen; i++) {
                    if (written + 1 < size && buf) {
                        buf[written] = num_buf[i];
                    }
                    written++;
                }
                break;
            }
            case 'x':
            case 'X': {
                uint64_t val;
                if (is_long_long || is_long) {
                    val = va_arg(ap, uint64_t);
                } else {
                    val = va_arg(ap, uint32_t);
                }
                char num_buf[64];
                int nlen = format_uint(num_buf, sizeof(num_buf), val, 16, (*p == 'X'), min_width, pad_char);
                for (int i = 0; i < nlen; i++) {
                    if (written + 1 < size && buf) {
                        buf[written] = num_buf[i];
                    }
                    written++;
                }
                break;
            }
            case 'p': {
                uintptr_t val = (uintptr_t)va_arg(ap, void *);
                if (written + 1 < size && buf) buf[written] = '0';
                written++;
                if (written + 1 < size && buf) buf[written] = 'x';
                written++;

                char num_buf[64];
                int nlen = format_uint(num_buf, sizeof(num_buf), val, 16, 0, 16, '0');
                for (int i = 0; i < nlen; i++) {
                    if (written + 1 < size && buf) {
                        buf[written] = num_buf[i];
                    }
                    written++;
                }
                break;
            }
            case 'b': {
                uint64_t val = (is_long || is_long_long) ? va_arg(ap, uint64_t) : va_arg(ap, uint32_t);
                char num_buf[65];
                int nlen = format_uint(num_buf, sizeof(num_buf), val, 2, 0, min_width, pad_char);
                for (int i = 0; i < nlen; i++) {
                    if (written + 1 < size && buf) {
                        buf[written] = num_buf[i];
                    }
                    written++;
                }
                break;
            }
            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) {
                    s = "(null)";
                }
                size_t slen = strlen(s);
                int pad = (min_width > (int)slen) ? (min_width - (int)slen) : 0;
                if (!left_align) {
                    while (pad-- > 0) {
                        if (written + 1 < size && buf) {
                            buf[written] = ' ';
                        }
                        written++;
                    }
                }
                while (*s) {
                    if (written + 1 < size && buf) {
                        buf[written] = *s;
                    }
                    written++;
                    s++;
                }
                if (left_align) {
                    while (pad-- > 0) {
                        if (written + 1 < size && buf) {
                            buf[written] = ' ';
                        }
                        written++;
                    }
                }
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                if (written + 1 < size && buf) {
                    buf[written] = c;
                }
                written++;
                break;
            }
            default:
                if (written + 1 < size && buf) {
                    buf[written] = *p;
                }
                written++;
                break;
        }
    }

    if (buf && size > 0) {
        if (written < size) {
            buf[written] = '\0';
        } else {
            buf[size - 1] = '\0';
        }
    }

    return (int)written;
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = kvsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}

int kvprintf(const char *fmt, va_list ap) {
    char buf[1024];
    int len = kvsnprintf(buf, sizeof(buf), fmt, ap);
    console_puts(buf);
    return len;
}

int kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = kvprintf(fmt, ap);
    va_end(ap);
    return ret;
}

void klog(int level, const char *fmt, ...) {
    const char *prefix = "[INFO ] ";
    enum vga_color color = VGA_COLOR_LIGHT_GREY;

    switch (level) {
        case KLOG_DEBUG:
            prefix = "[DEBUG] ";
            color = VGA_COLOR_DARK_GREY;
            break;
        case KLOG_INFO:
            prefix = "[INFO ] ";
            color = VGA_COLOR_LIGHT_GREEN;
            break;
        case KLOG_WARN:
            prefix = "[WARN ] ";
            color = VGA_COLOR_YELLOW;
            break;
        case KLOG_ERROR:
            prefix = "[ERROR] ";
            color = VGA_COLOR_LIGHT_RED;
            break;
        case KLOG_PANIC:
            prefix = "[PANIC] ";
            color = VGA_COLOR_WHITE;
            break;
    }

    vga_set_color(color, VGA_COLOR_BLACK);
    console_puts(prefix);
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}

void kpanic(const char *fmt, ...) {
    cli();

    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    kprintf("\n=======================================================\n");
    kprintf("                 *** KERNEL PANIC ***                  \n");
    kprintf("=======================================================\n");
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);

    kprintf("DUnix Kernel Panic: ");
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    kprintf("\n\nSystem halted. Please reboot the machine.\n");

    for (;;) {
        hlt();
    }
}
