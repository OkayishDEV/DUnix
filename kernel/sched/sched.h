#ifndef _SCHED_SCHED_H
#define _SCHED_SCHED_H

#include <sched/thread.h>
#include <arch/x86_64/cpu/idt.h>

void           sched_init(void);
struct thread *sched_get_current_thread(void);
void           sched_yield(void);
void           sched_sleep(uint64_t ms);
void           sched_add_runnable(struct thread *t);
void           sched_remove(struct thread *t);
void           sched_tick(struct interrupt_frame *frame);

#endif /* _SCHED_SCHED_H */
