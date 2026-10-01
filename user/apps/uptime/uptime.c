#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <stdbool.h>

int main(int argc, char **argv) {
    bool opt_pretty = false;
    bool opt_since = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--pretty") == 0) {
            opt_pretty = true;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--since") == 0) {
            opt_since = true;
        }
    }

    /* Read uptime from /proc/uptime */
    double uptime_sec = 0.0;
    FILE *fp_up = fopen("/proc/uptime", "r");
    if (fp_up) {
        char buf[128];
        if (fgets(buf, sizeof(buf), fp_up)) {
            uptime_sec = atof(buf);
        }
        fclose(fp_up);
    }

    /* Read load average from /proc/loadavg */
    char load_str[64] = "0.00, 0.00, 0.00";
    FILE *fp_load = fopen("/proc/loadavg", "r");
    if (fp_load) {
        char lbuf[128];
        if (fgets(lbuf, sizeof(lbuf), fp_load)) {
            char *p1 = lbuf;
            while (*p1 == ' ') p1++;
            char *p2 = strchr(p1, ' ');
            if (p2) {
                *p2 = '\0';
                char *p3 = p2 + 1;
                while (*p3 == ' ') p3++;
                char *p4 = strchr(p3, ' ');
                if (p4) {
                    *p4 = '\0';
                    char *p5 = p4 + 1;
                    while (*p5 == ' ') p5++;
                    char *p6 = strchr(p5, ' ');
                    if (p6) *p6 = '\0';
                    snprintf(load_str, sizeof(load_str), "%s, %s, %s", p1, p3, p5);
                }
            }
        }
        fclose(fp_load);
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t cur_sec = tv.tv_sec;
    time_t boot_sec = (cur_sec > (time_t)uptime_sec) ? (cur_sec - (time_t)uptime_sec) : cur_sec;

    if (opt_since) {
        struct tm *btm = gmtime(&boot_sec);
        if (btm) {
            printf("%04d-%02d-%02d %02d:%02d:%02d\n",
                   btm->tm_year + 1900, btm->tm_mon + 1, btm->tm_mday,
                   btm->tm_hour, btm->tm_min, btm->tm_sec);
        }
        return 0;
    }

    unsigned long total_secs = (unsigned long)uptime_sec;
    unsigned long days = total_secs / 86400;
    unsigned long hours = (total_secs % 86400) / 3600;
    unsigned long minutes = (total_secs % 3600) / 60;

    if (opt_pretty) {
        printf("up ");
        if (days > 0) {
            printf("%lu day%s, ", days, (days == 1) ? "" : "s");
        }
        if (hours > 0 || days > 0) {
            printf("%lu hour%s, ", hours, (hours == 1) ? "" : "s");
        }
        printf("%lu minute%s\n", minutes, (minutes == 1) ? "" : "s");
        return 0;
    }

    struct tm *ctm = gmtime(&cur_sec);
    int chour = ctm ? ctm->tm_hour : 0;
    int cmin = ctm ? ctm->tm_min : 0;
    int csec = ctm ? ctm->tm_sec : 0;

    printf(" %02d:%02d:%02d up ", chour, cmin, csec);
    if (days > 0) {
        printf("%lu day%s, ", days, (days == 1) ? "" : "s");
    }
    if (hours > 0) {
        printf("%2lu:%02lu, ", hours, minutes);
    } else {
        printf("%lu min, ", minutes);
    }
    printf(" 1 user,  load average: %s\n", load_str);

    return 0;
}
