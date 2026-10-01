#include <compat/linux.h>
#include <process/process.h>
#include <sched/sched.h>
#include <sched/thread.h>
#include <fs/vfs.h>
#include <ipc/pipe.h>
#include <mm/pmm.h>
#include <arch/x86_64/drivers/pit.h>
#include <arch/x86_64/drivers/rtc.h>
#include <arch/x86_64/io.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

#define IA32_FS_BASE_MSR 0xC0000100
#define IA32_GS_BASE_MSR 0xC0000101

static uint64_t lcg_entropy_seed = 0x5a17c92b4f6e803dULL;

void linux_compat_init(void) {
    klog(KLOG_INFO, "Linux Binary Compatibility Subsystem (Linuxulator) initialized\n");
}

void linux_fill_stat(struct vfs_node *node, struct linux_stat *lst) {
    if (!node || !lst) return;

    memset(lst, 0, sizeof(struct linux_stat));
    lst->st_dev = 1;
    lst->st_ino = node->inode ? node->inode : 1;
    lst->st_nlink = 1;

    uint32_t type_bits = 0100000; /* S_IFREG */
    if (node->flags == VFS_DIRECTORY)        type_bits = 0040000; /* S_IFDIR */
    else if (node->flags == VFS_CHARDEVICE)  type_bits = 0020000; /* S_IFCHR */
    else if (node->flags == VFS_BLOCKDEVICE) type_bits = 0060000; /* S_IFBLK */
    else if (node->flags == VFS_PIPE)        type_bits = 0010000; /* S_IFIFO */
    else if (node->flags == VFS_SYMLINK)     type_bits = 0120000; /* S_IFLNK */

    lst->st_mode = type_bits | (node->mask & 07777);
    lst->st_uid = node->uid;
    lst->st_gid = node->gid;
    lst->st_rdev = 0;
    lst->st_size = (int64_t)node->length;
    lst->st_blksize = 4096;
    lst->st_blocks = (lst->st_size + 511) / 512;
    lst->st_atime = 1700000000;
    lst->st_mtime = 1700000000;
    lst->st_ctime = 1700000000;
}

const char *linux_resolve_path(const char *path, char *out_buf, size_t buf_sz) {
    if (!path) return NULL;
    struct process *proc = process_get_current();

    if (proc && proc->is_linux_compat && path[0] == '/' && strncmp(path, "/compat/linux", 13) != 0) {
        ksnprintf(out_buf, buf_sz, "/compat/linux%s", path);
        struct vfs_node *compat_node = vfs_lookup(out_buf);
        if (compat_node) {
            return out_buf;
        }
    }

    return path;
}

int64_t linux_sys_arch_prctl(int code, unsigned long addr) {
    struct process *proc = process_get_current();
    if (!proc) return -1;

    switch (code) {
        case ARCH_SET_FS:
            proc->fs_base = (uint64_t)addr;
            if (proc->main_thread) {
                proc->main_thread->fs_base = (uint64_t)addr;
            }
            wrmsr(IA32_FS_BASE_MSR, (uint64_t)addr);
            return 0;

        case ARCH_GET_FS:
            if (!addr) return -14; /* -EFAULT */
            *(uint64_t *)addr = proc->fs_base;
            return 0;

        case ARCH_SET_GS:
            proc->gs_base = (uint64_t)addr;
            wrmsr(IA32_GS_BASE_MSR, (uint64_t)addr);
            return 0;

        case ARCH_GET_GS:
            if (!addr) return -14; /* -EFAULT */
            *(uint64_t *)addr = proc->gs_base;
            return 0;

        default:
            klog(KLOG_WARN, "Linux arch_prctl: unsupported code 0x%x\n", code);
            return -22; /* -EINVAL */
    }
}

int64_t linux_sys_set_tid_address(int *tidptr) {
    struct process *proc = process_get_current();
    if (!proc) return -1;
    proc->clear_child_tid = tidptr;
    return (int64_t)proc->pid;
}

