#include <signal.h>
#include <errno.h>
#include <stdint.h>

#define SYS_kill 62
extern int64_t __syscall(uint64_t num, ...);

int kill(pid_t pid, int sig) {
    int64_t ret = __syscall(SYS_kill, (uint64_t)pid, (uint64_t)sig);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

sighandler_t signal(int signum, sighandler_t handler) {
    (void)signum;
    (void)handler;
    return SIG_DFL;
}
