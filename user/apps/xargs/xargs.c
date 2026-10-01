#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>

int main(int argc, char **argv) {
    const char *cmd = (argc > 1) ? argv[1] : "echo";

    char line[512];
    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (line[0] == '\0') continue;

        char *exec_argv[32];
        exec_argv[0] = (char *)cmd;
        int arg_idx = 1;

        /* Append arguments passed on command line */
        for (int i = 2; i < argc && arg_idx < 30; i++) {
            exec_argv[arg_idx++] = argv[i];
        }

        /* Tokenize input line */
        char *p = line;
        while (*p && arg_idx < 30) {
            while (isspace((unsigned char)*p)) p++;
            if (!*p) break;
            exec_argv[arg_idx++] = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (*p) *p++ = '\0';
        }
        exec_argv[arg_idx] = NULL;

        pid_t pid = fork();
        if (pid == 0) {
            execvp(exec_argv[0], exec_argv);
            perror("xargs");
            exit(127);
        } else if (pid > 0) {
            int status = 0;
            waitpid(pid, &status, 0);
        }
    }

    return 0;
}
