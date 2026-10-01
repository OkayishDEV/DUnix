#include <unistd.h>

int main(int argc, char **argv) {
    (void)argc;
    argv[0] = (char *)"/bin/dweb";
    execve("/bin/dweb", argv, NULL);
    return 1;
}
