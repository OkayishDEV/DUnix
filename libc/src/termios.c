#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>

int tcgetattr(int fd, struct termios *termios_p) {
    return ioctl(fd, TCGETS, termios_p);
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    (void)optional_actions;
    return ioctl(fd, TCSETS, termios_p);
}
