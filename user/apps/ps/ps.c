#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

static void get_username_by_uid(int uid, char *buf, size_t len) {
    if (uid == 0) {
        snprintf(buf, len, "root");
        return;
    }
    FILE *fp = fopen("/etc/passwd", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            char *u = strtok(line, ":");
            strtok(NULL, ":");
            char *u_uid = strtok(NULL, ":");
            if (u && u_uid && atoi(u_uid) == uid) {
                snprintf(buf, len, "%s", u);
                fclose(fp);
                return;
            }
        }
        fclose(fp);
    }
    snprintf(buf, len, "%d", uid);
}

static void print_process_info(const char *pid_str) {
    char status_path[64];
    snprintf(status_path, sizeof(status_path), "/proc/%s/status", pid_str);

    int fd = open(status_path, O_RDONLY);
    if (fd < 0) return;

    char buf[512];
    memset(buf, 0, sizeof(buf));
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return;

    char name[64] = "unknown";
    char state[32] = "R";
    int ppid = 0;
    int uid = 0;

    char *line = buf;
    while (line && *line) {
        char *next = strchr(line, '\n');
        if (next) *next = '\0';

        if (strncmp(line, "Name:\t", 6) == 0) {
            strncpy(name, line + 6, sizeof(name) - 1);
        } else if (strncmp(line, "State:\t", 7) == 0) {
            strncpy(state, line + 7, sizeof(state) - 1);
        } else if (strncmp(line, "PPid:\t", 6) == 0) {
            ppid = atoi(line + 6);
        } else if (strncmp(line, "Uid:\t", 5) == 0) {
            uid = atoi(line + 5);
        }

        if (!next) break;
        line = next + 1;
    }

    char user_str[32];
    get_username_by_uid(uid, user_str, sizeof(user_str));
    printf("%5s %5d %-8s %-10s %s\n", pid_str, ppid, user_str, state, name);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    DIR *dir = opendir("/proc");
    if (!dir) {
        fprintf(stderr, "ps: cannot open /proc\n");
        return 1;
    }

    printf("  PID  PPID USER     STATE      COMMAND\n");

    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (isdigit((unsigned char)de->d_name[0])) {
            print_process_info(de->d_name);
        }
    }

    closedir(dir);
    return 0;
}
