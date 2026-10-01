#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>

/* ASCII Art Logos */

static const char *logo_dunix[] = {
    "  .-------------.",
    "  |  _________  |\\",
    "  | |         \\ | \\",
    "  | |   ___     |  \\",
    "  | |  |   \\    |   |",
    "  | |  |    |   |   |",
    "  | |  |    |   |   |",
    "  | |  |___/    |   |",
    "  | |          /|  /",
    "  | |_________/ | /",
    "  |_____________|/"
};

static const char *logo_arch[] = {
    "       /\\",
    "      /  \\",
    "     /\\   \\",
    "    /      \\",
    "   /   ,,   \\",
    "  /   |  |  -\\",
    " /_-''    ''-_\\"
};

static const char *logo_debian[] = {
    "  ,---._",
    " /    __ \\",
    "|   /    |",
    "|  |     |",
    " \\  \\___/",
    "  `---'"
};

static const char *logo_ubuntu[] = {
    "         _",
    "     ---(_)",
    " _/  ---  \\",
    "(_) |   |",
    "  \\  --- _/",
    "     ---(_)"
};

static const char *logo_freebsd[] = {
    " /\\,-'''''-,/\\",
    " \\_)       (_/",
    " |           |",
    " |           |",
    "  ;         ;",
    "   '-_____-'"
};

static const char *logo_generic[] = {
    "   .---.",
    "  /     \\",
    " | () () |",
    "  \\  _  /",
    "   /   \\",
    "  /|   |\\",
    " (_|   |_)"
};

struct distro_entry {
    const char *name;
    const char **lines;
    int count;
    const char *color;
};

static const struct distro_entry distros[] = {
    { "dunix",   logo_dunix,   (int)(sizeof(logo_dunix) / sizeof(logo_dunix[0])),     "\033[1;36m" },
    { "arch",    logo_arch,    (int)(sizeof(logo_arch) / sizeof(logo_arch[0])),       "\033[1;36m" },
    { "debian",  logo_debian,  (int)(sizeof(logo_debian) / sizeof(logo_debian[0])),   "\033[1;31m" },
    { "ubuntu",  logo_ubuntu,  (int)(sizeof(logo_ubuntu) / sizeof(logo_ubuntu[0])),   "\033[1;33m" },
    { "freebsd", logo_freebsd, (int)(sizeof(logo_freebsd) / sizeof(logo_freebsd[0])), "\033[1;31m" },
    { "generic", logo_generic, (int)(sizeof(logo_generic) / sizeof(logo_generic[0])), "\033[1;32m" }
};

/* Dynamic Probing Subroutines */

static void get_user_host(char *user, size_t ulen, char *host, size_t hlen) {
    const char *u = getenv("USER");
    if (!u || !*u) u = getenv("LOGNAME");
    if (u && *u) {
        snprintf(user, ulen, "%s", u);
    } else {
        uid_t uid = getuid();
        if (uid == 0) {
            snprintf(user, ulen, "root");
        } else {
            snprintf(user, ulen, "user%u", (unsigned int)uid);
        }
    }

    struct utsname uts;
    if (uname(&uts) == 0 && uts.nodename[0]) {
        snprintf(host, hlen, "%s", uts.nodename);
    } else {
        int fd = open("/etc/hostname", O_RDONLY, 0);
        if (fd >= 0) {
            char buf[64];
            ssize_t n = read(fd, buf, sizeof(buf) - 1);
            close(fd);
            if (n > 0) {
                buf[n] = '\0';
                char *nl = strchr(buf, '\n');
                if (nl) *nl = '\0';
                snprintf(host, hlen, "%s", buf);
                return;
            }
        }
        snprintf(host, hlen, "localhost");
    }
}

