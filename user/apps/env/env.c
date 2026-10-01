#include <stdio.h>

extern char **environ;

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    if (environ) {
        for (char **ep = environ; *ep; ep++) {
            printf("%s\n", *ep);
        }
    } else {
        printf("USER=root\nHOME=/root\nPATH=/bin:/usr/bin:/sbin\nTERM=vt100\nSHELL=/bin/sh\n");
    }

    return 0;
}
