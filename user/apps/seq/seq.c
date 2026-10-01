#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTION]... LAST\n", prog);
    printf("  or:  %s [OPTION]... FIRST LAST\n", prog);
    printf("  or:  %s [OPTION]... FIRST INCREMENT LAST\n", prog);
    printf("Print numbers from FIRST to LAST, in steps of INCREMENT.\n\n");
    printf("      --help     display this help and exit\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    long first = 1;
    long step = 1;
    long last = 1;

    if (argc == 2) {
        last = strtol(argv[1], NULL, 10);
    } else if (argc == 3) {
        first = strtol(argv[1], NULL, 10);
        last = strtol(argv[2], NULL, 10);
    } else if (argc >= 4) {
        first = strtol(argv[1], NULL, 10);
        step = strtol(argv[2], NULL, 10);
        last = strtol(argv[3], NULL, 10);
    }

    if (step == 0) {
        fprintf(stderr, "%s: zero increment\n", argv[0]);
        return 1;
    }

    if (step > 0) {
        for (long i = first; i <= last; i += step) {
            printf("%ld\n", i);
        }
    } else {
        for (long i = first; i >= last; i += step) {
            printf("%ld\n", i);
        }
    }

    return 0;
}
