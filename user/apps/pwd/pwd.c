#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    char buf[256];
    if (getcwd(buf, sizeof(buf))) {
        printf("%s\n", buf);
        return 0;
    }

    fprintf(stderr, "pwd: error getting current working directory\n");
    return 1;
}
