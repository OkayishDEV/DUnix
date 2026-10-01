.section .data
.align 8
user_rsp_scratch:
    .quad 0

.section .text

.extern current_kernel_rsp
.extern syscall_dispatch

.global syscall_entry_asm
.type syscall_entry_asm, @function
syscall_entry_asm:
    /* When 'syscall' instruction is executed:
     *   %rcx = user RIP
     *   %r11 = user RFLAGS
     *   %rax = syscall number
     *   Arguments: %rdi, %rsi, %rdx, %r10, %r8, %r9
     */

    /* Save user RSP temporarily and switch to Kernel Stack without clobbering user GP registers */
    movq %rsp, user_rsp_scratch(%rip)
    movq current_kernel_rsp(%rip), %rsp

    /* Construct trap frame on kernel stack (standard iretq frame) */
    pushq $0x1B                 /* User SS (0x18 | 3) */
    pushq user_rsp_scratch(%rip)/* User RSP */
    pushq %r11                  /* User RFLAGS */
    pushq $0x23                 /* User CS (0x20 | 3) */
    pushq %rcx                  /* User RIP */

    /* Save general purpose registers matching struct syscall_frame */
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %rbp
    pushq %rbx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %r8
    pushq %r9
    pushq %r10

    /* Prepare C arguments:
     *   rdi = syscall number (%rax)
     *   rsi = arg1 (%rdi)
     *   rdx = arg2 (%rsi)
     *   rcx = arg3 (%rdx)
     *   r8  = arg4 (%r10)
     *   r9  = arg5 (%r8)
     *   7th arg on stack: pointer to syscall_frame (%rsp)
     */
    movq %rax, %r15             /* Save syscall num */
    movq %rsp, %rax             /* Pointer to syscall_frame */
    pushq %rax                  /* 7th parameter on stack for C ABI */

    movq %r8,  %r9              /* arg5 = old r8 */
    movq %r10, %r8              /* arg4 = old r10 */
    movq %rdx, %rcx             /* arg3 = old rdx */
    movq %rsi, %rdx             /* arg2 = old rsi */
    movq %rdi, %rsi             /* arg1 = old rdi */
    movq %r15, %rdi             /* num  = old rax */

    call syscall_dispatch

    addq $8, %rsp               /* Clean 7th parameter */

    /* Return value is in %rax */

    /* Restore registers */
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rbx
    popq %rbp
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    /* Return to Ring 3 via atomic iretq (pops RIP, CS, RFLAGS, RSP, SS) */
    iretq

.global fork_child_trampoline
.type fork_child_trampoline, @function
fork_child_trampoline:
    /* Restore registers from child's copied syscall_frame */
    popq %r10
    popq %r9
    popq %r8
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rbx
    popq %rbp
    popq %r12
    popq %r13
    popq %r14
    popq %r15

    /* Return value for child is 0 */
    xorq %rax, %rax

    /* Return to Ring 3 via atomic iretq */
    iretq

.global enter_usermode
.type enter_usermode, @function
enter_usermode:
    /* %rdi = entry_point
     * %rsi = user_stack_top
     */
    cli

    /* Set data segment registers to User Data (0x18 | 3 = 0x1B) */
    movw $0x1B, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    /* Construct iretq frame */
    pushq $0x1B                 /* SS: User Data */
    pushq %rsi                  /* RSP: User Stack */
    pushq $0x202                /* RFLAGS: IF enabled */
    pushq $0x23                 /* CS: User Code (0x20 | 3) */
    pushq %rdi                  /* RIP: Entry point */

    iretq

