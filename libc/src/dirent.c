#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#define SYS_getdents 78
extern int64_t __syscall(uint64_t num, ...);

DIR *opendir(const char *name) {
    if (!name) {
        errno = EFAULT;
        return NULL;
    }

    int fd = open(name, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }

    DIR *dir = (DIR *)malloc(sizeof(DIR));
    if (!dir) {
        close(fd);
        errno = ENOMEM;
        return NULL;
    }

    dir->fd = fd;
    return dir;
}

struct dirent *readdir(DIR *dirp) {
    if (!dirp || dirp->fd < 0) {
        errno = EBADF;
        return NULL;
    }

    int64_t ret = __syscall(SYS_getdents, (uint64_t)dirp->fd, (uint64_t)&dirp->entry, sizeof(struct dirent));
    if (ret <= 0) {
        if (ret < 0) {
            errno = (int)(-ret);
        }
        return NULL;
    }

    return &dirp->entry;
}

int closedir(DIR *dirp) {
    if (!dirp) {
        errno = EBADF;
        return -1;
    }

    int res = close(dirp->fd);
    free(dirp);
    return res;
}
