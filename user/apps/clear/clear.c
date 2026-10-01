#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("\033[2J\033[H");
    fflush(stdout);
    return 0;
}
