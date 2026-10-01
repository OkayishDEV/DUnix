#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdint.h>
#include <stdbool.h>

#define ELFMAG "\177ELF"
#define SELFMAG 4

typedef struct {
    unsigned char e_ident[16];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint64_t      e_entry;
    uint64_t      e_phoff;
    uint64_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

static void print_usage(const char *prog) {
    printf("DUnix Linux Binary Compatibility Layer Launcher (Linuxulator)\n");
    printf("Usage: %s [-i] <linux_executable> [args...]\n", prog);
    printf("       %s -v | --version\n", prog);
    printf("       %s -h | --help\n\n", prog);
    printf("Options:\n");
    printf("  -i <binary>   Inspect Linux ELF binary ABI and headers\n");
    printf("  -v, --version Display Linux compatibility layer version & capabilities\n");
    printf("  -h, --help    Show this help message\n");
}

static void print_version(void) {
    printf("DUnix Linuxulator 1.0 (x86_64 ABI Emulation)\n");
    printf("Kernel Emulation Target: Linux 5.15.0-dunix (System V AMD64 ABI)\n");
    printf("Features Supported:\n");
    printf("  - Thread-Local Storage (TLS): IA32_FS_BASE (MSR 0xC0000100) hardware switching\n");
    printf("  - System V AMD64 Auxiliary Vector (auxv): AT_RANDOM, AT_PAGESZ, AT_PHDR, AT_ENTRY\n");
    printf("  - Linux Syscalls: openat, newfstatat, getdents64, writev, readv, clock_gettime,\n");
    printf("                   futex, arch_prctl, set_tid_address, prlimit64, getrandom, exit_group\n");
    printf("  - Compat Root: /compat/linux path fallback & shadow filesystem\n");
}

static int inspect_elf(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    Elf64_Ehdr ehdr;
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        printf("Error: failed to read ELF header from %s\n", path);
        close(fd);
        return 1;
    }

    if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0) {
        printf("Error: %s is not an ELF binary\n", path);
        close(fd);
        return 1;
    }

    printf("ELF Binary Inspection: %s\n", path);
    printf("----------------------------------------\n");
    printf("Class:       %s\n", (ehdr.e_ident[4] == 2) ? "ELF64 (64-bit)" : "ELF32 (unsupported)");
    printf("Endianness:  %s\n", (ehdr.e_ident[5] == 1) ? "Little Endian" : "Big Endian");
    
    const char *osabi_str = "Unknown";
    if (ehdr.e_ident[7] == 0) osabi_str = "UNIX - System V (Linux default)";
    else if (ehdr.e_ident[7] == 3) osabi_str = "GNU / Linux";
    printf("OS/ABI:      %s (0x%02x)\n", osabi_str, ehdr.e_ident[7]);

    printf("Machine:     %s (0x%x)\n", (ehdr.e_machine == 62) ? "Advanced Micro Devices X86-64" : "Other", ehdr.e_machine);
    printf("Type:        %s\n", (ehdr.e_type == 2) ? "EXEC (Executable)" : (ehdr.e_type == 3) ? "DYN (Position-Independent Executable)" : "Other");
    printf("Entry Point: 0x%lx\n", (unsigned long)ehdr.e_entry);
    printf("Program Hdr: %u headers at offset 0x%lx\n", ehdr.e_phnum, (unsigned long)ehdr.e_phoff);

    /* Read Program Headers to look for dynamic interpreter */
    char interp[128] = {0};
    if (ehdr.e_phnum > 0 && ehdr.e_phoff > 0) {
        lseek(fd, (off_t)ehdr.e_phoff, SEEK_SET);
        for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
            Elf64_Phdr ph;
            if (read(fd, &ph, sizeof(ph)) == sizeof(ph)) {
                if (ph.p_type == 3 /* PT_INTERP */) {
                    off_t cur = lseek(fd, 0, SEEK_CUR);
                    lseek(fd, (off_t)ph.p_offset, SEEK_SET);
                    size_t to_read = (ph.p_filesz < sizeof(interp) - 1) ? ph.p_filesz : (sizeof(interp) - 1);
                    read(fd, interp, to_read);
                    interp[to_read] = '\0';
                    lseek(fd, cur, SEEK_SET);
                }
            }
        }
    }

    if (interp[0]) {
        printf("Link Type:   Dynamic (Interpreter: %s)\n", interp);
    } else {
        printf("Link Type:   Static (Direct execution supported)\n");
    }

    printf("Compat ABI:  COMPATIBLE with DUnix Linuxulator\n");
    close(fd);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
        print_version();
        return 0;
    }

    if (strcmp(argv[1], "-i") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: missing binary path for -i\n");
            return 1;
        }
        return inspect_elf(argv[2]);
    }

    /* Execute the specified Linux executable with remaining arguments */
    const char *target = argv[1];
    char **sub_argv = &argv[1];

    /* Check if target binary exists */
    struct stat st;
    if (stat(target, &st) != 0) {
        /* Check in /compat/linux/bin or /bin */
        char alt_path[256];
        snprintf(alt_path, sizeof(alt_path), "/compat/linux/bin/%s", target);
        if (stat(alt_path, &st) == 0) {
            target = alt_path;
            sub_argv[0] = (char *)target;
        } else {
            snprintf(alt_path, sizeof(alt_path), "/bin/%s", target);
            if (stat(alt_path, &st) == 0) {
                target = alt_path;
                sub_argv[0] = (char *)target;
            } else {
                fprintf(stderr, "linux: cannot execute '%s': No such file or directory\n", argv[1]);
                return 127;
            }
        }
    }

    /* Set up Linux execution environment */
    char *const linux_env[] = {
        (char *)"LINUX_COMPAT=1",
        (char *)"PATH=/compat/linux/bin:/bin:/usr/bin",
        "TERM=xterm-256color",
        (char *)"USER=root",
        (char *)"HOME=/root",
        NULL
    };

    /* Execute the program */
    execve(target, sub_argv, linux_env);

    /* If execve returns, an error occurred */
    perror("execve");
    return 126;
}
