#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    /* Open standard I/O streams on /dev/console if not already open */
    int con_fd = open("/dev/console", O_RDWR);
    if (con_fd >= 0) {
        if (con_fd != 0) dup2(con_fd, 0);
        dup2(0, 1);
        dup2(0, 2);
        if (con_fd > 2) close(con_fd);
    }

    printf("\n");
    printf("=======================================================\n");
    printf(" DUnix Init System (PID %d)\n", getpid());
    printf(" Starting Userspace Unix Environment...\n");
    printf(" Spawning /bin/sh interactive shell\n");
    printf("=======================================================\n\n");

    for (;;) {
        pid_t pid = fork();
        if (pid < 0) {
            printf("[init] fork failed, sleeping...\n");
            sleep(1);
            continue;
        }

        if (pid == 0) {
            /* Child: Execute shell */
            char *sh_argv[] = { "/bin/sh", NULL };
            execve("/bin/sh", sh_argv, NULL);
            printf("[init] Failed to exec /bin/sh\n");
            exit(1);
        }

        /* Parent: Wait and reap child processes */
        for (;;) {
            int status = 0;
            pid_t reaped = waitpid(-1, &status, 0);
            if (reaped == pid) {
                printf("\n[init] Shell (PID %d) terminated with status %d. Respawning...\n", reaped, status);
                sleep(1);
                break;
            }
        }
    }

    return 0;
}