int64_t linux_sys_openat(int dfd, const char *filename, int flags, mode_t mode) {
    if (!filename) return -14; /* -EFAULT */

    char path_buf[256];
    const char *target = linux_resolve_path(filename, path_buf, sizeof(path_buf));

    if (filename[0] != '/' && dfd != LINUX_AT_FDCWD) {
        struct process *proc = process_get_current();
        if (!proc || dfd < 0 || dfd >= MAX_FD || !proc->files[dfd]) {
            return -9; /* -EBADF */
        }
        /* Relative to directory fd */
        struct vfs_node *dir = proc->files[dfd]->node;
        if (!dir || !(dir->flags & VFS_DIRECTORY)) {
            return -20; /* -ENOTDIR */
        }
        ksnprintf(path_buf, sizeof(path_buf), "%s/%s", dir->name, filename);
        target = path_buf;
    }

    return sys_open(target, flags, mode);
}

int64_t linux_sys_mkdirat(int dfd, const char *pathname, mode_t mode) {
    if (!pathname) return -14; /* -EFAULT */

    char path_buf[256];
    const char *target = linux_resolve_path(pathname, path_buf, sizeof(path_buf));

    if (pathname[0] != '/' && dfd != LINUX_AT_FDCWD) {
        struct process *proc = process_get_current();
        if (!proc || dfd < 0 || dfd >= MAX_FD || !proc->files[dfd]) {
            return -9; /* -EBADF */
        }
        struct vfs_node *dir = proc->files[dfd]->node;
        if (!dir || !(dir->flags & VFS_DIRECTORY)) {
            return -20; /* -ENOTDIR */
        }
        ksnprintf(path_buf, sizeof(path_buf), "%s/%s", dir->name, pathname);
        target = path_buf;
    }

    return sys_mkdir(target, mode);
}

int64_t linux_sys_unlinkat(int dfd, const char *pathname, int flags) {
    (void)flags;
    if (!pathname) return -14;

    char path_buf[256];
    const char *target = linux_resolve_path(pathname, path_buf, sizeof(path_buf));

    if (pathname[0] != '/' && dfd != LINUX_AT_FDCWD) {
        struct process *proc = process_get_current();
        if (!proc || dfd < 0 || dfd >= MAX_FD || !proc->files[dfd]) {
            return -9;
        }
        struct vfs_node *dir = proc->files[dfd]->node;
        if (!dir || !(dir->flags & VFS_DIRECTORY)) {
            return -20;
        }
        ksnprintf(path_buf, sizeof(path_buf), "%s/%s", dir->name, pathname);
        target = path_buf;
    }

    return sys_unlink(target);
}

int64_t linux_sys_rename(const char *oldpath, const char *newpath) {
    if (!oldpath || !newpath) return -14;
    char old_buf[256], new_buf[256];
    const char *target_old = linux_resolve_path(oldpath, old_buf, sizeof(old_buf));
    const char *target_new = linux_resolve_path(newpath, new_buf, sizeof(new_buf));
    return sys_rename(target_old, target_new);
}

int64_t linux_sys_renameat(int olddfd, const char *oldpath, int newdfd, const char *newpath) {
    (void)olddfd; (void)newdfd;
    return linux_sys_rename(oldpath, newpath);
}

int64_t linux_sys_faccessat(int dfd, const char *pathname, int mode, int flags) {
    (void)flags;
    if (!pathname) return -14;

    char path_buf[256];
    const char *target = linux_resolve_path(pathname, path_buf, sizeof(path_buf));

    if (pathname[0] != '/' && dfd != LINUX_AT_FDCWD) {
        struct process *proc = process_get_current();
        if (!proc || dfd < 0 || dfd >= MAX_FD || !proc->files[dfd]) {
            return -9;
        }
        struct vfs_node *dir = proc->files[dfd]->node;
        if (!dir || !(dir->flags & VFS_DIRECTORY)) {
            return -20;
        }
        ksnprintf(path_buf, sizeof(path_buf), "%s/%s", dir->name, pathname);
        target = path_buf;
    }

    return linux_sys_access(target, mode);
}

int64_t linux_sys_newfstatat(int dfd, const char *filename, void *statbuf, int flag) {
    (void)flag;
    if (!statbuf) return -14; /* -EFAULT */

    struct process *proc = process_get_current();
    struct vfs_node *node = NULL;

    if (filename && filename[0] != '\0') {
        char path_buf[256];
        const char *target = linux_resolve_path(filename, path_buf, sizeof(path_buf));
        node = vfs_lookup(target);
    } else {
        /* Filename empty: inspect directory fd directly */
        if (!proc || dfd < 0 || dfd >= MAX_FD || !proc->files[dfd]) {
            return -9; /* -EBADF */
        }
        node = proc->files[dfd]->node;
    }

    if (!node) {
        return -2; /* -ENOENT */
    }

    linux_fill_stat(node, (struct linux_stat *)statbuf);
    return 0;
}

