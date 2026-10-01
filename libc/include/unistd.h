#ifndef _LIBC_UNISTD_H
#define _LIBC_UNISTD_H

#include <stddef.h>
#include <sys/types.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int     close(int fd);
off_t   lseek(int fd, off_t offset, int whence);
int     dup(int oldfd);
int     dup2(int oldfd, int newfd);
int     pipe(int pipefd[2]);

pid_t   fork(void);
int     execve(const char *pathname, char *const argv[], char *const envp[]);
int     execvp(const char *file, char *const argv[]);

pid_t   getpid(void);
pid_t   getppid(void);
uid_t   getuid(void);
gid_t   getgid(void);
uid_t   geteuid(void);
gid_t   getegid(void);
int     setuid(uid_t uid);
int     setgid(gid_t gid);
int     seteuid(uid_t euid);
int     setegid(gid_t egid);

char   *getcwd(char *buf, size_t size);
int     chdir(const char *path);
int     unlink(const char *pathname);
int     chown(const char *pathname, uid_t owner, gid_t group);
int     fchown(int fd, uid_t owner, gid_t group);

void   *sbrk(intptr_t increment);
unsigned int sleep(unsigned int seconds);
int     usleep(useconds_t usec);
int     isatty(int fd);
void    sync(void);
int     reboot(int cmd);

#endif /* _LIBC_UNISTD_H */
