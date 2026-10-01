#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: chown <uid[:gid]> <file...>\n");
        return 1;
    }

    uid_t uid = 0;
    gid_t gid = 0;

    char *colon = strchr(argv[1], ':');
    if (colon) {
        *colon = '\0';
        uid = (uid_t)atoi(argv[1]);
        gid = (gid_t)atoi(colon + 1);
    } else {
        uid = (uid_t)atoi(argv[1]);
        gid = (gid_t)-1;
    }

    int status = 0;
    for (int i = 2; i < argc; i++) {
        if (chown(argv[i], uid, gid) != 0) {
            fprintf(stderr, "chown: cannot change ownership of '%s'\n", argv[i]);
            status = 1;
        }
    }

    return status;
}
