#ifndef _LIBC_TIME_H
#define _LIBC_TIME_H

#include <sys/types.h>
#include <sys/time.h>

#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3

struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

int        nanosleep(const struct timespec *req, struct timespec *rem);
int        clock_gettime(int clk_id, struct timespec *tp);
time_t     time(time_t *tloc);
struct tm *gmtime(const time_t *timep);
struct tm *localtime(const time_t *timep);
char      *asctime(const struct tm *tm);
char      *ctime(const time_t *timep);
size_t     strftime(char *s, size_t max, const char *format, const struct tm *tm);

#endif /* _LIBC_TIME_H */
