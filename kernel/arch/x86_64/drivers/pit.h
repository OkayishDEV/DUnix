#ifndef _DRIVERS_PIT_H
#define _DRIVERS_PIT_H

#include <dunix/types.h>

#define PIT_BASE_FREQUENCY 1193182
#define PIT_TARGET_HZ      100

void     pit_init(uint32_t frequency);
uint64_t pit_get_ticks(void);
uint64_t pit_get_uptime_ms(void);
void     pit_sleep_ticks(uint64_t ticks);

#endif /* _DRIVERS_PIT_H */
