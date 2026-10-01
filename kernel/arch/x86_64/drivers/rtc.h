#ifndef _ARCH_X86_64_DRIVERS_RTC_H
#define _ARCH_X86_64_DRIVERS_RTC_H

#include <dunix/types.h>
#include <dunix/stdbool.h>

struct rtc_time {
    uint8_t  second;
    uint8_t  minute;
    uint8_t  hour;
    uint8_t  day;
    uint8_t  month;
    uint16_t year;
};

void     rtc_init(void);
void     rtc_read_datetime(struct rtc_time *t);
uint64_t rtc_get_epoch(void);

#endif /* _ARCH_X86_64_DRIVERS_RTC_H */
