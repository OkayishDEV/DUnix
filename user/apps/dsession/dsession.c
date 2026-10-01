#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>

void launch_app(const char *path) {
    pid_t pid = fork();
    if (pid == 0) {
        char *argv[] = {(char*)path, NULL};
        execve(path, argv, NULL);
        exit(1);
    }
}

int main(int argc, char **argv) {
    printf("Starting DUnix Desktop Session...\n");

    pid_t dws_pid = fork();
    if (dws_pid == 0) {
        char *dws_argv[] = {"/bin/dws", NULL};
        execve("/bin/dws", dws_argv, NULL);
        exit(1);
    }

    // Wait for DWS server
    int ready = 0;
    for (int i = 0; i < 30; i++) {
        int status = 0;
        pid_t res = waitpid(dws_pid, &status, WNOHANG);
        if (res > 0) {
            printf("Display server (dws) terminated unexpectedly (status=%d).\n", status);
            return 1;
        }

        int s = socket(AF_INET, SOCK_STREAM, 0);
        if (s >= 0) {
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(7000);
            addr.sin_addr.s_addr = htonl(0x7F000001); /* 127.0.0.1 */
            if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
                close(s);
                ready = 1;
                break;
            }
            close(s);
        }
        usleep(100000);
    }

    if (!ready) {
        printf("Failed to connect to display server on 127.0.0.1:7000. Session aborted.\n");
        kill(dws_pid, SIGTERM);
        return 1;
    }

    printf("Display server ready. Launching apps...\n");
    if (argc > 1 && (strcmp(argv[1], "games") == 0 || strcmp(argv[1], "--games") == 0)) {
        launch_app("/bin/snake");
        launch_app("/bin/minesweeper");
        launch_app("/bin/2048");
    } else if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            launch_app(argv[i]);
        }
    } else {
        launch_app("/bin/dterm");
        usleep(100000);
        launch_app("/bin/dclock");
    }

    waitpid(dws_pid, NULL, 0);

    kill(0, SIGTERM); // Kill children
    printf("DUnix Desktop Session ended.\n");
    return 0;
}
