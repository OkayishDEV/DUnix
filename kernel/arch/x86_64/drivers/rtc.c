#include <arch/x86_64/drivers/rtc.h>
#include <arch/x86_64/drivers/pit.h>
#include <arch/x86_64/io.h>
#include <dunix/kprintf.h>

#define CMOS_PORT_ADDR 0x70
#define CMOS_PORT_DATA 0x71

#define RTC_REG_SEC       0x00
#define RTC_REG_MIN       0x02
#define RTC_REG_HOUR      0x04
#define RTC_REG_DAY       0x07
#define RTC_REG_MONTH     0x08
#define RTC_REG_YEAR      0x09
#define RTC_REG_CENTURY   0x32
#define RTC_REG_STATUS_A  0x0A
#define RTC_REG_STATUS_B  0x0B

static uint64_t g_boot_epoch = 0;

static inline uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_PORT_ADDR, (reg & 0x7F));
    return inb(CMOS_PORT_DATA);
}

static inline int rtc_is_updating(void) {
    return (cmos_read(RTC_REG_STATUS_A) & 0x80);
}

static inline uint8_t bcd_to_bin(uint8_t val) {
    return ((val & 0x0F) + ((val >> 4) * 10));
}

static bool is_leap_year(uint16_t year) {
    return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

static const uint16_t days_before_month[12] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

static uint64_t datetime_to_epoch(const struct rtc_time *t) {
    uint64_t days = 0;
    for (uint16_t y = 1970; y < t->year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }
    if (t->month >= 1 && t->month <= 12) {
        days += days_before_month[t->month - 1];
        if (t->month > 2 && is_leap_year(t->year)) {
            days += 1;
        }
    }
    if (t->day >= 1) {
        days += (t->day - 1);
    }
    return ((days * 24ULL + t->hour) * 60ULL + t->minute) * 60ULL + t->second;
}

static void rtc_read_raw(struct rtc_time *t) {
    t->second  = cmos_read(RTC_REG_SEC);
    t->minute  = cmos_read(RTC_REG_MIN);
    t->hour    = cmos_read(RTC_REG_HOUR);
    t->day     = cmos_read(RTC_REG_DAY);
    t->month   = cmos_read(RTC_REG_MONTH);
    t->year    = cmos_read(RTC_REG_YEAR);
}

void rtc_read_datetime(struct rtc_time *t) {
    struct rtc_time last;

    /* Wait if update is in progress */
    int timeout = 100000;
    while (rtc_is_updating() && --timeout > 0) {
        __asm__ volatile("pause" ::: "memory");
    }

    rtc_read_raw(t);

    /* Read repeatedly until two consecutive reads match (safeguard against rollover) */
    do {
        last = *t;
        timeout = 100000;
        while (rtc_is_updating() && --timeout > 0) {
            __asm__ volatile("pause" ::: "memory");
        }
        rtc_read_raw(t);
    } while (t->second != last.second || t->minute != last.minute ||
             t->hour   != last.hour   || t->day    != last.day    ||
             t->month  != last.month  || t->year   != last.year);

    uint8_t status_b = cmos_read(RTC_REG_STATUS_B);

    /* Convert BCD to binary if BCD mode is active (bit 2 = 0) */
    if (!(status_b & 0x04)) {
        t->second = bcd_to_bin(t->second);
        t->minute = bcd_to_bin(t->minute);
        t->hour   = ((t->hour & 0x0F) + (((t->hour & 0x70) >> 4) * 10)) | (t->hour & 0x80);
        t->day    = bcd_to_bin(t->day);
        t->month  = bcd_to_bin(t->month);
        t->year   = bcd_to_bin((uint8_t)t->year);
    }

    /* Handle 12-hour mode (bit 1 = 0) */
    if (!(status_b & 0x02) && (t->hour & 0x80)) {
        t->hour = ((t->hour & 0x7F) + 12) % 24;
    }

    /* Calculate 4-digit year */
    uint8_t century = cmos_read(RTC_REG_CENTURY);
    if (century != 0 && century != 0xFF) {
        if (!(status_b & 0x04)) {
            century = bcd_to_bin(century);
        }
        t->year += century * 100;
    } else {
        /* Assume 2000s if year < 70, otherwise 1900s */
        if (t->year < 70) {
            t->year += 2000;
        } else {
            t->year += 1900;
        }
    }
}

void rtc_init(void) {
    struct rtc_time t;
    rtc_read_datetime(&t);
    g_boot_epoch = datetime_to_epoch(&t);

    klog(KLOG_INFO, "RTC initialized: %04u-%02u-%02u %02u:%02u:%02u UTC (Epoch: %lu)\n",
         t.year, t.month, t.day, t.hour, t.minute, t.second, g_boot_epoch);
}

uint64_t rtc_get_epoch(void) {
    uint64_t ticks = pit_get_ticks();
    return g_boot_epoch + (ticks / PIT_TARGET_HZ);
}