static void get_os(char *out, size_t len) {
    char name[64] = {0};
    char ver[64] = {0};
    char pretty[128] = {0};

    int fd = open("/etc/os-release", O_RDONLY, 0);
    if (fd >= 0) {
        char buf[512];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            char *line = strtok(buf, "\n");
            while (line) {
                if (strncmp(line, "PRETTY_NAME=", 12) == 0) {
                    const char *val = line + 12;
                    if (*val == '"') val++;
                    snprintf(pretty, sizeof(pretty), "%s", val);
                    char *endq = strrchr(pretty, '"');
                    if (endq) *endq = '\0';
                } else if (strncmp(line, "NAME=", 5) == 0 && !name[0]) {
                    const char *val = line + 5;
                    if (*val == '"') val++;
                    snprintf(name, sizeof(name), "%s", val);
                    char *endq = strrchr(name, '"');
                    if (endq) *endq = '\0';
                } else if (strncmp(line, "VERSION=", 8) == 0 && !ver[0]) {
                    const char *val = line + 8;
                    if (*val == '"') val++;
                    snprintf(ver, sizeof(ver), "%s", val);
                    char *endq = strrchr(ver, '"');
                    if (endq) *endq = '\0';
                }
                line = strtok(NULL, "\n");
            }
        }
    }

    struct utsname uts;
    const char *arch = "x86_64";
    if (uname(&uts) == 0 && uts.machine[0]) {
        arch = uts.machine;
    }

    if (pretty[0]) {
        snprintf(out, len, "%s %s", pretty, arch);
    } else if (name[0]) {
        if (ver[0]) {
            snprintf(out, len, "%s %s %s", name, ver, arch);
        } else {
            snprintf(out, len, "%s %s", name, arch);
        }
    } else {
        if (uname(&uts) == 0 && uts.sysname[0]) {
            snprintf(out, len, "%s %s", uts.sysname, arch);
        } else {
            snprintf(out, len, "DUnix %s", arch);
        }
    }
}

static void get_host(char *out, size_t len) {
    char prod[128] = {0};
    char vendor[128] = {0};

    int fd = open("/sys/class/dmi/id/product_name", O_RDONLY, 0);
    if (fd >= 0) {
        ssize_t n = read(fd, prod, sizeof(prod) - 1);
        close(fd);
        if (n > 0) {
            prod[n] = '\0';
            char *nl = strchr(prod, '\n');
            if (nl) *nl = '\0';
        }
    }

    fd = open("/sys/class/dmi/id/sys_vendor", O_RDONLY, 0);
    if (fd >= 0) {
        ssize_t n = read(fd, vendor, sizeof(vendor) - 1);
        close(fd);
        if (n > 0) {
            vendor[n] = '\0';
            char *nl = strchr(vendor, '\n');
            if (nl) *nl = '\0';
        }
    }

    if (!prod[0] && !vendor[0]) {
        fd = open("/proc/dmi", O_RDONLY, 0);
        if (fd >= 0) {
            char buf[512];
            ssize_t n = read(fd, buf, sizeof(buf) - 1);
            close(fd);
            if (n > 0) {
                buf[n] = '\0';
                char *line = strtok(buf, "\n");
                while (line) {
                    if (strncmp(line, "product_name:", 13) == 0) {
                        char *p = line + 13;
                        while (*p == ' ' || *p == '\t') p++;
                        snprintf(prod, sizeof(prod), "%s", p);
                    } else if (strncmp(line, "sys_vendor:", 11) == 0) {
                        char *p = line + 11;
                        while (*p == ' ' || *p == '\t') p++;
                        snprintf(vendor, sizeof(vendor), "%s", p);
                    }
                    line = strtok(NULL, "\n");
                }
            }
        }
    }

    char *p = prod;
    while (*p == ' ') p++;
    char *v = vendor;
    while (*v == ' ') v++;

    if (p[0] && v[0]) {
        if (strstr(p, v) != NULL) {
            snprintf(out, len, "%s", p);
        } else {
            snprintf(out, len, "%s %s", v, p);
        }
    } else if (p[0]) {
        snprintf(out, len, "%s", p);
    } else if (v[0]) {
        snprintf(out, len, "%s", v);
    } else {
        struct utsname uts;
        if (uname(&uts) == 0 && uts.machine[0]) {
            snprintf(out, len, "%s Machine", uts.machine);
        } else {
            snprintf(out, len, "PC Compatible");
        }
    }
}

static void get_kernel(char *out, size_t len) {
    struct utsname uts;
    if (uname(&uts) == 0 && uts.release[0]) {
        snprintf(out, len, "%s", uts.release);
    } else {
        int fd = open("/proc/version", O_RDONLY, 0);
        if (fd >= 0) {
            char buf[128];
            ssize_t n = read(fd, buf, sizeof(buf) - 1);
            close(fd);
            if (n > 0) {
                buf[n] = '\0';
                char *ver = strstr(buf, "version ");
                if (ver) {
                    ver += 8;
                    char *sp = strchr(ver, ' ');
                    if (sp) *sp = '\0';
                    snprintf(out, len, "%s", ver);
                    return;
                }
            }
        }
        snprintf(out, len, "unknown");
    }
}

