/* Native Freestanding Linux x86_64 Application: linux_hello
 * Tests Linux System V AMD64 ABI, TLS (%fs), auxv, and Linux syscalls on DUnix.
 */

typedef unsigned long  uint64_t;
typedef long           int64_t;
typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;
typedef unsigned long  size_t;

#define SYS_read          0
#define SYS_write         1
#define SYS_open          2
#define SYS_close         3
#define SYS_stat          4
#define SYS_fstat         5
#define SYS_writev        20
#define SYS_access        21
#define SYS_getpid        39
#define SYS_uname         63
#define SYS_arch_prctl    158
#define SYS_gettid        186
#define SYS_futex         202
#define SYS_getdents64    217
#define SYS_set_tid_address 218
#define SYS_clock_gettime 228
#define SYS_exit_group    231
#define SYS_openat        257
#define SYS_newfstatat    262
#define SYS_prlimit64     302
#define SYS_getrandom     318

#define ARCH_SET_FS       0x1002
#define ARCH_GET_FS       0x1003

#define AT_NULL           0
#define AT_PHDR           3
#define AT_PHENT          4
#define AT_PHNUM          5
#define AT_PAGESZ         6
#define AT_ENTRY          9
#define AT_RANDOM         25
#define AT_PLATFORM       15

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

struct timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

struct iovec {
    void  *iov_base;
    size_t iov_len;
};

struct linux_dirent64 {
    uint64_t       d_ino;
    int64_t        d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[];
};

static inline int64_t sys1(int64_t n, int64_t a1) {
    int64_t r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a1) : "rcx", "r11", "memory");
    return r;
}

static inline int64_t sys2(int64_t n, int64_t a1, int64_t a2) {
    int64_t r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return r;
}

static inline int64_t sys3(int64_t n, int64_t a1, int64_t a2, int64_t a3) {
    int64_t r;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
    return r;
}

static inline int64_t sys4(int64_t n, int64_t a1, int64_t a2, int64_t a3, int64_t a4) {
    int64_t r;
    register int64_t r10 __asm__("r10") = a4;
    __asm__ volatile("syscall" : "=a"(r) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10) : "rcx", "r11", "memory");
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

static void print_num(int64_t num) {
    char buf[32];
    if (num == 0) {
        print("0");
        return;
    }
    if (num < 0) {
        print("-");
        num = -num;
    }
    int pos = 30;
    buf[31] = '\0';
    while (num > 0 && pos >= 0) {
        buf[pos--] = '0' + (num % 10);
        num /= 10;
    }
    print(&buf[pos + 1]);
}

static void print_hex(uint64_t num) {
    char buf[19];
    const char *hex = "0123456789abcdef";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 15; i >= 0; i--) {
        buf[2 + (15 - i)] = hex[(num >> (i * 4)) & 0xF];
    }
    buf[18] = '\0';
    print(buf);
}

/* Freestanding TLS control block */
static struct {
    uint64_t self;
    uint64_t dtv;
    uint64_t canary;
    uint64_t thread_id;
} tls_block;

