#ifndef _SCHED_THREAD_H
#define _SCHED_THREAD_H

#include <dunix/types.h>
#include <dunix/stdbool.h>

typedef int32_t tid_t;

typedef enum {
    THREAD_EMBRYO,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_SLEEPING,
    THREAD_DEAD
} thread_state_t;

#define THREAD_STACK_SIZE (64 * 1024ULL) /* 64 KB kernel stack per thread */
#define DEFAULT_TIME_SLICE 5             /* 5 ticks = 50 ms */

struct thread_context {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t rip;
};

struct thread {
    tid_t                 tid;
    char                  name[32];
    thread_state_t        state;
    uint64_t             *pml4;
    uint64_t              kernel_stack;
    uint64_t              kernel_stack_top;
    uint64_t              rsp;
    uint64_t              wake_tick;
    int                   time_slice;
    int                   priority;
    uint64_t              fs_base;
    void                 *process;       /* Owner process struct pointer */
    struct thread        *next;
    struct thread        *prev;
};

typedef void (*thread_func_t)(void *arg);

struct thread *thread_create(const char *name, thread_func_t entry, void *arg);
void           thread_exit(void);

#endif /* _SCHED_THREAD_H */
