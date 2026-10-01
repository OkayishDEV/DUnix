#include <stdio.h>
#include <sys/mount.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: umount <target>\n");
        return 1;
    }

    if (umount(argv[1]) != 0) {
        fprintf(stderr, "umount: failed to unmount %s\n", argv[1]);
        return 1;
    }

    printf("Unmounted %s\n", argv[1]);
    return 0;
}
