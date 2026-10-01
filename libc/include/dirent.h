#ifndef _LIBC_DIRENT_H
#define _LIBC_DIRENT_H

#include <sys/types.h>

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10

struct dirent {
    char     d_name[128];
    uint32_t d_ino;
    uint32_t d_type;
};

typedef struct {
    int           fd;
    struct dirent entry;
} DIR;

DIR           *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int            closedir(DIR *dirp);

#endif /* _LIBC_DIRENT_H */
