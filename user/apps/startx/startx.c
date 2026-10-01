#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

static pid_t spawn(const char *path, char *const argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        execve(path, argv, NULL);
        fprintf(stderr, "startx: failed to execute %s\n", path);
        exit(1);
    }
    return pid;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("=======================================================\n");
    printf(" Starting DUnix X11 Desktop Environment (Xorg + twm)\n");
    printf("=======================================================\n\n");

    /* Step 1: Start X11 Display Server */
    printf("startx: launching Xserver on :0 (1024x768x32)...\n");
    char *x_argv[] = { "/bin/Xserver", NULL };
    pid_t x_pid = spawn("/bin/Xserver", x_argv);
    if (x_pid < 0) {
        perror("startx: cannot fork Xserver");
        return 1;
    }

    /* Step 2: Wait for X11 socket to be ready */
    int retries = 20;
    int connected = 0;
    while (retries-- > 0) {
        usleep(100000); /* 100ms */
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd >= 0) {
            struct sockaddr_in saddr;
            memset(&saddr, 0, sizeof(saddr));
            saddr.sin_family = AF_INET;
            saddr.sin_port = htons(6000);
            saddr.sin_addr.s_addr = htonl(0x7F000001);

            if (connect(fd, (struct sockaddr *)&saddr, sizeof(saddr)) == 0) {
                close(fd);
                connected = 1;
                break;
            }
            close(fd);
        }
    }

    if (!connected) {
        fprintf(stderr, "startx: timed out waiting for Xserver socket on 127.0.0.1:6000\n");
        kill(x_pid, 9);
        return 1;
    }

    printf("startx: Xserver ready! Spawning twm, xclock, xeyes, and xterm...\n");

    /* Step 3: Launch Window Manager (twm) */
    char *twm_argv[] = { "/bin/twm", NULL };
    pid_t twm_pid = spawn("/bin/twm", twm_argv);

    /* Step 4: Launch Standard Desktop Applications */
    usleep(100000);
    char *xclock_argv[] = { "/bin/xclock", NULL };
    pid_t xclock_pid = spawn("/bin/xclock", xclock_argv);

    char *xeyes_argv[] = { "/bin/xeyes", NULL };
    pid_t xeyes_pid = spawn("/bin/xeyes", xeyes_argv);

    char *xterm_argv[] = { "/bin/xterm", NULL };
    pid_t xterm_pid = spawn("/bin/xterm", xterm_argv);

    /* Step 5: Wait for Window Manager to exit */
    int status;
    waitpid(twm_pid, &status, 0);

    printf("\nstartx: twm session finished. Cleaning up X11 processes...\n");

    /* Cleanup */
    if (xterm_pid > 0) kill(xterm_pid, 9);
    if (xeyes_pid > 0) kill(xeyes_pid, 9);
    if (xclock_pid > 0) kill(xclock_pid, 9);
    if (x_pid > 0) kill(x_pid, 9);

    printf("startx: Returned to DUnix console.\n");
    return 0;
}
