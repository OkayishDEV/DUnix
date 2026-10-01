#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static void print_escaped(const char *s) {
    while (*s) {
        if (*s == '\\' && *(s + 1)) {
            s++;
            switch (*s) {
                case 'n': putchar('\n'); break;
                case 't': putchar('\t'); break;
                case 'r': putchar('\r'); break;
                case '\\': putchar('\\'); break;
                case '0': putchar('\0'); break;
                default: putchar('\\'); putchar(*s); break;
            }
        } else {
            putchar(*s);
        }
        s++;
    }
}

int main(int argc, char **argv) {
    bool newline = true;
    bool enable_escapes = false;
    int start_idx = 1;

    while (start_idx < argc && argv[start_idx][0] == '-' && argv[start_idx][1] != '\0') {
        const char *opt = argv[start_idx] + 1;
        bool valid = true;
        for (const char *c = opt; *c; c++) {
            if (*c != 'n' && *c != 'e' && *c != 'E') {
                valid = false;
                break;
            }
        }
        if (!valid) break;

        for (const char *c = opt; *c; c++) {
            if (*c == 'n') newline = false;
            else if (*c == 'e') enable_escapes = true;
            else if (*c == 'E') enable_escapes = false;
        }
        start_idx++;
    }

    for (int i = start_idx; i < argc; i++) {
        if (enable_escapes) {
            print_escaped(argv[i]);
        } else {
            fputs(argv[i], stdout);
        }
        if (i < argc - 1) {
            putchar(' ');
        }
    }

    if (newline) {
        putchar('\n');
    }

    return 0;
}
