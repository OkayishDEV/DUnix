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
        fprintf(stderr, "startlxde: failed to execute %s\n", path);
        exit(1);
    }
    return pid;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("=======================================================\n");
    printf(" Starting DUnix LXDE Desktop Environment (Openbox+LXPanel)\n");
    printf("=======================================================\n\n");

    /* Step 1: Start X11 Display Server */
    printf("startlxde: launching Xserver on :0 (1024x768x32)...\n");
    char *x_argv[] = { "/bin/Xserver", NULL };
    pid_t x_pid = spawn("/bin/Xserver", x_argv);
    if (x_pid < 0) {
        perror("startlxde: cannot fork Xserver");
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
        fprintf(stderr, "startlxde: timed out waiting for Xserver socket on 127.0.0.1:6000\n");
        kill(x_pid, 9);
        return 1;
    }

    printf("startlxde: Xserver ready! Spawning Openbox, LXPanel, and PCManFM...\n");

    /* Step 3: Launch Openbox Window Manager */
    char *openbox_argv[] = { "/bin/openbox", NULL };
    pid_t openbox_pid = spawn("/bin/openbox", openbox_argv);

    /* Step 4: Launch LXPanel (Taskbar & Start Menu) */
    usleep(100000);
    char *lxpanel_argv[] = { "/bin/lxpanel", NULL };
    pid_t lxpanel_pid = spawn("/bin/lxpanel", lxpanel_argv);

    /* Step 5: Launch PCManFM File Manager */
    usleep(100000);
    char *pcmanfm_argv[] = { "/bin/pcmanfm", NULL };
    pid_t pcmanfm_pid = spawn("/bin/pcmanfm", pcmanfm_argv);

    /* Step 6: Launch Initial Terminal */
    usleep(100000);
    char *xterm_argv[] = { "/bin/xterm", NULL };
    pid_t xterm_pid = spawn("/bin/xterm", xterm_argv);

    /* Step 7: Wait for Openbox session manager to exit */
    int status;
    waitpid(openbox_pid, &status, 0);

    printf("\nstartlxde: LXDE session finished. Cleaning up desktop processes...\n");

    if (xterm_pid > 0) kill(xterm_pid, 9);
    if (pcmanfm_pid > 0) kill(pcmanfm_pid, 9);
    if (lxpanel_pid > 0) kill(lxpanel_pid, 9);
    if (x_pid > 0) kill(x_pid, 9);

    printf("startlxde: Returned to DUnix console.\n");
    return 0;
}
