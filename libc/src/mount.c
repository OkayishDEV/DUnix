#include <sys/mount.h>
#include <errno.h>
#include <stdint.h>

#define SYS_mount  165
#define SYS_umount 166

extern int64_t __syscall(uint64_t num, ...);

int mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *data) {
    int64_t ret = __syscall(SYS_mount, (uint64_t)source, (uint64_t)target, (uint64_t)filesystemtype, (uint64_t)mountflags, (uint64_t)data);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int umount(const char *target) {
    int64_t ret = __syscall(SYS_umount, (uint64_t)target);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}
