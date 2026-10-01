#ifndef _SYSCALL_SYSCALL_H
#define _SYSCALL_SYSCALL_H

#include <dunix/types.h>
#include <arch/x86_64/cpu/idt.h>

/* DUnix & Linux x86_64 POSIX System Call Numbers */
#define SYS_read            0
#define SYS_write           1
#define SYS_open            2
#define SYS_close           3
#define SYS_stat            4
#define SYS_fstat           5
#define SYS_poll            7
#define SYS_lseek           8
#define SYS_mmap            9
#define SYS_mprotect        10
#define SYS_munmap          11
#define SYS_brk             12
#define SYS_rt_sigaction    13
#define SYS_rt_sigprocmask  14
#define SYS_ioctl           16
#define SYS_readv           19
#define SYS_writev          20
#define SYS_access          21
#define SYS_pipe            22
#define SYS_select          23
#define SYS_sched_yield     24
#define SYS_madvise         28
#define SYS_dup             32
#define SYS_dup2            33
#define SYS_nanosleep       35
#define SYS_getpid          39
#define SYS_socket          41
#define SYS_connect         42
#define SYS_accept          43
#define SYS_sendto          44
#define SYS_recvfrom        45
#define SYS_shutdown        48
#define SYS_bind            49
#define SYS_listen          50
#define SYS_getsockname     51
#define SYS_getpeername     52
#define SYS_socketpair      53
#define SYS_setsockopt      54
#define SYS_getsockopt      55
#define SYS_fork            57
#define SYS_execve          59
#define SYS_exit            60
#define SYS_wait4           61
#define SYS_kill            62
#define SYS_uname           63
#define SYS_fcntl           72
#define SYS_getdents        78
#define SYS_getcwd          79
#define SYS_chdir           80
#define SYS_rename          82
#define SYS_mkdir           83
#define SYS_unlink          87
#define SYS_readlink        89
#define SYS_chmod           90
#define SYS_fchmod          91
#define SYS_chown           92
#define SYS_fchown          93
#define SYS_gettimeofday    96
#define SYS_getrlimit       97
#define SYS_sysinfo         99
#define SYS_getuid          102
#define SYS_getgid          104
#define SYS_setuid          105
#define SYS_setgid          106
#define SYS_geteuid         107
#define SYS_getegid         108
#define SYS_seteuid         109
#define SYS_setegid         110
#define SYS_getppid         110
#define SYS_sigaltstack     131
#define SYS_arch_prctl      158
#define SYS_sync            162
#define SYS_mount           165
#define SYS_umount          166
#define SYS_reboot          169
#define SYS_gettid          186
#define SYS_futex           202
#define SYS_getdents64      217
#define SYS_set_tid_address 218
#define SYS_clock_gettime   228
#define SYS_exit_group      231
#define SYS_openat          257
#define SYS_mkdirat         258
#define SYS_newfstatat      262
#define SYS_unlinkat        263
#define SYS_renameat        264
#define SYS_readlinkat      267
#define SYS_faccessat       269
#define SYS_pselect6        270
#define SYS_pipe2           293
#define SYS_prlimit64       302
#define SYS_getrandom       318
#define SYS_statx           332
#define SYS_rseq            334

#define MAX_SYSCALLS        512

struct syscall_frame {
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
};

void    syscall_init(void);
int64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, struct syscall_frame *frame);

/* Ring 3 execution entry helper */
__attribute__((noreturn)) void enter_usermode(uint64_t entry_point, uint64_t user_stack_top);

#endif /* _SYSCALL_SYSCALL_H */
