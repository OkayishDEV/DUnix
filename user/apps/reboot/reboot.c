#include <stdio.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <errno.h>
#include <string.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    if (getuid() != 0) {
        fprintf(stderr, "reboot: Must be root UID 0 to restart the system\n");
        return 1;
    }

    printf("Syncing filesystems and restarting system...\n");
    sync();

    if (reboot(RB_AUTOBOOT) != 0) {
        fprintf(stderr, "reboot: failed: %s\n", strerror(errno));
        return 1;
    }

    return 0;
}
