#include <ipc/signal.h>
#include <process/process.h>
#include <sched/sched.h>
#include <dunix/kprintf.h>

int64_t sys_kill(pid_t pid, int sig) {
    if (sig < 1 || sig >= NSIG) {
        return -22; /* -EINVAL */
    }

    struct process *target = process_get_by_pid(pid);
    if (!target) {
        return -3; /* -ESRCH */
    }

    switch (sig) {
        case SIGKILL:
        case SIGTERM:
        case SIGINT:
        case SIGSEGV:
            klog(KLOG_INFO, "Process PID %d terminated by signal %d\n", pid, sig);
            target->exit_code = 128 + sig;
            target->state = PROC_ZOMBIE;
            if (target->main_thread) {
                target->main_thread->state = THREAD_DEAD;
            }
            break;
        case SIGSTOP:
            target->state = PROC_SLEEPING;
            if (target->main_thread) {
                target->main_thread->state = THREAD_BLOCKED;
            }
            break;
        case SIGCONT:
            target->state = PROC_RUNNING;
            if (target->main_thread && target->main_thread->state == THREAD_BLOCKED) {
                sched_add_runnable(target->main_thread);
            }
            break;
        default:
            break;
    }

    return 0;
}

int64_t sys_signal(int signum, sighandler_t handler) {
    (void)signum;
    (void)handler;
    return 0;
}
