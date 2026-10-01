#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTION]... [+FORMAT]\n", prog);
    printf("Display the current date and time in the given FORMAT, or default format.\n\n");
    printf("Options:\n");
    printf("  -u, --utc, --universal  Print Coordinated Universal Time (UTC)\n");
    printf("  -R, --rfc-2822          Output date and time in RFC 2822 format\n");
    printf("  --help                  Display this help and exit\n\n");
    printf("Format controls:\n");
    printf("  %%Y  4-digit year       %%m  month (01..12)    %%d  day of month (01..31)\n");
    printf("  %%H  hour (00..23)      %%M  minute (00..59)   %%S  second (00..59)\n");
    printf("  %%F  full date (%%Y-%%m-%%d)\n");
    printf("  %%T  time (%%H:%%M:%%S)\n");
    printf("  %%%%  a literal %%\n");
}

int main(int argc, char **argv) {
    const char *format = NULL;
    bool rfc2822 = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--utc") == 0 || strcmp(argv[i], "--universal") == 0) {
            /* UTC is default in DUnix */
        } else if (strcmp(argv[i], "-R") == 0 || strcmp(argv[i], "--rfc-2822") == 0) {
            rfc2822 = true;
        } else if (argv[i][0] == '+') {
            format = argv[i] + 1;
        } else {
            fprintf(stderr, "%s: unrecognized option '%s'\n", argv[0], argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    if (!tm) {
        fprintf(stderr, "%s: failed to get current time\n", argv[0]);
        return 1;
    }

    char out[256];
    if (rfc2822) {
        /* RFC 2822: Thu, 24 Sep 2026 17:45:00 +0000 */
        strftime(out, sizeof(out), "%a, %d %b %Y %T +0000", tm);
        printf("%s\n", out);
    } else if (format) {
        strftime(out, sizeof(out), format, tm);
        printf("%s\n", out);
    } else {
        /* Default standard Unix date output: Thu Sep 24 17:45:00 UTC 2026 */
        strftime(out, sizeof(out), "%a %b %e %T %Z %Y", tm);
        printf("%s\n", out);
    }

    return 0;
}
