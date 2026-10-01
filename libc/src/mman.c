#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>

#define SYS_mmap   9
#define SYS_munmap 11

extern int64_t __syscall(uint64_t num, ...);

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    int64_t ret = __syscall(SYS_mmap, (uint64_t)addr, (uint64_t)length, (uint64_t)prot, (uint64_t)flags, (uint64_t)fd, (uint64_t)offset);
    if (ret < 0) {
        errno = (int)(-ret);
        return MAP_FAILED;
    }
    return (void *)ret;
}

int munmap(void *addr, size_t length) {
    int64_t ret = __syscall(SYS_munmap, (uint64_t)addr, (uint64_t)length);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}
