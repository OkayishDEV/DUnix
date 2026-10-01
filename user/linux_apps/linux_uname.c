/* Freestanding Linux uname binary */
typedef unsigned long  uint64_t;
typedef long           int64_t;
typedef unsigned long  size_t;

#define SYS_write      1
#define SYS_uname      63
#define SYS_exit_group 231

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

static inline int64_t sys1(int64_t n, int64_t a1) {
    int64_t r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a1) : "rcx", "r11", "memory");
    return r;
}

static inline int64_t sys3(int64_t n, int64_t a1, int64_t a2, int64_t a3) {
    int64_t r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
    return r;
}

static size_t slen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static void print(const char *s) {
    sys3(SYS_write, 1, (int64_t)s, slen(s));
}

void _start(void) {
    struct utsname u;
    if (sys1(SYS_uname, (int64_t)&u) == 0) {
        print(u.sysname);
        print(" ");
        print(u.nodename);
        print(" ");
        print(u.release);
        print(" ");
        print(u.version);
        print(" ");
        print(u.machine);
        print("\n");
    }
    sys1(SYS_exit_group, 0);
}
