.code64
.section .text

.global __syscall
.type __syscall, @function
__syscall:
    /* C ABI arguments:
     *   rdi = syscall number
     *   rsi = arg1
     *   rdx = arg2
     *   rcx = arg3
     *   r8  = arg4
     *   r9  = arg5
     *   [rsp+8] = arg6
     *
     * Linux/Unix x86_64 Syscall ABI:
     *   rax = syscall number
     *   rdi = arg1
     *   rsi = arg2
     *   rdx = arg3
     *   r10 = arg4
     *   r8  = arg5
     *   r9  = arg6
     */
    movq %rdi, %rax
    movq %rsi, %rdi
    movq %rdx, %rsi
    movq %rcx, %rdx
    movq %r8,  %r10
    movq %r9,  %r8
    movq 8(%rsp), %r9

    syscall

    ret
