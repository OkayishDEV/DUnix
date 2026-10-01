#include <sys/wait.h>
#include <errno.h>
#include <stdint.h>

#define SYS_wait4 61
extern int64_t __syscall(uint64_t num, ...);

pid_t waitpid(pid_t pid, int *wstatus, int options) {
    int64_t ret = __syscall(SYS_wait4, (uint64_t)pid, (uint64_t)wstatus, (uint64_t)options);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (pid_t)ret;
}

pid_t wait(int *wstatus) {
    return waitpid(-1, wstatus, 0);
}
