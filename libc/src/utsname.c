#include <sys/utsname.h>
#include <errno.h>
#include <stdint.h>

#define SYS_uname 63

extern int64_t __syscall(uint64_t num, ...);

int uname(struct utsname *buf) {
    int64_t ret = __syscall(SYS_uname, (uint64_t)buf);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}
