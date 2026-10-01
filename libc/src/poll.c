#include <poll.h>
#include <errno.h>
#include <stdint.h>

#define SYS_poll 7

extern int64_t __syscall(uint64_t num, ...);

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    int64_t ret = __syscall(SYS_poll, (uint64_t)fds, (uint64_t)nfds, (uint64_t)timeout);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (int)ret;
}
