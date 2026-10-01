#ifndef _ARCH_X86_64_CPU_POWER_H
#define _ARCH_X86_64_CPU_POWER_H

#define LINUX_REBOOT_MAGIC1         0xFEE1DEAD
#define LINUX_REBOOT_MAGIC2         672274793

#define LINUX_REBOOT_CMD_RESTART    0x01234567
#define LINUX_REBOOT_CMD_HALT       0xCDEF0123
#define LINUX_REBOOT_CMD_POWER_OFF  0x4321FEDC
#define LINUX_REBOOT_CMD_RESTART2   0xA1B2C3D4

void system_reboot(void) __attribute__((noreturn));
void system_poweroff(void) __attribute__((noreturn));
void system_halt(void) __attribute__((noreturn));

#endif /* _ARCH_X86_64_CPU_POWER_H */