static void get_uptime(char *out, size_t len) {
    int fd = open("/proc/uptime", O_RDONLY, 0);
    int uptime_sec = 0;
    if (fd >= 0) {
        char buf[64];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            uptime_sec = atoi(buf);
        }
    }

    int days = uptime_sec / 86400;
    int hours = (uptime_sec % 86400) / 3600;
    int mins = (uptime_sec % 3600) / 60;
    int secs = uptime_sec % 60;

    if (days > 0) {
        snprintf(out, len, "%d days, %d hours, %d mins", days, hours, mins);
    } else if (hours > 0) {
        snprintf(out, len, "%d hours, %d mins", hours, mins);
    } else if (mins > 0) {
        snprintf(out, len, "%d mins, %d secs", mins, secs);
    } else {
        snprintf(out, len, "%d secs", secs);
    }
}

static int count_dir_entries(const char *path) {
    DIR *d = opendir(path);
    if (!d) return 0;
    int count = 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        count++;
    }
    closedir(d);
    return count;
}

static void get_packages(char *out, size_t len) {
    int count = count_dir_entries("/bin");
    count += count_dir_entries("/sbin");
    count += count_dir_entries("/usr/bin");
    snprintf(out, len, "%d (bin)", count);
}

static void get_shell(char *out, size_t len) {
    const char *sh = getenv("SHELL");
    if (sh && *sh) {
        const char *base = strrchr(sh, '/');
        if (base) base++;
        else base = sh;
        snprintf(out, len, "%s (%s)", base, sh);
    } else {
        snprintf(out, len, "sh (/bin/sh)");
    }
}

static void get_resolution(char *out, size_t len) {
    int fb = open("/dev/fb0", O_RDONLY, 0);
    if (fb >= 0) {
        struct fb_var_screeninfo vinfo;
        if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) == 0 && vinfo.xres > 0 && vinfo.yres > 0) {
            snprintf(out, len, "%ux%u", vinfo.xres, vinfo.yres);
            close(fb);
            return;
        }
        close(fb);
    }

    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        if (ws.ws_xpixel > 0 && ws.ws_ypixel > 0) {
            snprintf(out, len, "%ux%u", ws.ws_xpixel, ws.ws_ypixel);
        } else {
            snprintf(out, len, "%ux%u (chars)", ws.ws_col, ws.ws_row);
        }
        return;
    }

    snprintf(out, len, "80x25");
}

static void get_wm(char *out, size_t len) {
    DIR *d = opendir("/proc");
    bool found = false;
    if (d) {
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (de->d_name[0] >= '1' && de->d_name[0] <= '9') {
                char path[64];
                snprintf(path, sizeof(path), "/proc/%s/cmdline", de->d_name);
                int fd = open(path, O_RDONLY, 0);
                if (fd >= 0) {
                    char cmd[64];
                    ssize_t n = read(fd, cmd, sizeof(cmd) - 1);
                    close(fd);
                    if (n > 0) {
                        cmd[n] = '\0';
                        char *nl = strchr(cmd, '\n');
                        if (nl) *nl = '\0';
                        if (strstr(cmd, "dws") != NULL) {
                            snprintf(out, len, "DWS");
                            found = true;
                            break;
                        } else if (strstr(cmd, "twm") != NULL) {
                            snprintf(out, len, "twm");
                            found = true;
                            break;
                        } else if (strstr(cmd, "openbox") != NULL) {
                            snprintf(out, len, "Openbox");
                            found = true;
                            break;
                        } else if (strstr(cmd, "i3") != NULL) {
                            snprintf(out, len, "i3");
                            found = true;
                            break;
                        }
                    }
                }
            }
        }
        closedir(d);
    }

    if (!found) {
        if (getenv("DISPLAY")) {
            snprintf(out, len, "X11");
        } else {
            snprintf(out, len, "None");
        }
    }
}

