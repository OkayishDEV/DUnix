#ifndef _LIBC_SYS_WAIT_H
#define _LIBC_SYS_WAIT_H

#include <sys/types.h>

#define WNOHANG   1
#define WUNTRACED 2

#define WIFEXITED(status)   (((status) & 0x7f) == 0)
#define WEXITSTATUS(status) (((status) & 0xff00) >> 8)
#define WIFSIGNALED(status) (((status) & 0x7f) != 0)
#define WTERMSIG(status)    ((status) & 0x7f)

pid_t wait(int *wstatus);
pid_t waitpid(pid_t pid, int *wstatus, int options);

#endif /* _LIBC_SYS_WAIT_H */
