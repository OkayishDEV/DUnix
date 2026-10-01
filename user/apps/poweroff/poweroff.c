#include <stdio.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <errno.h>
#include <string.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    if (getuid() != 0) {
        fprintf(stderr, "poweroff: Must be root UID 0 to power off the system\n");
        return 1;
    }

    printf("Syncing filesystems and powering off system...\n");
    sync();

    if (reboot(RB_POWER_OFF) != 0) {
        fprintf(stderr, "poweroff: failed: %s\n", strerror(errno));
        return 1;
    }

    return 0;
}
