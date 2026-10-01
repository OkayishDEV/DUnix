#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static void get_username(uid_t uid, char *buf, size_t sz) {
    FILE *fp = fopen("/etc/passwd", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            char *colon = strchr(line, ':');
            if (!colon) continue;
            *colon = '\0';
            char *p = colon + 1;
            colon = strchr(p, ':');
            if (!colon) continue;
            p = colon + 1;
            colon = strchr(p, ':');
            if (colon) *colon = '\0';
            if ((uid_t)atoi(p) == uid) {
                strncpy(buf, line, sz - 1);
                buf[sz - 1] = '\0';
                fclose(fp);
                return;
            }
        }
        fclose(fp);
    }
    const char *env_user = getenv("USER");
    if (env_user && env_user[0]) {
        strncpy(buf, env_user, sz - 1);
        buf[sz - 1] = '\0';
        return;
    }
    if (uid == 0) {
        strncpy(buf, "root", sz - 1);
        buf[sz - 1] = '\0';
    } else {
        snprintf(buf, sz, "%u", (unsigned int)uid);
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    char name[64];
    get_username(geteuid(), name, sizeof(name));
    printf("%s\n", name);
    return 0;
}