static void get_terminal(char *out, size_t len) {
    const char *term_program = getenv("TERM_PROGRAM");
    const char *term = getenv("TERM");
    if (term_program && *term_program) {
        snprintf(out, len, "%s", term_program);
    } else if (term && *term) {
        snprintf(out, len, "%s", term);
    } else {
        snprintf(out, len, "/dev/tty");
    }
}

static void get_cpu(char *out, size_t len) {
    int fd = open("/proc/cpuinfo", O_RDONLY, 0);
    char model[128] = {0};
    char mhz[32] = {0};
    char cores[16] = {0};

    if (fd >= 0) {
        char buf[2048];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            char *line = strtok(buf, "\n");
            while (line) {
                if (strncmp(line, "model name", 10) == 0 && !model[0]) {
                    char *col = strchr(line, ':');
                    if (col) {
                        col++;
                        while (*col == ' ' || *col == '\t') col++;
                        snprintf(model, sizeof(model), "%s", col);
                    }
                } else if (strncmp(line, "cpu MHz", 7) == 0 && !mhz[0]) {
                    char *col = strchr(line, ':');
                    if (col) {
                        col++;
                        while (*col == ' ' || *col == '\t') col++;
                        snprintf(mhz, sizeof(mhz), "%s", col);
                    }
                } else if (strncmp(line, "cpu cores", 9) == 0 && !cores[0]) {
                    char *col = strchr(line, ':');
                    if (col) {
                        col++;
                        while (*col == ' ' || *col == '\t') col++;
                        snprintf(cores, sizeof(cores), "%s", col);
                    }
                }
                line = strtok(NULL, "\n");
            }
        }
    }

    if (model[0]) {
        char clean[128];
        size_t ci = 0;
        bool in_space = false;
        for (size_t i = 0; model[i] && ci < sizeof(clean) - 1; i++) {
            if (model[i] == ' ') {
                if (!in_space) clean[ci++] = ' ';
                in_space = true;
            } else {
                clean[ci++] = model[i];
                in_space = false;
            }
        }
        clean[ci] = '\0';
        while (ci > 0 && clean[ci - 1] == ' ') clean[--ci] = '\0';

        if (cores[0] && mhz[0]) {
            char *dot = strchr(mhz, '.');
            if (dot) *dot = '\0';
            snprintf(out, len, "%s (%s) @ %sMHz", clean, cores, mhz);
        } else if (cores[0]) {
            snprintf(out, len, "%s (%s)", clean, cores);
        } else {
            snprintf(out, len, "%s", clean);
        }
    } else {
        struct utsname uts;
        if (uname(&uts) == 0 && uts.machine[0]) {
            snprintf(out, len, "%s Processor", uts.machine);
        } else {
            snprintf(out, len, "Generic x86_64 CPU");
        }
    }
}

static void get_gpu(char *out, size_t len) {
    int fd = open("/proc/pci", O_RDONLY, 0);
    if (fd >= 0) {
        char buf[2048];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            char *line = strtok(buf, "\n");
            while (line) {
                if (strstr(line, "Display Controller") != NULL ||
                    strstr(line, "VGA Compatible") != NULL ||
                    strstr(line, "Graphics") != NULL ||
                    strstr(line, "SVGA") != NULL ||
                    strstr(line, "GPU") != NULL) {
                    char *dash = strstr(line, " - ");
                    if (dash) {
                        dash += 3;
                        while (*dash == ' ') dash++;
                        snprintf(out, len, "%s", dash);
                        return;
                    }
                }
                line = strtok(NULL, "\n");
            }
        }
    }
    snprintf(out, len, "None");
}

static void get_memory(char *out, size_t len) {
    int fd = open("/proc/meminfo", O_RDONLY, 0);
    unsigned long total_kb = 0;
    unsigned long free_kb = 0;
    unsigned long avail_kb = 0;

    if (fd >= 0) {
        char buf[1024];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            char *line = strtok(buf, "\n");
            while (line) {
                if (strncmp(line, "MemTotal:", 9) == 0) {
                    char *p = line + 9;
                    while (*p == ' ' || *p == '\t') p++;
                    total_kb = (unsigned long)atol(p);
                } else if (strncmp(line, "MemAvailable:", 13) == 0) {
                    char *p = line + 13;
                    while (*p == ' ' || *p == '\t') p++;
                    avail_kb = (unsigned long)atol(p);
                } else if (strncmp(line, "MemFree:", 8) == 0 && avail_kb == 0) {
                    char *p = line + 8;
                    while (*p == ' ' || *p == '\t') p++;
                    free_kb = (unsigned long)atol(p);
                }
                line = strtok(NULL, "\n");
            }
        }
    }

    if (avail_kb == 0) avail_kb = free_kb;
    unsigned long used_kb = (total_kb > avail_kb) ? (total_kb - avail_kb) : 0;
    unsigned long used_mib = used_kb / 1024;
    unsigned long total_mib = total_kb / 1024;
    unsigned int pct = total_mib ? (unsigned int)((used_mib * 100) / total_mib) : 0;

    snprintf(out, len, "%luMiB / %luMiB (%u%%)", used_mib, total_mib, pct);
}

