#include <sys/ioctl.h>
#include <stdarg.h>
#include <errno.h>
#include <stdint.h>

#define SYS_ioctl 16

extern int64_t __syscall(uint64_t num, ...);

int ioctl(int fd, unsigned long request, ...) {
    va_list ap;
    va_start(ap, request);
    void *arg = va_arg(ap, void *);
    va_end(ap);

    int64_t ret = __syscall(SYS_ioctl, (uint64_t)fd, (uint64_t)request, (uint64_t)arg);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (int)ret;
}
