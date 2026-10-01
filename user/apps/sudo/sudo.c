#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
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

static char *find_whitespace(char *s) {
    while (*s) {
        if (isspace((unsigned char)*s)) return s;
        s++;
    }
    return NULL;
}

static bool check_group_membership(const char *username, const char *group_name) {
    FILE *fp = fopen("/etc/group", "r");
    if (!fp) return false;

    char line[256];
    bool member = false;

    while (fgets(line, sizeof(line), fp)) {
        trim(line);
        if (line[0] == '#' || strlen(line) == 0) continue;

        char *gname = strtok(line, ":");
        strtok(NULL, ":"); /* pass */
        strtok(NULL, ":"); /* gid */
        char *members = strtok(NULL, ":");

        if (gname && strcmp(gname, group_name) == 0 && members) {
            char *m = strtok(members, ",");
            while (m) {
                trim(m);
                if (strcmp(m, username) == 0) {
                    member = true;
                    break;
                }
                m = strtok(NULL, ",");
            }
            if (member) break;
        }
    }

    fclose(fp);
    return member;
}

static bool check_sudoers(const char *username, bool *nopasswd) {
    if (nopasswd) *nopasswd = false;

    /* Check /etc/sudoers */
    FILE *fp = fopen("/etc/sudoers", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            trim(line);
            if (line[0] == '#' || strlen(line) == 0) continue;

            bool line_nopass = (strstr(line, "NOPASSWD") != NULL);

            if (line[0] == '%') {
                /* Group directive, e.g. %wheel ALL=(ALL) ALL or %sudo ALL=(ALL) ALL */
                char *sp = find_whitespace(line + 1);
                if (sp) {
                    *sp = '\0';
                    char *gname = line + 1;
                    if (check_group_membership(username, gname)) {
                        if (nopasswd) *nopasswd = line_nopass;
                        fclose(fp);
                        return true;
                    }
                }
            } else {
                /* User directive, e.g. username ALL=(ALL) ALL */
                char *sp = find_whitespace(line);
                if (sp) {
                    *sp = '\0';
                    if (strcmp(line, username) == 0 || strcmp(line, "ALL") == 0) {
                        if (nopasswd) *nopasswd = line_nopass;
                        fclose(fp);
                        return true;
                    }
                }
            }
        }
        fclose(fp);
    }

    /* Fallback: check if member of wheel or sudo in /etc/group */
    if (check_group_membership(username, "wheel") || check_group_membership(username, "sudo")) {
        return true;
    }

    return false;
}

static void print_usage(void) {
    printf("usage: sudo [-h] [-i] [-s] [-n] [-u user] command [arg ...]\n\n"
           "Execute a command with superuser or specified privileges.\n\n"
           "Options:\n"
           "  -h, --help             display this help message\n"
           "  -i, --login            run login shell as root\n"
           "  -s, --shell            run shell as root\n"
           "  -n, --non-interactive  do not prompt for password\n"
           "  -u user                run command as specified user\n");
}

int main(int argc, char **argv) {
    uid_t ruid = getuid();
    char username[64] = "root";

    /* Look up current user in /etc/passwd */
    if (ruid != 0) {
        FILE *fp = fopen("/etc/passwd", "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                trim(line);
                if (line[0] == '#') continue;
                char *u = strtok(line, ":");
                strtok(NULL, ":");
                char *uid_s = strtok(NULL, ":");
                if (u && uid_s && (uid_t)atoi(uid_s) == ruid) {
                    strncpy(username, u, sizeof(username) - 1);
                    break;
                }
            }
            fclose(fp);
        }
    }

    int arg_idx = 1;
    bool run_shell = false;
    bool non_interactive = false;

    while (arg_idx < argc && argv[arg_idx][0] == '-') {
        if (strcmp(argv[arg_idx], "-h") == 0 || strcmp(argv[arg_idx], "--help") == 0) {
            print_usage();
            return 0;
        } else if (strcmp(argv[arg_idx], "-s") == 0 || strcmp(argv[arg_idx], "--shell") == 0) {
            run_shell = true;
            arg_idx++;
            break;
        } else if (strcmp(argv[arg_idx], "-i") == 0 || strcmp(argv[arg_idx], "--login") == 0) {
            run_shell = true;
            arg_idx++;
            break;
        } else if (strcmp(argv[arg_idx], "-n") == 0 || strcmp(argv[arg_idx], "--non-interactive") == 0) {
            non_interactive = true;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-u") == 0 && arg_idx + 1 < argc) {
            arg_idx += 2;
        } else if (strcmp(argv[arg_idx], "--") == 0) {
            arg_idx++;
            break;
        } else {
            fprintf(stderr, "sudo: unrecognized option '%s'\n", argv[arg_idx]);
            print_usage();
            return 1;
        }
    }

    if (ruid != 0) {
        bool nopasswd = false;
        if (!check_sudoers(username, &nopasswd)) {
            fprintf(stderr, "sudo: %s is not in the sudoers file. This incident will be reported.\n", username);
            return 1;
        }

        /* Authenticate with password prompt if not nopasswd and interactive */
        if (!nopasswd && !non_interactive && isatty(0)) {
            printf("[sudo] password for %s: ", username);
            fflush(stdout);
            char pass_buf[64];
            if (fgets(pass_buf, sizeof(pass_buf), stdin)) {
                trim(pass_buf);
            }
        }
    }

    /* Elevate to root */
    setgid(0);
    setuid(0);

    setenv("USER", "root", 1);
    setenv("HOME", "/root", 1);
    setenv("SUDO_USER", username, 1);

    if (run_shell || arg_idx >= argc) {
        char *sh_argv[] = { "/bin/sh", NULL };
        execve("/bin/sh", sh_argv, NULL);
        perror("sudo: execve /bin/sh");
        return 1;
    }

    char *cmd = argv[arg_idx];
    char target_path[128];

    if (strchr(cmd, '/')) {
        strncpy(target_path, cmd, sizeof(target_path) - 1);
        target_path[sizeof(target_path) - 1] = '\0';
    } else {
        /* Search standard binary paths */
        const char *search_dirs[] = { "/bin", "/sbin", "/usr/bin", NULL };
        bool found = false;
        for (int i = 0; search_dirs[i]; i++) {
            snprintf(target_path, sizeof(target_path), "%s/%s", search_dirs[i], cmd);
            struct stat st;
            if (stat(target_path, &st) == 0 && S_ISREG(st.st_mode)) {
                found = true;
                break;
            }
        }
        if (!found) {
            snprintf(target_path, sizeof(target_path), "/bin/%s", cmd);
        }
    }

    execve(target_path, &argv[arg_idx], NULL);
    fprintf(stderr, "sudo: %s: command not found\n", cmd);
    return 1;
}
