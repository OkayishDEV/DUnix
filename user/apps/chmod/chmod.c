#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: chmod <octal-mode> <file...>\n");
        return 1;
    }

    mode_t mode = (mode_t)strtoul(argv[1], NULL, 8);
    int status = 0;

    for (int i = 2; i < argc; i++) {
        if (chmod(argv[i], mode) != 0) {
            fprintf(stderr, "chmod: cannot change mode of '%s'\n", argv[i]);
            status = 1;
        }
    }

    return status;
}
