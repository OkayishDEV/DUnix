#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc == 1) {
        int fd = open("/proc/mounts", O_RDONLY);
        if (fd >= 0) {
            char buf[512];
            ssize_t n;
            while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
                buf[n] = '\0';
                printf("%s", buf);
            }
            close(fd);
        }
        return 0;
    }

    if (argc < 3) {
        fprintf(stderr, "Usage: mount [-t <fstype>] <source> <target>\n");
        return 1;
    }

    const char *fstype = "ext2";
    const char *source = NULL;
    const char *target = NULL;

    if (argc >= 5 && strcmp(argv[1], "-t") == 0) {
        fstype = argv[2];
        source = argv[3];
        target = argv[4];
    } else if (argc >= 4 && strcmp(argv[1], "-t") != 0) {
        source = argv[1];
        target = argv[2];
        fstype = argv[3];
    } else {
        source = argv[1];
        target = argv[2];
    }

    if (mount(source, target, fstype, 0, NULL) != 0) {
        fprintf(stderr, "mount: failed to mount %s onto %s\n", source, target);
        return 1;
    }

    printf("Mounted %s on %s (type %s)\n", source, target, fstype);
    return 0;
}
