#include <syscall/syscall.h>
#include <process/process.h>
#include <sched/sched.h>
#include <compat/linux.h>
#include <arch/x86_64/io.h>
#include <arch/x86_64/cpu/power.h>
#include <fs/vfs.h>
#include <fs/block.h>
#include <ipc/pipe.h>
#include <ipc/signal.h>
#include <net/socket.h>
#include <dunix/kprintf.h>
#include <dunix/string.h>

#define IA32_EFER_MSR  0xC0000080
#define IA32_STAR_MSR  0xC0000081
#define IA32_LSTAR_MSR 0xC0000082
#define IA32_FMASK_MSR 0xC0000084

extern void syscall_entry_asm(void);

int64_t syscall_dispatch(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, struct syscall_frame *frame) {
    uint64_t a6 = frame ? frame->r9 : 0;
    struct process *proc = process_get_current();

    switch (num) {
        case SYS_read:              return sys_read((int)a1, (void *)a2, (size_t)a3);
        case SYS_write:             return sys_write((int)a1, (const void *)a2, (size_t)a3);
        case SYS_open:              return sys_open((const char *)a1, (int)a2, (mode_t)a3);
        case SYS_close:             return sys_close((int)a1);
        case SYS_stat:
            if (proc && proc->is_linux_compat) {
                return linux_sys_newfstatat(LINUX_AT_FDCWD, (const char *)a1, (void *)a2, 0);
            }
            return sys_stat((const char *)a1, (struct stat *)a2);

        case SYS_fstat:
            if (proc && proc->is_linux_compat) {
                return linux_sys_newfstatat((int)a1, "", (void *)a2, 0);
            }
            return sys_fstat((int)a1, (struct stat *)a2);

        case SYS_poll:              return sys_poll((struct pollfd *)a1, (nfds_t)a2, (int)a3);
        case SYS_lseek:             return sys_lseek((int)a1, (off_t)a2, (int)a3);
        case SYS_mmap:              return (int64_t)sys_mmap((void *)a1, (size_t)a2, (int)a3, (int)a4, (int)a5, (off_t)a6);
        case SYS_mprotect:          return linux_sys_mprotect((void *)a1, (size_t)a2, (int)a3);
        case SYS_munmap:            return sys_munmap((void *)a1, (size_t)a2);
        case SYS_brk:               return sys_brk((void *)a1);
        case SYS_rt_sigaction:      return linux_sys_rt_sigaction((int)a1, (const void *)a2, (void *)a3, (size_t)a4);
        case SYS_rt_sigprocmask:    return linux_sys_rt_sigprocmask((int)a1, (const void *)a2, (void *)a3, (size_t)a4);
        case SYS_ioctl:             return sys_ioctl((int)a1, (unsigned long)a2, (void *)a3);
        case SYS_readv:             return linux_sys_readv((int)a1, (const struct linux_iovec *)a2, (int)a3);
        case SYS_writev:            return linux_sys_writev((int)a1, (const struct linux_iovec *)a2, (int)a3);
        case SYS_access:            return linux_sys_access((const char *)a1, (int)a2);
        case SYS_pipe:              return sys_pipe((int *)a1);
        case SYS_select:            return linux_sys_select((int)a1, (void *)a2, (void *)a3, (void *)a4, (struct linux_timeval *)a5);
        case SYS_sched_yield:       sched_yield(); return 0;
        case SYS_madvise:           return 0;
        case SYS_dup:               return sys_dup((int)a1);
        case SYS_dup2:              return sys_dup2((int)a1, (int)a2);
        case SYS_nanosleep:         return sys_nanosleep((const struct timespec *)a1, (struct timespec *)a2);
        case SYS_getpid:            return (int64_t)sys_getpid();
        case SYS_socket:            return sys_socket((int)a1, (int)a2, (int)a3);
        case SYS_connect:           return sys_connect((int)a1, (const struct sockaddr *)a2, (socklen_t)a3);
        case SYS_accept:            return sys_accept((int)a1, (struct sockaddr *)a2, (socklen_t *)a3);
        case SYS_sendto:            return sys_sendto((int)a1, (const void *)a2, (size_t)a3, (int)a4, (const struct sockaddr *)a5, (socklen_t)a6);
        case SYS_recvfrom:          return sys_recvfrom((int)a1, (void *)a2, (size_t)a3, (int)a4, (struct sockaddr *)a5, (socklen_t *)a6);
        case SYS_shutdown:          return sys_shutdown((int)a1, (int)a2);
        case SYS_bind:              return sys_bind((int)a1, (const struct sockaddr *)a2, (socklen_t)a3);
        case SYS_listen:            return sys_listen((int)a1, (int)a2);
        case SYS_getsockname:       return 0;
        case SYS_getpeername:       return 0;
        case SYS_socketpair:        return -38; /* -ENOSYS */
        case SYS_setsockopt:        return 0;
        case SYS_getsockopt:
            if (a5 && *(socklen_t *)a5 >= sizeof(int)) {
                *(int *)a4 = 0; /* Clear error */
            }
            return 0;
        case SYS_fork:              return sys_fork(frame);
        case SYS_execve:            return sys_execve((const char *)a1, (char *const *)a2, (char *const *)a3);
        case SYS_exit:              return sys_exit((int)a1);
        case SYS_wait4:             return sys_waitpid((pid_t)a1, (int *)a2, (int)a3);
        case SYS_kill:              return sys_kill((pid_t)a1, (int)a2);
        case SYS_uname:
            if (proc && proc->is_linux_compat) {
                return linux_sys_uname((struct utsname *)a1);
            }
            return sys_uname((struct utsname *)a1);

        case SYS_fcntl:             return sys_fcntl((int)a1, (int)a2, (uint64_t)a3);
        case SYS_getdents:          return sys_getdents((int)a1, (void *)a2, (size_t)a3);
        case SYS_getcwd:            return sys_getcwd((char *)a1, (size_t)a2);
        case SYS_chdir:             return sys_chdir((const char *)a1);
        case SYS_rename:            return linux_sys_rename((const char *)a1, (const char *)a2);
        case SYS_mkdir:             return sys_mkdir((const char *)a1, (mode_t)a2);
        case SYS_unlink:            return sys_unlink((const char *)a1);
        case SYS_readlink:          return linux_sys_readlink((const char *)a1, (char *)a2, (size_t)a3);
        case SYS_chmod:             return sys_chmod((const char *)a1, (mode_t)a2);
        case SYS_fchmod:            return sys_chmod((const char *)a1, (mode_t)a2);
        case SYS_chown:             return sys_chown((const char *)a1, (uid_t)a2, (gid_t)a3);
        case SYS_fchown:            return sys_chown((const char *)a1, (uid_t)a2, (gid_t)a3);
        case SYS_gettimeofday:      return linux_sys_gettimeofday((struct linux_timeval *)a1, (void *)a2);
        case SYS_getrlimit:         return linux_sys_prlimit64(0, (int)a1, NULL, (void *)a2);
        case SYS_sysinfo:           return linux_sys_sysinfo((struct linux_sysinfo *)a1);
        case SYS_getuid:            return (int64_t)sys_getuid();
        case SYS_getgid:            return (int64_t)sys_getgid();
        case SYS_setuid:            return sys_setuid((uid_t)a1);
        case SYS_setgid:            return sys_setgid((gid_t)a1);
        case SYS_geteuid:           return (int64_t)sys_geteuid();
        case SYS_getegid:           return (int64_t)sys_getegid();
        case SYS_seteuid:           return sys_seteuid((uid_t)a1);
        case SYS_getppid:
            if (proc && proc->is_linux_compat) {
                return (int64_t)sys_getppid();
            }
            return sys_setegid((gid_t)a1);
        case SYS_arch_prctl:        return linux_sys_arch_prctl((int)a1, (unsigned long)a2);
        case SYS_sync:              return block_device_sync_all();
        case SYS_mount:             return sys_mount((const char *)a1, (const char *)a2, (const char *)a3, (unsigned long)a4, (const void *)a5);
        case SYS_umount:            return sys_umount((const char *)a1);
        case SYS_reboot:
            if (proc && proc->euid != 0) {
                return -1; /* -EPERM */
            }
            if ((uint32_t)a3 == LINUX_REBOOT_CMD_POWER_OFF) {
                system_poweroff();
            } else if ((uint32_t)a3 == LINUX_REBOOT_CMD_HALT) {
                system_halt();
            } else {
                system_reboot();
            }
            return 0;
        case SYS_gettid:            return linux_sys_gettid();
        case SYS_futex:             return linux_sys_futex((uint32_t *)a1, (int)a2, (uint32_t)a3, (const struct timespec *)a4, (uint32_t *)a5, (uint32_t)a6);
        case SYS_getdents64:        return linux_sys_getdents64((unsigned int)a1, (void *)a2, (unsigned int)a3);
        case SYS_set_tid_address:   return linux_sys_set_tid_address((int *)a1);
        case SYS_clock_gettime:     return linux_sys_clock_gettime((int)a1, (struct timespec *)a2);
        case SYS_exit_group:        return sys_exit((int)a1);
        case SYS_openat:            return linux_sys_openat((int)a1, (const char *)a2, (int)a3, (mode_t)a4);
        case SYS_mkdirat:           return linux_sys_mkdirat((int)a1, (const char *)a2, (mode_t)a3);
        case SYS_newfstatat:        return linux_sys_newfstatat((int)a1, (const char *)a2, (void *)a3, (int)a4);
        case SYS_unlinkat:          return linux_sys_unlinkat((int)a1, (const char *)a2, (int)a3);
        case SYS_renameat:          return linux_sys_renameat((int)a1, (const char *)a2, (int)a3, (const char *)a4);
        case SYS_readlinkat:        return linux_sys_readlinkat((int)a1, (const char *)a2, (char *)a3, (size_t)a4);
        case SYS_faccessat:         return linux_sys_faccessat((int)a1, (const char *)a2, (int)a3, (int)a4);
        case SYS_pipe2:             return linux_sys_pipe2((int *)a1, (int)a2);
        case SYS_prlimit64:         return linux_sys_prlimit64((pid_t)a1, (int)a2, (const void *)a3, (void *)a4);
        case SYS_getrandom:         return linux_sys_getrandom((void *)a1, (size_t)a2, (unsigned int)a3);
        case SYS_sigaltstack:       return 0;
        case SYS_pselect6:          return linux_sys_pselect6((int)a1, (void *)a2, (void *)a3, (void *)a4, (const struct timespec *)a5, (const void *)a6);
        case SYS_statx:             return -38; /* -ENOSYS */
        case SYS_rseq:              return -38; /* -ENOSYS */
        default:
            klog(KLOG_WARN, "Unhandled syscall: %lu (args: 0x%lx, 0x%lx, 0x%lx)\n", num, a1, a2, a3);
            return -38; /* -ENOSYS */
    }
}

void syscall_init(void) {
    /* Enable Syscall Extensions (SCE) and No-Execute (NXE) in IA32_EFER */
    uint64_t efer = rdmsr(IA32_EFER_MSR);
    efer |= 1 | (1ULL << 11); /* SCE and NXE bits */
    wrmsr(IA32_EFER_MSR, efer);

    /* Configure STAR MSR:
     *   Bits [47:32] = Kernel CS (0x08)
     *   Bits [63:48] = User CS / SS Base (0x10 -> User Code 0x20|3=0x23, User Data 0x18|3=0x1B)
     */
    uint64_t star = ((uint64_t)0x10 << 48) | ((uint64_t)0x08 << 32);
    wrmsr(IA32_STAR_MSR, star);

    /* Configure LSTAR MSR with syscall entry point */
    wrmsr(IA32_LSTAR_MSR, (uint64_t)syscall_entry_asm);

    /* Configure FMASK to clear IF (Interrupt Flag) on syscall entry */
    wrmsr(IA32_FMASK_MSR, 0x200);

    linux_compat_init();
    klog(KLOG_INFO, "Syscall subsystem initialized (Fast SYSCALL/SYSRET MSRs armed)\n");
}