int64_t linux_sys_getdents64(unsigned int fd, void *dirp, unsigned int count) {
    if (!dirp || count < sizeof(struct linux_dirent64)) return -14; /* -EFAULT */

    struct process *proc = process_get_current();
    if (!proc || (int)fd < 0 || (int)fd >= MAX_FD || !proc->files[fd]) {
        return -9; /* -EBADF */
    }

    struct file *f = proc->files[fd];
    if (!f->node || (f->node->flags & VFS_DIRECTORY) != VFS_DIRECTORY) {
        return -20; /* -ENOTDIR */
    }

    uint8_t *buf = (uint8_t *)dirp;
    unsigned int bytes_written = 0;

    while (bytes_written < count) {
        struct dirent *d = vfs_readdir(f->node, (uint32_t)f->offset);
        if (!d) {
            break; /* End of directory stream */
        }

        size_t name_len = strlen(d->d_name);
        /* Linux struct linux_dirent64 header size:
         * 8 (d_ino) + 8 (d_off) + 2 (d_reclen) + 1 (d_type) = 19 bytes
         * Followed by null-terminated d_name, aligned to 8-byte boundary.
         */
        unsigned short reclen = (unsigned short)(((19 + name_len + 1) + 7) & ~7);

        if (bytes_written + reclen > count) {
            if (bytes_written == 0) return -22; /* -EINVAL if buffer is too small for 1 entry */
            break;
        }

        struct linux_dirent64 *ld = (struct linux_dirent64 *)(buf + bytes_written);
        memset(ld, 0, reclen);
        ld->d_ino = d->d_ino ? (uint64_t)d->d_ino : (uint64_t)(f->offset + 1);
        ld->d_off = (int64_t)(f->offset + 1);
        ld->d_reclen = reclen;
        ld->d_type = (unsigned char)d->d_type;
        memcpy(ld->d_name, d->d_name, name_len + 1);

        bytes_written += reclen;
        f->offset++;
    }

    return (int64_t)bytes_written;
}

int64_t linux_sys_writev(int fd, const struct linux_iovec *iov, int iovcnt) {
    if (!iov || iovcnt < 0 || iovcnt > 1024) return -14; /* -EFAULT / -EINVAL */

    int64_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (iov[i].iov_len == 0) continue;
        if (!iov[i].iov_base) return -14;

        ssize_t ret = sys_write(fd, iov[i].iov_base, iov[i].iov_len);
        if (ret < 0) {
            return (total > 0) ? total : (int64_t)ret;
        }
        total += ret;
    }
    return total;
}

int64_t linux_sys_readv(int fd, const struct linux_iovec *iov, int iovcnt) {
    if (!iov || iovcnt < 0 || iovcnt > 1024) return -14;

    int64_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (iov[i].iov_len == 0) continue;
        if (!iov[i].iov_base) return -14;

        ssize_t ret = sys_read(fd, iov[i].iov_base, iov[i].iov_len);
        if (ret < 0) {
            return (total > 0) ? total : (int64_t)ret;
        }
        total += ret;
        if ((size_t)ret < iov[i].iov_len) break;
    }
    return total;
}

int64_t linux_sys_clock_gettime(int which_clock, struct timespec *tp) {
    if (!tp) return -14;

    uint64_t ticks = pit_get_ticks();
    if (which_clock == 0) { /* CLOCK_REALTIME */
        tp->tv_sec = (time_t)rtc_get_epoch();
    } else { /* CLOCK_MONOTONIC and others */
        tp->tv_sec = (time_t)(ticks / PIT_TARGET_HZ);
    }
    tp->tv_nsec = (long)((ticks % PIT_TARGET_HZ) * (1000000000ULL / PIT_TARGET_HZ));
    return 0;
}

