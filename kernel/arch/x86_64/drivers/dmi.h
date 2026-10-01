#ifndef _ARCH_X86_64_DRIVERS_DMI_H
#define _ARCH_X86_64_DRIVERS_DMI_H

#include <dunix/types.h>

struct dmi_system_info {
    char sys_vendor[64];
    char product_name[64];
    char product_version[64];
    char product_serial[64];
    char bios_vendor[64];
    char bios_version[64];
    char bios_date[32];
    char board_vendor[64];
    char board_name[64];
    char chassis_type[32];
};

void dmi_init(void);
const struct dmi_system_info *dmi_get_info(void);

#endif /* _ARCH_X86_64_DRIVERS_DMI_H */
