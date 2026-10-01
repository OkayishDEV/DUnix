#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>

#define SYS_open  2
#define SYS_stat  4
#define SYS_fstat 5
#define SYS_mkdir 83
#define SYS_chmod 90
#define SYS_fchmod 91

extern int64_t __syscall(uint64_t num, ...);

int open(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }

    int64_t ret = __syscall(SYS_open, (uint64_t)pathname, (uint64_t)flags, (uint64_t)mode);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (int)ret;
}

int stat(const char *pathname, struct stat *statbuf) {
    int64_t ret = __syscall(SYS_stat, (uint64_t)pathname, (uint64_t)statbuf);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int fstat(int fd, struct stat *statbuf) {
    int64_t ret = __syscall(SYS_fstat, (uint64_t)fd, (uint64_t)statbuf);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int mkdir(const char *pathname, mode_t mode) {
    int64_t ret = __syscall(SYS_mkdir, (uint64_t)pathname, (uint64_t)mode);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int chmod(const char *pathname, mode_t mode) {
    int64_t ret = __syscall(SYS_chmod, (uint64_t)pathname, (uint64_t)mode);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int fchmod(int fd, mode_t mode) {
    int64_t ret = __syscall(SYS_fchmod, (uint64_t)fd, (uint64_t)mode);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}