int64_t linux_sys_futex(uint32_t *uaddr, int op, uint32_t val, const struct timespec *timeout, uint32_t *uaddr2, uint32_t val3) {
    (void)timeout; (void)uaddr2; (void)val3;
    if (!uaddr) return -14;

    int cmd = op & ~128; /* Clear FUTEX_PRIVATE_FLAG */
    cmd &= ~256;         /* Clear FUTEX_CLOCK_REALTIME */

    switch (cmd) {
        case 0: /* FUTEX_WAIT */
            if (*uaddr != val) {
                return -11; /* -EAGAIN */
            }
            sched_yield();
            return 0;
        case 1: /* FUTEX_WAKE */
            return 0; /* 0 waiters woken */
        default:
            return 0;
    }
}

int64_t linux_sys_prlimit64(pid_t pid, int resource, const void *new_limit, void *old_limit) {
    (void)pid; (void)new_limit;
    if (old_limit) {
        struct linux_rlimit64 *lim = (struct linux_rlimit64 *)old_limit;
        if (resource == 3) { /* RLIMIT_STACK */
            lim->rlim_cur = 8 * 1024 * 1024;  /* 8 MB */
            lim->rlim_max = 64 * 1024 * 1024; /* 64 MB */
        } else if (resource == 7) { /* RLIMIT_NOFILE */
            lim->rlim_cur = MAX_FD;
            lim->rlim_max = MAX_FD;
        } else {
            lim->rlim_cur = 0x7fffffffffffffffULL;
            lim->rlim_max = 0x7fffffffffffffffULL;
        }
    }
    return 0;
}

int64_t linux_sys_getrandom(void *buf, size_t buflen, unsigned int flags) {
    (void)flags;
    if (!buf && buflen > 0) return -14;

    uint8_t *p = (uint8_t *)buf;
    uint64_t ticks = pit_get_ticks();
    lcg_entropy_seed ^= (ticks << 19) ^ (ticks >> 5) ^ 0x6a09e667f3bcc908ULL;

    for (size_t i = 0; i < buflen; i++) {
        lcg_entropy_seed = lcg_entropy_seed * 6364136223846793005ULL + 1442695040888963407ULL;
        p[i] = (uint8_t)(lcg_entropy_seed >> 56);
    }
    return (int64_t)buflen;
}

int64_t linux_sys_access(const char *pathname, int mode) {
    (void)mode;
    if (!pathname) return -14;

    char path_buf[256];
    const char *target = linux_resolve_path(pathname, path_buf, sizeof(path_buf));
    struct vfs_node *node = vfs_lookup(target);
    if (!node) {
        return -2; /* -ENOENT */
    }
    return 0;
}

int64_t linux_sys_readlink(const char *path, char *buf, size_t bufsiz) {
    if (!path || !buf || bufsiz == 0) return -14;
    struct process *proc = process_get_current();

    if (strcmp(path, "/proc/self/exe") == 0 || strcmp(path, "/proc/thread-self/exe") == 0) {
        const char *name = (proc && proc->name[0]) ? proc->name : "/bin/app";
        size_t len = strlen(name);
        if (len > bufsiz) len = bufsiz;
        memcpy(buf, name, len);
        return (int64_t)len;
    }
    return -22; /* -EINVAL */
}

int64_t linux_sys_readlinkat(int dfd, const char *path, char *buf, size_t bufsiz) {
    (void)dfd;
    return linux_sys_readlink(path, buf, bufsiz);
}

int64_t linux_sys_gettimeofday(struct linux_timeval *tv, void *tz) {
    (void)tz;
    if (tv) {
        uint64_t ticks = pit_get_ticks();
        tv->tv_sec = (time_t)rtc_get_epoch();
        tv->tv_usec = (long)((ticks % PIT_TARGET_HZ) * (1000000ULL / PIT_TARGET_HZ));
    }
    return 0;
}

int64_t linux_sys_sysinfo(struct linux_sysinfo *info) {
    if (!info) return -14;

    memset(info, 0, sizeof(struct linux_sysinfo));
    info->uptime = (int64_t)(pit_get_ticks() / PIT_TARGET_HZ);
    info->totalram = pmm_get_total_frames() * 4096;
    info->freeram = pmm_get_free_frames() * 4096;
    info->procs = 4;
    info->mem_unit = 1;
    return 0;
}

