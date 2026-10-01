#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#define SYS_read        0
#define SYS_write       1
#define SYS_close       3
#define SYS_lseek       8
#define SYS_brk         12
#define SYS_pipe        22
#define SYS_dup         32
#define SYS_dup2        33
#define SYS_nanosleep   35
#define SYS_getpid      39
#define SYS_fork        57
#define SYS_execve      59
#define SYS_fcntl       72
#define SYS_getcwd      79
#define SYS_chdir       80
#define SYS_unlink      87
#define SYS_chown       92
#define SYS_fchown      93
#define SYS_getuid      102
#define SYS_getgid      104
#define SYS_setuid      105
#define SYS_setgid      106
#define SYS_geteuid     107
#define SYS_getegid     108
#define SYS_seteuid     109
#define SYS_setegid     110
#define SYS_getppid     110

extern int64_t __syscall(uint64_t num, ...);

ssize_t read(int fd, void *buf, size_t count) {
    int64_t ret = __syscall(SYS_read, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (ssize_t)ret;
}

ssize_t write(int fd, const void *buf, size_t count) {
    int64_t ret = __syscall(SYS_write, (uint64_t)fd, (uint64_t)buf, (uint64_t)count);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (ssize_t)ret;
}

int close(int fd) {
    int64_t ret = __syscall(SYS_close, (uint64_t)fd);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

off_t lseek(int fd, off_t offset, int whence) {
    int64_t ret = __syscall(SYS_lseek, (uint64_t)fd, (uint64_t)offset, (uint64_t)whence);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (off_t)ret;
}

int dup(int oldfd) {
    int64_t ret = __syscall(SYS_dup, (uint64_t)oldfd);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (int)ret;
}

int dup2(int oldfd, int newfd) {
    int64_t ret = __syscall(SYS_dup2, (uint64_t)oldfd, (uint64_t)newfd);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (int)ret;
}

int pipe(int pipefd[2]) {
    int64_t ret = __syscall(SYS_pipe, (uint64_t)pipefd);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

pid_t fork(void) {
    int64_t ret = __syscall(SYS_fork);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (pid_t)ret;
}

int execve(const char *pathname, char *const argv[], char *const envp[]) {
    int64_t ret = __syscall(SYS_execve, (uint64_t)pathname, (uint64_t)argv, (uint64_t)envp);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (int)ret;
}

int execvp(const char *file, char *const argv[]) {
    if (!file || !*file) {
        errno = ENOENT;
        return -1;
    }

    if (strchr(file, '/')) {
        return execve(file, argv, NULL);
    }

    const char *paths[] = { "/bin", "/usr/bin", "/sbin", NULL };
    char full_path[256];

    for (int i = 0; paths[i] != NULL; i++) {
        full_path[0] = '\0';
        strncpy(full_path, paths[i], sizeof(full_path) - 1);
        strncat(full_path, "/", sizeof(full_path) - strlen(full_path) - 1);
        strncat(full_path, file, sizeof(full_path) - strlen(full_path) - 1);

        execve(full_path, argv, NULL);
    }

    errno = ENOENT;
    return -1;
}

pid_t getpid(void) {
    return (pid_t)__syscall(SYS_getpid);
}

pid_t getppid(void) {
    return (pid_t)__syscall(SYS_getppid);
}

uid_t getuid(void) {
    return (uid_t)__syscall(SYS_getuid);
}

gid_t getgid(void) {
    return (gid_t)__syscall(SYS_getgid);
}

uid_t geteuid(void) {
    return (uid_t)__syscall(SYS_geteuid);
}

gid_t getegid(void) {
    return (gid_t)__syscall(SYS_getegid);
}

int setuid(uid_t uid) {
    int64_t ret = __syscall(SYS_setuid, (uint64_t)uid);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int setgid(gid_t gid) {
    int64_t ret = __syscall(SYS_setgid, (uint64_t)gid);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int seteuid(uid_t euid) {
    int64_t ret = __syscall(SYS_seteuid, (uint64_t)euid);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int setegid(gid_t egid) {
    int64_t ret = __syscall(SYS_setegid, (uint64_t)egid);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

char *getcwd(char *buf, size_t size) {
    int64_t ret = __syscall(SYS_getcwd, (uint64_t)buf, (uint64_t)size);
    if (ret < 0) {
        errno = (int)(-ret);
        return NULL;
    }
    return buf;
}

int chdir(const char *path) {
    int64_t ret = __syscall(SYS_chdir, (uint64_t)path);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int unlink(const char *pathname) {
    int64_t ret = __syscall(SYS_unlink, (uint64_t)pathname);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int chown(const char *pathname, uid_t owner, gid_t group) {
    int64_t ret = __syscall(SYS_chown, (uint64_t)pathname, (uint64_t)owner, (uint64_t)group);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

int fchown(int fd, uid_t owner, gid_t group) {
    int64_t ret = __syscall(SYS_fchown, (uint64_t)fd, (uint64_t)owner, (uint64_t)group);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

void *sbrk(intptr_t increment) {
    static uintptr_t current_brk = 0;

    if (current_brk == 0) {
        current_brk = (uintptr_t)__syscall(SYS_brk, 0);
    }

    if (increment == 0) {
        return (void *)current_brk;
    }

    uintptr_t new_brk = current_brk + increment;
    uintptr_t res = (uintptr_t)__syscall(SYS_brk, new_brk);
    if (res < new_brk) {
        errno = ENOMEM;
        return (void *)-1;
    }

    uintptr_t old_brk = current_brk;
    current_brk = res;
    return (void *)old_brk;
}

unsigned int sleep(unsigned int seconds) {
    struct timespec req;
    req.tv_sec = seconds;
    req.tv_nsec = 0;
    nanosleep(&req, NULL);
    return 0;
}

int usleep(useconds_t usec) {
    struct timespec req;
    req.tv_sec = usec / 1000000;
    req.tv_nsec = (usec % 1000000) * 1000;
    return nanosleep(&req, NULL);
}

int isatty(int fd) {
    return (fd >= 0 && fd <= 2) ? 1 : 0;
}

int fcntl(int fd, int cmd, ...) {
    uint64_t arg = 0;
    __builtin_va_list ap;
    __builtin_va_start(ap, cmd);
    arg = __builtin_va_arg(ap, uint64_t);
    __builtin_va_end(ap);

    int64_t ret = __syscall(SYS_fcntl, (uint64_t)fd, (uint64_t)cmd, arg);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return (int)ret;
}

#define SYS_sync   162
#define SYS_reboot 169

void sync(void) {
    __syscall(SYS_sync);
}

int reboot(int cmd) {
    int64_t ret = __syscall(SYS_reboot, 0xFEE1DEADULL, 672274793ULL, (uint64_t)cmd, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}

