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
    if (uid == 0) strncpy(buf, "root", sz - 1);
    else snprintf(buf, sz, "%u", (unsigned int)uid);
    buf[sz - 1] = '\0';
}

static void get_groupname(gid_t gid, char *buf, size_t sz) {
    FILE *fp = fopen("/etc/group", "r");
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
            if ((gid_t)atoi(p) == gid) {
                strncpy(buf, line, sz - 1);
                buf[sz - 1] = '\0';
                fclose(fp);
                return;
            }
        }
        fclose(fp);
    }
    if (gid == 0) strncpy(buf, "root", sz - 1);
    else snprintf(buf, sz, "%u", (unsigned int)gid);
    buf[sz - 1] = '\0';
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    uid_t uid = getuid();
    gid_t gid = getgid();
    uid_t euid = geteuid();
    gid_t egid = getegid();

    char uname[64], gname[64];
    get_username(uid, uname, sizeof(uname));
    get_groupname(gid, gname, sizeof(gname));

    printf("uid=%u(%s) gid=%u(%s)", (unsigned int)uid, uname, (unsigned int)gid, gname);
    if (euid != uid) {
        char euname[64];
        get_username(euid, euname, sizeof(euname));
        printf(" euid=%u(%s)", (unsigned int)euid, euname);
    }
    if (egid != gid) {
        char egname[64];
        get_groupname(egid, egname, sizeof(egname));
        printf(" egid=%u(%s)", (unsigned int)egid, egname);
    }
    printf(" groups=%u(%s)\n", (unsigned int)gid, gname);
    return 0;
}