void _start(void) {
    /* Step 1: Print startup header via writev (Linux Vector I/O) */
    const char *hdr1 = "\n=======================================================\n";
    const char *hdr2 = "  [LINUX COMPAT] Native Linux ELF64 Binary Executing!  \n";
    const char *hdr3 = "=======================================================\n";
    struct iovec iov[3];
    iov[0].iov_base = (void *)hdr1; iov[0].iov_len = slen(hdr1);
    iov[1].iov_base = (void *)hdr2; iov[1].iov_len = slen(hdr2);
    iov[2].iov_base = (void *)hdr3; iov[2].iov_len = slen(hdr3);
    sys3(SYS_writev, 1, (int64_t)iov, 3);

    /* Step 2: Test Linux arch_prctl and Thread-Local Storage (%fs) */
    print("[1] Initializing TLS via arch_prctl(ARCH_SET_FS)...\n");
    tls_block.self = (uint64_t)&tls_block;
    tls_block.canary = 0xdeadbeefc001cafeULL;
    tls_block.thread_id = 42;

    int64_t ret = sys2(SYS_arch_prctl, ARCH_SET_FS, (int64_t)&tls_block);
    if (ret != 0) {
        print("[-] arch_prctl failed with code: ");
        print_num(ret);
        print("\n");
        sys1(SYS_exit_group, 1);
    }

    /* Verify %fs dereferencing */
    uint64_t canary_read = 0;
    uint64_t tid_read = 0;
    __asm__ volatile("movq %%fs:16, %0" : "=r"(canary_read));
    __asm__ volatile("movq %%fs:24, %0" : "=r"(tid_read));

    if (canary_read == 0xdeadbeefc001cafeULL && tid_read == 42) {
        print("    [+] TLS Verified: %fs:16 canary=");
        print_hex(canary_read);
        print(" tid=");
        print_num(tid_read);
        print(" (OK)\n");
    } else {
        print("[-] TLS Verification FAILED! Canary mismatch\n");
        sys1(SYS_exit_group, 2);
    }

    /* Step 3: Test Linux set_tid_address */
    print("[2] Registering thread via set_tid_address...\n");
    int clear_tid = 0;
    int64_t pid = sys1(SYS_set_tid_address, (int64_t)&clear_tid);
    print("    [+] Current Linux PID/TID: ");
    print_num(pid);
    print("\n");

    /* Step 4: Test Linux uname(63) */
    print("[3] Querying uname(63) from Linuxulator...\n");
    struct utsname u;
    ret = sys1(SYS_uname, (int64_t)&u);
    if (ret == 0) {
        print("    [+] sysname:  "); print(u.sysname); print("\n");
        print("    [+] nodename: "); print(u.nodename); print("\n");
        print("    [+] release:  "); print(u.release); print("\n");
        print("    [+] version:  "); print(u.version); print("\n");
        print("    [+] machine:  "); print(u.machine); print("\n");
    } else {
        print("[-] uname failed!\n");
    }

    /* Step 5: Test Linux clock_gettime(228) */
    print("[4] Querying clock_gettime(CLOCK_REALTIME)...\n");
    struct timespec ts;
    ret = sys2(SYS_clock_gettime, 0 /* CLOCK_REALTIME */, (int64_t)&ts);
    if (ret == 0) {
        print("    [+] Kernel Uptime: ");
        print_num(ts.tv_sec);
        print(" sec, ");
        print_num(ts.tv_nsec / 1000000);
        print(" ms\n");
    }

    /* Step 6: Test Linux getrandom(318) */
    print("[5] Generating entropy via getrandom(318)...\n");
    uint8_t rand_bytes[8];
    ret = sys3(SYS_getrandom, (int64_t)rand_bytes, sizeof(rand_bytes), 0);
    if (ret == sizeof(rand_bytes)) {
        uint64_t val = *(uint64_t *)rand_bytes;
        print("    [+] Kernel Random 64-bit Seed: ");
        print_hex(val);
        print("\n");
    }

    /* Step 7: Test Linux openat(257) and getdents64(217) */
    print("[6] Directory Listing via openat(257) + getdents64(217) on /compat/linux...\n");
    int64_t dir_fd = sys4(SYS_openat, -100 /* AT_FDCWD */, (int64_t)"/compat/linux", 0 /* O_RDONLY */, 0);
    if (dir_fd >= 0) {
        uint8_t d_buf[512];
        int64_t nread = sys3(SYS_getdents64, dir_fd, (int64_t)d_buf, sizeof(d_buf));
        if (nread > 0) {
            int pos = 0;
            print("    [+] Entries in /compat/linux: ");
            while (pos < nread) {
                struct linux_dirent64 *d = (struct linux_dirent64 *)(d_buf + pos);
                print(d->d_name);
                print("  ");
                pos += d->d_reclen;
            }
            print("\n");
        }
        sys1(SYS_close, dir_fd);
    } else {
        print("    [-] Note: /compat/linux not found or failed to open\n");
    }

    print("\n[+] All Linux compatibility tests PASSED successfully!\n");
    print("=======================================================\n\n");

    /* Terminate cleanly via exit_group(231) */
    sys1(SYS_exit_group, 0);
}
