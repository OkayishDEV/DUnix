#ifndef _DRIVERS_SERIAL_H
#define _DRIVERS_SERIAL_H

#include <dunix/types.h>

#define COM1_PORT 0x3F8

int  serial_init(void);
bool serial_is_present(void);
int  serial_received(void);
char serial_getchar(void);
int  serial_is_transmit_empty(void);
void serial_putchar(char c);
void serial_puts(const char *str);
void serial_write(const char *data, size_t size);

#endif /* _DRIVERS_SERIAL_H */
