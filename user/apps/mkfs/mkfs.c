#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>

#define BLKGETSIZE   0x1260
#define BLKFORMAT    0x1261
#define BLKGETSIZE64 0x1268

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s [-t fstype] <device>\n", argv[0]);
        printf("Example: %s /dev/hda\n", argv[0]);
        printf("         %s /dev/ram0\n", argv[0]);
        return 1;
    }

    const char *dev_path = argv[1];
    if (argc >= 4 && strcmp(argv[1], "-t") == 0) {
        dev_path = argv[3];
    } else if (argc >= 3 && strcmp(argv[1], "-t") == 0) {
        dev_path = argv[2];
    }

    int fd = open(dev_path, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "mkfs: cannot open device '%s'\n", dev_path);
        return 1;
    }

    uint64_t blocks = 0;
    if (ioctl(fd, BLKGETSIZE, &blocks) < 0 || blocks == 0) {
        blocks = 2048; /* default fallback */
    }

    printf("mke2fs 1.45 (DUnix Ext2 Filesystem Engine)\n");
    printf("Target device: %s (%lu 512-byte sectors, %lu MB)\n",
           dev_path, (unsigned long)blocks, (unsigned long)(blocks / 2048));
    printf("Discarding device blocks: done\n");
    printf("Allocating group tables: done\n");
    printf("Writing inode tables: done\n");

    int res = ioctl(fd, BLKFORMAT, 0);
    close(fd);

    if (res != 0) {
        fprintf(stderr, "mkfs: formatting failed with code %d\n", res);
        return 1;
    }

    printf("Writing superblocks and filesystem accounting information: done\n");
    printf("Filesystem successfully created on %s!\n", dev_path);
    return 0;
}
