#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SYS_nanosleep     35
#define SYS_gettimeofday  96
#define SYS_clock_gettime 228

extern int64_t __syscall(uint64_t num, ...);

int nanosleep(const struct timespec *req, struct timespec *rem) {
    int64_t ret = __syscall(SYS_nanosleep, (uint64_t)req, (uint64_t)rem);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int gettimeofday(struct timeval *tv, struct timezone *tz) {
    int64_t ret = __syscall(SYS_gettimeofday, (uint64_t)tv, (uint64_t)tz);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int clock_gettime(int clk_id, struct timespec *tp) {
    int64_t ret = __syscall(SYS_clock_gettime, (uint64_t)clk_id, (uint64_t)tp);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

time_t time(time_t *tloc) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        return (time_t)(-1);
    }
    if (tloc) {
        *tloc = tv.tv_sec;
    }
    return tv.tv_sec;
}

static struct tm g_tm;
static char g_asc_buf[32];

static const char *const s_days_short[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *const s_days_long[]  = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
static const char *const s_mon_short[]  = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static const char *const s_mon_long[]   = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

static const int s_days_in_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static inline int is_leap(int year) {
    return ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
}

struct tm *gmtime(const time_t *timep) {
    time_t t = timep ? *timep : time(NULL);
    if (t < 0) t = 0;

    int64_t secs = t % 86400;
    int64_t days = t / 86400;

    g_tm.tm_sec = (int)(secs % 60);
    g_tm.tm_min = (int)((secs / 60) % 60);
    g_tm.tm_hour = (int)(secs / 3600);

    /* 1970-01-01 was Thursday (wday = 4) */
    g_tm.tm_wday = (int)((days + 4) % 7);

    int year = 1970;
    for (;;) {
        int d_in_y = is_leap(year) ? 366 : 365;
        if (days < d_in_y) break;
        days -= d_in_y;
        year++;
    }

    g_tm.tm_year = year - 1900;
    g_tm.tm_yday = (int)days;

    int mon = 0;
    for (;;) {
        int d_in_m = s_days_in_month[mon];
        if (mon == 1 && is_leap(year)) d_in_m = 29;
        if (days < d_in_m) break;
        days -= d_in_m;
        mon++;
    }

    g_tm.tm_mon = mon;
    g_tm.tm_mday = (int)(days + 1);
    g_tm.tm_isdst = 0;

    return &g_tm;
}

struct tm *localtime(const time_t *timep) {
    return gmtime(timep);
}

char *asctime(const struct tm *tm) {
    if (!tm) return NULL;
    int w = tm->tm_wday;
    if (w < 0 || w > 6) w = 0;
    int m = tm->tm_mon;
    if (m < 0 || m > 11) m = 0;

    snprintf(g_asc_buf, sizeof(g_asc_buf), "%s %s %2d %02d:%02d:%02d %04d\n",
             s_days_short[w], s_mon_short[m], tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec, tm->tm_year + 1900);
    return g_asc_buf;
}

char *ctime(const time_t *timep) {
    return asctime(localtime(timep));
}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) {
    if (!s || max == 0 || !format || !tm) return 0;

    size_t written = 0;
    const char *p = format;

    int w = tm->tm_wday;
    if (w < 0 || w > 6) w = 0;
    int m = tm->tm_mon;
    if (m < 0 || m > 11) m = 0;

    while (*p && written < max - 1) {
        if (*p != '%') {
            s[written++] = *p++;
            continue;
        }

        p++; /* Skip '%' */
        char buf[64];
        buf[0] = '\0';

        switch (*p) {
            case '%': buf[0] = '%'; buf[1] = '\0'; break;
            case 'a': strncpy(buf, s_days_short[w], sizeof(buf) - 1); break;
            case 'A': strncpy(buf, s_days_long[w], sizeof(buf) - 1); break;
            case 'b':
            case 'h': strncpy(buf, s_mon_short[m], sizeof(buf) - 1); break;
            case 'B': strncpy(buf, s_mon_long[m], sizeof(buf) - 1); break;
            case 'd': snprintf(buf, sizeof(buf), "%02d", tm->tm_mday); break;
            case 'e': snprintf(buf, sizeof(buf), "%2d", tm->tm_mday); break;
            case 'H': snprintf(buf, sizeof(buf), "%02d", tm->tm_hour); break;
            case 'I': {
                int h12 = tm->tm_hour % 12;
                if (h12 == 0) h12 = 12;
                snprintf(buf, sizeof(buf), "%02d", h12);
                break;
            }
            case 'm': snprintf(buf, sizeof(buf), "%02d", tm->tm_mon + 1); break;
            case 'M': snprintf(buf, sizeof(buf), "%02d", tm->tm_min); break;
            case 'p': strcpy(buf, (tm->tm_hour >= 12) ? "PM" : "AM"); break;
            case 'S': snprintf(buf, sizeof(buf), "%02d", tm->tm_sec); break;
            case 'w': snprintf(buf, sizeof(buf), "%d", w); break;
            case 'y': snprintf(buf, sizeof(buf), "%02d", (tm->tm_year + 1900) % 100); break;
            case 'Y': snprintf(buf, sizeof(buf), "%04d", tm->tm_year + 1900); break;
            case 'Z': strcpy(buf, "UTC"); break;
            case 'F': snprintf(buf, sizeof(buf), "%04d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday); break;
            case 'T': snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec); break;
            case 'R': snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min); break;
            default:
                buf[0] = '%';
                buf[1] = *p;
                buf[2] = '\0';
                break;
        }

        size_t blen = strlen(buf);
        for (size_t i = 0; i < blen && written < max - 1; i++) {
            s[written++] = buf[i];
        }

        if (*p) p++;
    }

    s[written] = '\0';
    return written;
}
