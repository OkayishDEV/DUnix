.code64
.section .text

.global switch_context
.type switch_context, @function
switch_context:
    /* %rdi = uint64_t *old_rsp (pointer to save current RSP) */
    /* %rsi = uint64_t  new_rsp (new RSP to switch to)       */

    /* Save callee-saved registers on current stack */
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbp
    pushq %rbx

    /* Save current stack pointer */
    movq %rsp, (%rdi)

    /* Switch to new stack */
    movq %rsi, %rsp

    /* Restore callee-saved registers from new stack */
    popq %rbx
    popq %rbp
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    ret

.global thread_start_trampoline
.type thread_start_trampoline, @function
thread_start_trampoline:
    sti
    movq %r13, %rdi
    call *%r12

    /* If thread entry returns, call thread_exit() */
    call thread_exit

.hang:
    cli
    hlt
    jmp .hang
