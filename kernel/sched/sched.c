#include <sched/sched.h>
#include <sched/thread.h>
#include <mm/heap.h>
#include <mm/vmm.h>
#include <arch/x86_64/cpu/gdt.h>
#include <arch/x86_64/drivers/pit.h>
#include <arch/x86_64/io.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

extern void switch_context(uint64_t *old_rsp, uint64_t new_rsp);
extern void thread_start_trampoline(void);

static struct thread *current_thread = NULL;
static struct thread *run_queue_head = NULL;
static struct thread *run_queue_tail = NULL;
static struct thread *sleep_queue_head = NULL;
static tid_t next_tid = 1;
static bool scheduler_enabled = false;

static struct thread main_thread;

void sched_add_runnable(struct thread *t) {
    t->state = THREAD_READY;
    t->next = NULL;
    t->prev = run_queue_tail;

    if (run_queue_tail) {
        run_queue_tail->next = t;
    } else {
        run_queue_head = t;
    }
    run_queue_tail = t;
}

void sched_remove(struct thread *t) {
    if (t->prev) {
        t->prev->next = t->next;
    } else if (run_queue_head == t) {
        run_queue_head = t->next;
    }

    if (t->next) {
        t->next->prev = t->prev;
    } else if (run_queue_tail == t) {
        run_queue_tail = t->prev;
    }

    t->next = NULL;
    t->prev = NULL;
}

struct thread *sched_get_current_thread(void) {
    return current_thread;
}

struct thread *thread_create(const char *name, thread_func_t entry, void *arg) {
    struct thread *t = (struct thread *)kzalloc(sizeof(struct thread));
    if (!t) return NULL;

    t->tid = next_tid++;
    strncpy(t->name, name ? name : "unnamed", sizeof(t->name) - 1);
    t->pml4 = vmm_get_kernel_pml4();
    t->priority = 1;
    t->time_slice = DEFAULT_TIME_SLICE;

    t->kernel_stack = (uint64_t)kmalloc(THREAD_STACK_SIZE);
    if (!t->kernel_stack) {
        kfree(t);
        return NULL;
    }
    t->kernel_stack_top = t->kernel_stack + THREAD_STACK_SIZE;

    /* Setup initial stack for switch_context:
     * High address -> Low address
     * [thread_start_trampoline (ret target)]
     * [dummy rbx]
     * [dummy rbp]
     * [entry (%r12)]
     * [arg   (%r13)]
     * [dummy r14]
     * [dummy r15] <- rsp points here
     */
    uint64_t *sp = (uint64_t *)t->kernel_stack_top;

    *(--sp) = (uint64_t)thread_start_trampoline; /* Return address for switch_context ret */
    *(--sp) = 0;                        /* r15 */
    *(--sp) = 0;                        /* r14 */
    *(--sp) = (uint64_t)arg;            /* r13 */
    *(--sp) = (uint64_t)entry;          /* r12 */
    *(--sp) = 0;                        /* rbp */
    *(--sp) = 0;                        /* rbx */

    t->rsp = (uint64_t)sp;

    sched_add_runnable(t);
    return t;
}

void thread_exit(void) {
    cli();
    current_thread->state = THREAD_DEAD;
    sched_yield();

    /* Should never reach here */
    for (;;) {
        hlt();
    }
}

void sched_yield(void) {
    if (!scheduler_enabled) return;

    cli();

    struct thread *prev = current_thread;

    if (prev && prev->state == THREAD_RUNNING) {
        prev->state = THREAD_READY;
        sched_add_runnable(prev);
    }

    /* Wait for a runnable thread if none is available (Idle CPU) */
    while (!run_queue_head) {
        sti();
        hlt();
        cli();
    }

    struct thread *next = run_queue_head;
    sched_remove(next);

    next->state = THREAD_RUNNING;
    next->time_slice = DEFAULT_TIME_SLICE;
    current_thread = next;

    /* Update TSS kernel stack for interrupts */
    gdt_set_kernel_stack(next->kernel_stack_top);

    /* Switch CR3 if address space changed */
    if (prev && prev->pml4 != next->pml4) {
        vmm_switch_pml4(next->pml4);
    }

    if (prev != next) {
        /* Save current hardware FS_BASE to prev thread */
        if (prev) {
            prev->fs_base = rdmsr(0xC0000100);
        }
        /* Restore next hardware FS_BASE */
        wrmsr(0xC0000100, next->fs_base);

        /* Perform low-level context switch */
        switch_context(&prev->rsp, next->rsp);
    }

    sti();
}

void sched_sleep(uint64_t ms) {
    cli();
    uint64_t ticks = (ms * PIT_TARGET_HZ + 999) / 1000;
    if (ticks == 0 && ms > 0) ticks = 1;
    current_thread->wake_tick = pit_get_ticks() + ticks;
    current_thread->state = THREAD_SLEEPING;

    /* Add to sleep queue */
    current_thread->next = sleep_queue_head;
    current_thread->prev = NULL;
    if (sleep_queue_head) {
        sleep_queue_head->prev = current_thread;
    }
    sleep_queue_head = current_thread;

    sched_yield();
}

void sched_tick(struct interrupt_frame *frame) {
    (void)frame;
    if (!scheduler_enabled) return;

    uint64_t now = pit_get_ticks();

    /* Wake up sleeping threads */
    struct thread *curr = sleep_queue_head;
    while (curr) {
        struct thread *next = curr->next;
        if (now >= curr->wake_tick) {
            /* Remove from sleep queue */
            if (curr->prev) {
                curr->prev->next = curr->next;
            } else {
                sleep_queue_head = curr->next;
            }
            if (curr->next) {
                curr->next->prev = curr->prev;
            }
            sched_add_runnable(curr);
        }
        curr = next;
    }

    /* Check current thread time slice */
    if (current_thread && current_thread->state == THREAD_RUNNING) {
        if (--current_thread->time_slice <= 0) {
            sched_yield();
        }
    }
}

extern uint64_t kernel_stack_top;

void sched_init(void) {
    memset(&main_thread, 0, sizeof(struct thread));
    main_thread.tid = 0;
    strcpy(main_thread.name, "kinit/0");
    main_thread.state = THREAD_RUNNING;
    main_thread.pml4 = vmm_get_kernel_pml4();
    main_thread.kernel_stack_top = (uint64_t)&kernel_stack_top;
    main_thread.time_slice = DEFAULT_TIME_SLICE;
    main_thread.priority = 0;

    current_thread = &main_thread;
    scheduler_enabled = true;

    klog(KLOG_INFO, "Preemptive Round-Robin Scheduler initialized (Main TID 0 active)\n");
}