static void get_disk(char *out, size_t len) {
    int fd = open("/proc/mounts", O_RDONLY, 0);
    char root_dev[64] = {0};
    if (fd >= 0) {
        char buf[512];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            char *line = strtok(buf, "\n");
            while (line) {
                char *dev = line;
                char *sp1 = strchr(dev, ' ');
                if (sp1) {
                    *sp1 = '\0';
                    char *mnt = sp1 + 1;
                    while (*mnt == ' ') mnt++;
                    char *sp2 = strchr(mnt, ' ');
                    if (sp2) {
                        *sp2 = '\0';
                        char *type = sp2 + 1;
                        while (*type == ' ') type++;
                        char *sp3 = strchr(type, ' ');
                        if (sp3) *sp3 = '\0';
                        if (strcmp(mnt, "/") == 0) {
                            snprintf(root_dev, sizeof(root_dev), "%s (%s)", dev, type);
                            break;
                        }
                    }
                }
                line = strtok(NULL, "\n");
            }
        }
    }

    if (root_dev[0]) {
        snprintf(out, len, "%s", root_dev);
    } else {
        snprintf(out, len, "ramfs (/)");
    }
}

static void print_help(void) {
    printf("Neofetch 7.1.0 (DUnix native port)\n\n"
           "Usage: neofetch [options]\n\n"
           "Options:\n"
           "    --help, -h               Show this help message and exit\n"
           "    --version, -v            Show version information and exit\n"
           "    --stdout                 Turn off colors and ASCII art (plain text output)\n"
           "    --off                    Turn off ASCII art\n"
           "    --ascii_distro <distro>  Which distro's ASCII art to print\n"
           "                             Available: dunix, arch, debian, ubuntu, freebsd, generic\n\n");
}

