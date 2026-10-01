#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: kill [-<sig>] <pid...>\n");
        return 1;
    }

    int sig = SIGTERM;
    int pid_start = 1;

    if (argv[1][0] == '-') {
        if (strcmp(argv[1], "-9") == 0 || strcmp(argv[1], "-KILL") == 0) sig = SIGKILL;
        else if (strcmp(argv[1], "-15") == 0 || strcmp(argv[1], "-TERM") == 0) sig = SIGTERM;
        else if (strcmp(argv[1], "-2") == 0 || strcmp(argv[1], "-INT") == 0) sig = SIGINT;
        else if (strcmp(argv[1], "-19") == 0 || strcmp(argv[1], "-STOP") == 0) sig = SIGSTOP;
        else if (strcmp(argv[1], "-18") == 0 || strcmp(argv[1], "-CONT") == 0) sig = SIGCONT;
        else sig = atoi(argv[1] + 1);

        pid_start = 2;
    }

    int ret = 0;
    for (int i = pid_start; i < argc; i++) {
        pid_t pid = (pid_t)atoi(argv[i]);
        if (kill(pid, sig) != 0) {
            fprintf(stderr, "kill: failed to send signal %d to PID %d\n", sig, pid);
            ret = 1;
        }
    }

    return ret;
}
