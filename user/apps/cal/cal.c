#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_leap(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int day_of_week(int d, int m, int y) {
    static int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

int main(int argc, char **argv) {
    int month = 9;
    int year = 2026;

    if (argc > 1) month = atoi(argv[1]);
    if (argc > 2) year = atoi(argv[2]);

    const char *months[] = {
        "", "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    int days_in_month[] = { 0, 31, 28 + is_leap(year), 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    if (month < 1 || month > 12) month = 9;

    printf("    %s %d\n", months[month], year);
    printf("Su Mo Tu We Th Fr Sa\n");

    int start_day = day_of_week(1, month, year);
    for (int i = 0; i < start_day; i++) {
        printf("   ");
    }

    for (int day = 1; day <= days_in_month[month]; day++) {
        printf("%2d ", day);
        if ((day + start_day) % 7 == 0) {
            printf("\n");
        }
    }
    printf("\n");

    return 0;
}
