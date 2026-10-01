.code64

.section .data
.global environ
.type environ, @object
environ:
    .quad 0

.section .text
.global _start
.type _start, @function
_start:
    /* Stack layout initialized by kernel elf_setup_user_stack:
     *   [rsp + 0] = argc
     *   [rsp + 8] = argv[0]
     *   [rsp + 8*argc] = argv[argc-1]
     *   [rsp + 8*(argc+1)] = NULL
     *   [rsp + 8*(argc+2)] = envp[0]
     */
    popq %rdi                   /* %rdi = argc */
    movq %rsp, %rsi             /* %rsi = argv */
    leaq 8(%rsp, %rdi, 8), %rdx /* %rdx = envp */
    movq %rdx, environ(%rip)

    /* Align stack to 16 bytes */
    pushq %rdx
    pushq %rsi
    pushq %rdi
    andq $-16, %rsp

    /* Call int main(int argc, char **argv, char **envp) */
    call main

    /* Pass return value of main to exit(status) */
    movq %rax, %rdi
    call exit

.hang:
    hlt
    jmp .hang