int64_t linux_sys_mprotect(void *addr, size_t len, int prot) {
    (void)addr; (void)len; (void)prot;
    return 0; /* Succeed dummy mprotect */
}

int64_t linux_sys_rt_sigaction(int signum, const void *act, void *oldact, size_t sigsetsize) {
    (void)signum; (void)act; (void)oldact; (void)sigsetsize;
    return 0;
}

int64_t linux_sys_rt_sigprocmask(int how, const void *set, void *oldset, size_t sigsetsize) {
    (void)how; (void)set; (void)oldset; (void)sigsetsize;
    return 0;
}

int64_t linux_sys_gettid(void) {
    struct process *proc = process_get_current();
    return proc ? (int64_t)proc->pid : 1;
}

int64_t linux_sys_pipe2(int pipefd[2], int flags) {
    (void)flags;
    return sys_pipe(pipefd);
}

int64_t linux_sys_uname(struct utsname *buf) {
    if (!buf) return -14;

    memset(buf, 0, sizeof(struct utsname));
    strcpy(buf->sysname, "Linux");
    strcpy(buf->nodename, "dunix");
    strcpy(buf->release, "5.15.0-dunix");
    strcpy(buf->version, "#1 SMP PREEMPT DUnix Linuxulator 2026");
    strcpy(buf->machine, "x86_64");
    strcpy(buf->domainname, "localdomain");
    return 0;
}

int64_t linux_sys_select(int nfds, void *readfds, void *writefds, void *exceptfds, struct linux_timeval *timeout) {
    if (nfds < 0 || nfds > 1024) return -22; /* -EINVAL */
    if (nfds > MAX_FD) nfds = MAX_FD;

    uint64_t *rfds = (uint64_t *)readfds;
    uint64_t *wfds = (uint64_t *)writefds;
    uint64_t *efds = (uint64_t *)exceptfds;

    struct pollfd pfds[MAX_FD];
    int pfd_map[MAX_FD];
    int count = 0;

    for (int fd = 0; fd < nfds; fd++) {
        int word = fd / 64;
        int bit = fd % 64;
        short events = 0;

        if (rfds && (rfds[word] & (1ULL << bit))) events |= POLLIN;
        if (wfds && (wfds[word] & (1ULL << bit))) events |= POLLOUT;
        if (efds && (efds[word] & (1ULL << bit))) events |= POLLPRI;

        if (events) {
            pfds[count].fd = fd;
            pfds[count].events = events;
            pfds[count].revents = 0;
            pfd_map[count] = fd;
            count++;
        }
    }

    int timeout_ms = -1;
    if (timeout) {
        timeout_ms = (int)(timeout->tv_sec * 1000 + timeout->tv_usec / 1000);
        if (timeout_ms < 0) timeout_ms = 0;
    }

    int64_t ret = sys_poll(pfds, count, timeout_ms);
    if (ret < 0) return ret;

    /* Clear output bitmasks */
    if (rfds) memset(rfds, 0, ((nfds + 63) / 64) * sizeof(uint64_t));
    if (wfds) memset(wfds, 0, ((nfds + 63) / 64) * sizeof(uint64_t));
    if (efds) memset(efds, 0, ((nfds + 63) / 64) * sizeof(uint64_t));

    int ready = 0;
    for (int i = 0; i < count; i++) {
        int fd = pfd_map[i];
        int word = fd / 64;
        int bit = fd % 64;

        if (rfds && (pfds[i].revents & (POLLIN | POLLHUP | POLLERR))) {
            rfds[word] |= (1ULL << bit);
            ready++;
        }
        if (wfds && (pfds[i].revents & (POLLOUT | POLLERR))) {
            wfds[word] |= (1ULL << bit);
            ready++;
        }
    }

    return ready;
}

int64_t linux_sys_pselect6(int nfds, void *readfds, void *writefds, void *exceptfds, const struct timespec *timeout, const void *sigmask) {
    (void)sigmask;
    struct linux_timeval tv;
    struct linux_timeval *tvp = NULL;
    if (timeout) {
        tv.tv_sec = timeout->tv_sec;
        tv.tv_usec = timeout->tv_nsec / 1000;
        tvp = &tv;
    }
    return linux_sys_select(nfds, readfds, writefds, exceptfds, tvp);
}
