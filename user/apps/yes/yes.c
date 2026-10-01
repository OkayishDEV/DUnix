#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s [STRING]...\n", argv[0]);
        printf("Repeatedly output a line with all specified STRING(s), or 'y'.\n\n");
        printf("      --help     display this help and exit\n");
        return 0;
    }

    char buf[1024];
    if (argc <= 1) {
        strcpy(buf, "y\n");
    } else {
        buf[0] = '\0';
        for (int i = 1; i < argc; i++) {
            if (i > 1) strncat(buf, " ", sizeof(buf) - strlen(buf) - 1);
            strncat(buf, argv[i], sizeof(buf) - strlen(buf) - 1);
        }
        strncat(buf, "\n", sizeof(buf) - strlen(buf) - 1);
    }

    size_t len = strlen(buf);
    for (;;) {
        fwrite(buf, 1, len, stdout);
    }

    return 0;
}
