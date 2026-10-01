#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: sleep <seconds>\n");
        return 1;
    }

    int sec = atoi(argv[1]);
    if (sec > 0) {
        sleep(sec);
    }
    return 0;
}
