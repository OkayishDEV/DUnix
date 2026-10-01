#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>

static void trim(char *s) {
    if (!s) return;
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    char username[64];
    char password[64];

    printf("\n");
    printf("DUnix Multi-User Login System\n");

    for (;;) {
        printf("dunix login: ");
        fflush(stdout);

        if (!fgets(username, sizeof(username), stdin)) {
            break;
        }
        trim(username);
        if (strlen(username) == 0) continue;

        printf("Password: ");
        fflush(stdout);
        if (!fgets(password, sizeof(password), stdin)) {
            break;
        }
        trim(password);

        /* Authenticate against /etc/passwd */
        FILE *fp = fopen("/etc/passwd", "r");
        if (!fp) {
            fprintf(stderr, "login: cannot read /etc/passwd\n");
            return 1;
        }

        char line[256];
        bool authenticated = false;
        uid_t target_uid = 0;
        gid_t target_gid = 0;
        char home_dir[128] = "/root";
        char shell_path[128] = "/bin/sh";

        while (fgets(line, sizeof(line), fp)) {
            trim(line);
            if (line[0] == '#' || strlen(line) == 0) continue;

            /* Parse username:password:uid:gid:gecos:homedir:shell */
            char *u = strtok(line, ":");
            char *p = strtok(NULL, ":");
            char *uid_s = strtok(NULL, ":");
            char *gid_s = strtok(NULL, ":");
            char *gecos = strtok(NULL, ":");
            char *home = strtok(NULL, ":");
            char *sh = strtok(NULL, ":");

            (void)p; (void)gecos;

            if (u && strcmp(u, username) == 0) {
                authenticated = true;
                if (uid_s) target_uid = (uid_t)atoi(uid_s);
                if (gid_s) target_gid = (gid_t)atoi(gid_s);
                if (home) strncpy(home_dir, home, sizeof(home_dir) - 1);
                if (sh) strncpy(shell_path, sh, sizeof(shell_path) - 1);
                break;
            }
        }
        fclose(fp);

        if (authenticated) {
            printf("\nAuthentication successful. Welcome, %s!\n\n", username);
            setgid(target_gid);
            setuid(target_uid);
            setenv("USER", username, 1);
            setenv("HOME", home_dir, 1);
            chdir(home_dir);

            char *sh_argv[] = { shell_path, NULL };
            execve(shell_path, sh_argv, NULL);
            fprintf(stderr, "login: failed to execute %s\n", shell_path);
            return 1;
        } else {
            printf("Login incorrect. Try again.\n");
        }
    }

    return 0;
}
