#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: basename <path> [suffix]\n");
        return 1;
    }

    const char *path = argv[1];
    const char *last_slash = strrchr(path, '/');
    const char *base = last_slash ? last_slash + 1 : path;

    if (argc > 2) {
        const char *suffix = argv[2];
        size_t base_len = strlen(base);
        size_t suf_len = strlen(suffix);
        if (base_len > suf_len && strcmp(base + base_len - suf_len, suffix) == 0) {
            char trimmed[128];
            strncpy(trimmed, base, base_len - suf_len);
            trimmed[base_len - suf_len] = '\0';
            printf("%s\n", trimmed);
            return 0;
        }
    }

    printf("%s\n", base);
    return 0;
}
