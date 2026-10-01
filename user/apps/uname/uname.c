#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/utsname.h>

int main(int argc, char **argv) {
    bool all = false;
    bool sysname = false;
    bool nodename = false;
    bool release = false;
    bool version = false;
    bool machine = false;

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (char *p = argv[i] + 1; *p; p++) {
                if (*p == 'a') all = true;
                else if (*p == 's') sysname = true;
                else if (*p == 'n') nodename = true;
                else if (*p == 'r') release = true;
                else if (*p == 'v') version = true;
                else if (*p == 'm') machine = true;
            }
        }
    }

    if (all) {
        sysname = nodename = release = version = machine = true;
    } else if (!sysname && !nodename && !release && !version && !machine) {
        sysname = true;
    }

    struct utsname u;
    if (uname(&u) != 0) {
        fprintf(stderr, "uname: system call failed\n");
        return 1;
    }

    bool space = false;
    if (sysname)  { printf("%s", u.sysname);  space = true; }
    if (nodename) { printf("%s%s", space ? " " : "", u.nodename); space = true; }
    if (release)  { printf("%s%s", space ? " " : "", u.release);  space = true; }
    if (version)  { printf("%s%s", space ? " " : "", u.version);  space = true; }
    if (machine)  { printf("%s%s", space ? " " : "", u.machine);  space = true; }
    printf("\n");

    return 0;
}
