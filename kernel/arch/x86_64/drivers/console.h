#ifndef _ARCH_X86_64_DRIVERS_CONSOLE_H
#define _ARCH_X86_64_DRIVERS_CONSOLE_H

#include <dunix/types.h>
#include <fs/vfs.h>

#define CONSOLE_BUF_SIZE 2048
#define TTY_LINE_BUF_SIZE 1024

/* Terminal IOCTL requests */
#define TCGETS      0x5401
#define TCSETS      0x5402
#define TCSETSW     0x5403
#define TCSETSF     0x5404
#define TIOCGWINSZ  0x5413
#define TIOCSWINSZ  0x5414
#define KIOCSOUND   0x4B2F
#define KDMKTONE    0x4B30

/* termios bit flags */
#define ICANON 0000002
#define ECHO   0000010
#define ECHOE  0000020
#define ECHOK  0000040
#define ECHONL 0000100
#define ISIG   0000001
#define ICRNL  0000400
#define OPOST  0000001
#define ONLCR  0000004

struct termios_kernel {
    uint32_t c_iflag;
    uint32_t c_oflag;
    uint32_t c_cflag;
    uint32_t c_lflag;
    uint8_t  c_line;
    uint8_t  c_cc[32];
    uint32_t c_ispeed;
    uint32_t c_ospeed;
};

void console_init(void);
void console_putc(char c);
void console_write(const char *buf, size_t size);
char console_getc(void);
int console_has_input(void);
void console_push_input(char c);
void console_push_string(const char *str);

ssize_t console_read(void *buf, size_t size);
int console_ioctl(unsigned long request, void *arg);

#endif /* _ARCH_X86_64_DRIVERS_CONSOLE_H */
