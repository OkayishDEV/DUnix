#ifndef _IPC_SIGNAL_H
#define _IPC_SIGNAL_H

#include <dunix/types.h>

#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGTRAP   5
#define SIGABRT   6
#define SIGFPE    8
#define SIGKILL   9
#define SIGSEGV   11
#define SIGPIPE   13
#define SIGALRM   14
#define SIGTERM   15
#define SIGCHLD   17
#define SIGCONT   18
#define SIGSTOP   19

#define NSIG      32

typedef void (*sighandler_t)(int);

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)

int64_t sys_kill(pid_t pid, int sig);
int64_t sys_signal(int signum, sighandler_t handler);

#endif /* _IPC_SIGNAL_H */