int main(int argc, char **argv) {
    bool opt_stdout = false;
    bool opt_off = false;
    const char *opt_distro = "dunix";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            return 0;
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("Neofetch 7.1.0 (DUnix native port)\n");
            return 0;
        } else if (strcmp(argv[i], "--stdout") == 0) {
            opt_stdout = true;
        } else if (strcmp(argv[i], "--off") == 0) {
            opt_off = true;
        } else if (strcmp(argv[i], "--ascii_distro") == 0) {
            if (i + 1 < argc) {
                opt_distro = argv[++i];
            } else {
                fprintf(stderr, "neofetch: option '--ascii_distro' requires an argument\n");
                return 1;
            }
        } else {
            fprintf(stderr, "neofetch: unrecognized option '%s'\n", argv[i]);
            fprintf(stderr, "Try 'neofetch --help' for more information.\n");
            return 1;
        }
    }

    const struct distro_entry *sel = &distros[0];
    for (size_t d = 0; d < sizeof(distros) / sizeof(distros[0]); d++) {
        if (strcasecmp(opt_distro, distros[d].name) == 0) {
            sel = &distros[d];
            break;
        }
    }

    char user[64];
    char host[64];
    get_user_host(user, sizeof(user), host, sizeof(host));

    char title[128];
    snprintf(title, sizeof(title), "%s@%s", user, host);

    char separator[128];
    size_t tlen = strlen(title);
    if (tlen > sizeof(separator) - 1) tlen = sizeof(separator) - 1;
    for (size_t s = 0; s < tlen; s++) {
        separator[s] = '-';
    }
    separator[tlen] = '\0';

    struct {
        const char *label;
        char value[256];
    } metrics[16];
    int num_metrics = 0;

    metrics[num_metrics].label = "OS";
    get_os(metrics[num_metrics].value, sizeof(metrics[num_metrics].value));
    num_metrics++;

    metrics[num_metrics].label = "Host";
    get_host(metrics[num_metrics].value, sizeof(metrics[num_metrics].value));
    num_metrics++;

    metrics[num_metrics].label = "Kernel";
    get_kernel(metrics[num_metrics].value, sizeof(metrics[num_metrics].value));
    num_metrics++;

    metrics[num_metrics].label = "Uptime";
    get_uptime(metrics[num_metrics].value, sizeof(metrics[num_metrics].value));
    num_metrics++;

    metrics[num_metrics].label = "Packages";
    get_packages(metrics[num_metrics].value, sizeof(metrics[num_metrics].value));
    num_metrics++;

    metrics[num_metrics].label = "Shell";
    get_shell(metrics[num_metrics].value, sizeof(metrics[num_metrics].value));
    num_metrics++;

    metrics[num_metrics].label = "Resolution";
    get_resolution(metrics[num_metrics].value, sizeof(metrics[num_metrics].value));
    num_metrics++;

    metrics[num_metrics].label = "WM";
    get_wm(metrics[num_metrics].value, sizeof(metrics[num_metrics].value));
    num_metrics++;

    metrics[num_metrics].label = "Terminal";
    get_terminal(metrics[num_metrics].value, sizeof(metrics[num_metrics].value));
    num_metrics++;

    metrics[num_metrics].label = "CPU";
    get_cpu(metrics[num_metrics].value, sizeof(metrics[num_metrics].value));
    num_metrics++;

    metrics[num_metrics].label = "GPU";
    get_gpu(metrics[num_metrics].value, sizeof(metrics[num_metrics].value));
    num_metrics++;

    metrics[num_metrics].label = "Memory";
    get_memory(metrics[num_metrics].value, sizeof(metrics[num_metrics].value));
    num_metrics++;

    metrics[num_metrics].label = "Disk (/)";
    get_disk(metrics[num_metrics].value, sizeof(metrics[num_metrics].value));
    num_metrics++;

    if (opt_stdout) {
        printf("%s\n", title);
        printf("%s\n", separator);
        for (int m = 0; m < num_metrics; m++) {
            if (metrics[m].value[0]) {
                printf("%s: %s\n", metrics[m].label, metrics[m].value);
            }
        }
        return 0;
    }

    char info_lines[32][512];
    int total_info_lines = 0;

    snprintf(info_lines[total_info_lines++], 512,
             "\033[1;37m%s\033[0m@%s%s\033[0m", user, sel->color, host);
    snprintf(info_lines[total_info_lines++], 512,
             "%s", separator);

    for (int m = 0; m < num_metrics; m++) {
        if (metrics[m].value[0]) {
            snprintf(info_lines[total_info_lines++], 512,
                     "%s%s\033[0m: %s", sel->color, metrics[m].label, metrics[m].value);
        }
    }

    snprintf(info_lines[total_info_lines++], 512, "");

    /* 16-color ANSI Palette Blocks */
    snprintf(info_lines[total_info_lines++], 512,
             "\033[40m   \033[41m   \033[42m   \033[43m   \033[44m   \033[45m   \033[46m   \033[47m   \033[0m");
    snprintf(info_lines[total_info_lines++], 512,
             "\033[100m   \033[101m   \033[102m   \033[103m   \033[104m   \033[105m   \033[106m   \033[107m   \033[0m");

    size_t logo_width = 0;
    if (!opt_off) {
        for (int i = 0; i < sel->count; i++) {
            size_t l = strlen(sel->lines[i]);
            if (l > logo_width) logo_width = l;
        }
    }

    int max_lines = (!opt_off && sel->count > total_info_lines) ? sel->count : total_info_lines;

    for (int line = 0; line < max_lines; line++) {
        if (!opt_off) {
            if (line < sel->count) {
                const char *lstr = sel->lines[line];
                size_t llen = strlen(lstr);
                printf("%s%s\033[0m", sel->color, lstr);
                int pad = (int)(logo_width - llen + 3);
                for (int s = 0; s < pad; s++) putchar(' ');
            } else {
                int pad = (int)(logo_width + 3);
                for (int s = 0; s < pad; s++) putchar(' ');
            }
        }

        if (line < total_info_lines) {
            printf("%s", info_lines[line]);
        }
        printf("\n");
    }
    printf("\n");

    return 0;
}
