#ifndef _LIBC_SYS_REBOOT_H
#define _LIBC_SYS_REBOOT_H

#define RB_AUTOBOOT     0x01234567
#define RB_HALT_SYSTEM  0xCDEF0123
#define RB_POWER_OFF    0x4321FEDC

int reboot(int cmd);

#endif /* _LIBC_SYS_REBOOT_H */
