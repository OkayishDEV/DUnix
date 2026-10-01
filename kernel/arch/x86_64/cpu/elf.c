#include <arch/x86_64/cpu/elf.h>
#include <compat/linux.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

bool elf_validate(const void *elf_data, size_t size) {
    if (!elf_data || size < sizeof(Elf64_Ehdr)) {
        return false;
    }

    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)elf_data;

    if (ehdr->e_ident[0] != ELFMAG0 ||
        ehdr->e_ident[1] != ELFMAG1 ||
        ehdr->e_ident[2] != ELFMAG2 ||
        ehdr->e_ident[3] != ELFMAG3) {
        return false;
    }

    if (ehdr->e_ident[4] != ELFCLASS64) {
        return false;
    }

    if (ehdr->e_ident[5] != ELFDATA2LSB) {
        return false;
    }

    if (ehdr->e_machine != EM_X86_64) {
        return false;
    }

    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        return false;
    }

    return true;
}

bool elf_load(const void *elf_data, size_t size, uint64_t *pml4, struct elf_info *info) {
    if (!elf_validate(elf_data, size) || !info) {
        klog(KLOG_ERROR, "ELF: Invalid or corrupted ELF64 binary\n");
        return false;
    }

    memset(info, 0, sizeof(struct elf_info));
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)elf_data;
    const Elf64_Phdr *ph_table = (const Elf64_Phdr *)((const uint8_t *)elf_data + ehdr->e_phoff);

    /* Check ELF OSABI: 3 = Linux, 0 = System V */
    if (ehdr->e_ident[7] == 3) {
        info->is_linux = true;
    }

    /* Position-Independent Executables (ET_DYN) use load bias */
    uint64_t load_bias = 0;
    if (ehdr->e_type == ET_DYN) {
        info->is_pie = true;
        load_bias = 0x0000000000400000ULL;
    }

    uint64_t max_vaddr = 0;
    info->phnum = ehdr->e_phnum;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr *ph = &ph_table[i];

        /* Detect Program Interpreter (e.g. /lib64/ld-linux-x86-64.so.2) */
        if (ph->p_type == PT_INTERP) {
            info->is_linux = true;
            size_t copy_len = ph->p_filesz;
            if (copy_len >= sizeof(info->interp)) {
                copy_len = sizeof(info->interp) - 1;
            }
            if (ph->p_offset + copy_len <= size) {
                memcpy(info->interp, (const char *)elf_data + ph->p_offset, copy_len);
                info->interp[copy_len] = '\0';
            }
        }

        if (ph->p_type == PT_PHDR) {
            info->phdr_vaddr = load_bias + ph->p_vaddr;
        }

        if (ph->p_type != PT_LOAD || ph->p_memsz == 0) {
            continue;
        }

        uint64_t vaddr_start = load_bias + ph->p_vaddr;
        uint64_t vaddr_page_start = ALIGN_DOWN(vaddr_start, PAGE_SIZE_4K);
        uint64_t vaddr_page_end = ALIGN_UP(vaddr_start + ph->p_memsz, PAGE_SIZE_4K);
        size_t total_pages = (vaddr_page_end - vaddr_page_start) / PAGE_SIZE_4K;

        /* If PHDR segment wasn't explicitly present, calculate phdr_vaddr from first load segment */
        if (info->phdr_vaddr == 0 && ph->p_offset == 0) {
            info->phdr_vaddr = vaddr_start + ehdr->e_phoff;
        }

        uint64_t flags = VMM_FLAG_PRESENT | VMM_FLAG_USER | VMM_FLAG_WRITABLE;

        /* Allocate frames and map into target address space */
        for (size_t p = 0; p < total_pages; p++) {
            vaddr_t vaddr = vaddr_page_start + p * PAGE_SIZE_4K;
            paddr_t frame = vmm_get_phys(pml4, vaddr);
            if (!frame) {
                frame = pmm_alloc_frame();
                if (!frame) {
                    return false;
                }
                vmm_map_page(pml4, vaddr, frame, flags);
            }

            /* Direct write into frame via higher-half mapping */
            uint8_t *frame_virt = (uint8_t *)PHYS_TO_VIRT(frame);

            uint64_t page_offset_in_segment = (vaddr > vaddr_start) ? (vaddr - vaddr_start) : 0;
            uint64_t page_write_offset = (vaddr < vaddr_start) ? (vaddr_start - vaddr) : 0;

            if (page_offset_in_segment < ph->p_filesz) {
                uint64_t bytes_to_copy = ph->p_filesz - page_offset_in_segment;
                uint64_t max_page_bytes = PAGE_SIZE_4K - page_write_offset;
                if (bytes_to_copy > max_page_bytes) {
                    bytes_to_copy = max_page_bytes;
                }

                memcpy(frame_virt + page_write_offset,
                       (const uint8_t *)elf_data + ph->p_offset + page_offset_in_segment,
                       bytes_to_copy);
            }
        }

        if (vaddr_page_end > max_vaddr) {
            max_vaddr = vaddr_page_end;
        }
    }

    if (info->phdr_vaddr == 0) {
        info->phdr_vaddr = load_bias + ehdr->e_phoff;
    }

    info->entry_point = load_bias + ehdr->e_entry;
    info->brk_val = ALIGN_UP(max_vaddr, PAGE_SIZE_4K);

    return true;
}

uint64_t elf_setup_user_stack(uint64_t *pml4, int argc, char *const argv[], char *const envp[], const struct elf_info *info) {
    /* Map user stack pages */
    vaddr_t stack_bottom = USER_STACK_TOP - (USER_STACK_PAGES * PAGE_SIZE_4K);
    for (size_t i = 0; i < USER_STACK_PAGES; i++) {
        paddr_t frame = pmm_alloc_frame();
        vmm_map_page(pml4, stack_bottom + i * PAGE_SIZE_4K, frame,
                     VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER);
    }

    paddr_t top_frame_phys = vmm_get_phys(pml4, USER_STACK_TOP - PAGE_SIZE_4K);
    uint8_t *top_frame_virt = (uint8_t *)PHYS_TO_VIRT(top_frame_phys);
    uint64_t base_vaddr = USER_STACK_TOP - PAGE_SIZE_4K;

    /* Write strings at the top of the stack page */
    uint64_t sp_offset = PAGE_SIZE_4K - 16;

    /* 1. Platform string: "x86_64\0" */
    sp_offset -= 8;
    memcpy(top_frame_virt + sp_offset, "x86_64\0\0", 8);
    uint64_t platform_vaddr = base_vaddr + sp_offset;

    /* 2. 16 pseudo-random bytes for AT_RANDOM (Stack Canary Seed) */
    sp_offset -= 16;
    uint64_t *rand_p = (uint64_t *)(top_frame_virt + sp_offset);
    rand_p[0] = 0xd6f83b219e4a7c05ULL ^ (uint64_t)(uintptr_t)pml4;
    rand_p[1] = 0x853c49e6748fea9bULL ^ (uint64_t)(info ? info->entry_point : 0);
    uint64_t random_vaddr = base_vaddr + sp_offset;

    /* 3. Environment variable strings */
    static const char *const default_env[] = {
        "PATH=/bin:/compat/linux/bin:/sbin",
        "TERM=xterm-256color",
        "USER=root",
        "HOME=/root",
        "SHELL=/bin/sh",
        NULL
    };
    char *const *env_src = envp ? envp : (char *const *)default_env;
    int envc = 0;
    while (env_src && env_src[envc] && envc < 15) envc++;

    uint64_t env_ptrs[16];
    for (int j = 0; j < envc; j++) {
        size_t len = strlen(env_src[j]) + 1;
        sp_offset -= len;
        memcpy(top_frame_virt + sp_offset, env_src[j], len);
        env_ptrs[j] = base_vaddr + sp_offset;
    }

    /* 4. Argument strings */
    uint64_t argv_ptrs[32];
    int count = (argc > 31) ? 31 : argc;
    for (int i = 0; i < count; i++) {
        if (argv && argv[i]) {
            size_t len = strlen(argv[i]) + 1;
            sp_offset -= len;
            memcpy(top_frame_virt + sp_offset, argv[i], len);
            argv_ptrs[i] = base_vaddr + sp_offset;
        } else {
            argv_ptrs[i] = 0;
        }
    }

    uint64_t execfn_vaddr = (count > 0 && argv_ptrs[0]) ? argv_ptrs[0] : platform_vaddr;

    /* Align stack pointer before pushing pointer arrays */
    sp_offset = ALIGN_DOWN(sp_offset, 16);

    /* 5. Construct Auxiliary Vector table */
    struct {
        uint64_t a_type;
        uint64_t a_val;
    } auxv[20];
    int auxc = 0;

    #define PUSH_AUX(t, v) do { auxv[auxc].a_type = (t); auxv[auxc].a_val = (v); auxc++; } while(0)

    PUSH_AUX(LINUX_AT_PLATFORM, platform_vaddr);
    PUSH_AUX(LINUX_AT_EXECFN, execfn_vaddr);
    PUSH_AUX(LINUX_AT_SECURE, 0);
    PUSH_AUX(LINUX_AT_RANDOM, random_vaddr);
    PUSH_AUX(LINUX_AT_EGID, 0);
    PUSH_AUX(LINUX_AT_GID, 0);
    PUSH_AUX(LINUX_AT_EUID, 0);
    PUSH_AUX(LINUX_AT_UID, 0);
    PUSH_AUX(LINUX_AT_ENTRY, info ? info->entry_point : 0);
    PUSH_AUX(LINUX_AT_FLAGS, 0);
    PUSH_AUX(LINUX_AT_BASE, 0);
    PUSH_AUX(LINUX_AT_PHNUM, info ? info->phnum : 0);
    PUSH_AUX(LINUX_AT_PHENT, sizeof(Elf64_Phdr));
    PUSH_AUX(LINUX_AT_PHDR, info ? info->phdr_vaddr : 0);
    PUSH_AUX(LINUX_AT_CLKTCK, 100);
    PUSH_AUX(LINUX_AT_PAGESZ, 4096);
    PUSH_AUX(LINUX_AT_HWCAP, 0);
    PUSH_AUX(LINUX_AT_NULL, 0);

    #undef PUSH_AUX

    /* Total 8-byte entries on stack:
     * 1 (argc) + count (argv) + 1 (NULL) + envc (envp) + 1 (NULL) + auxc * 2 (auxv pairs)
     */
    int total_words = 1 + count + 1 + envc + 1 + (auxc * 2);

    /* Ensure final user_rsp pointing to argc is 16-byte aligned per System V AMD64 ABI */
    if ((total_words % 2) != 0) {
        sp_offset -= 8;
    }

    /* Push auxv in reverse order */
    for (int a = auxc - 1; a >= 0; a--) {
        sp_offset -= 8;
        *(uint64_t *)(top_frame_virt + sp_offset) = auxv[a].a_val;
        sp_offset -= 8;
        *(uint64_t *)(top_frame_virt + sp_offset) = auxv[a].a_type;
    }

    /* Push envp NULL terminator */
    sp_offset -= 8;
    *(uint64_t *)(top_frame_virt + sp_offset) = 0;

    /* Push envp pointers */
    for (int j = envc - 1; j >= 0; j--) {
        sp_offset -= 8;
        *(uint64_t *)(top_frame_virt + sp_offset) = env_ptrs[j];
    }

    /* Push argv NULL terminator */
    sp_offset -= 8;
    *(uint64_t *)(top_frame_virt + sp_offset) = 0;

    /* Push argv pointers */
    for (int i = count - 1; i >= 0; i--) {
        sp_offset -= 8;
        *(uint64_t *)(top_frame_virt + sp_offset) = argv_ptrs[i];
    }

    /* Push argc */
    sp_offset -= 8;
    *(uint64_t *)(top_frame_virt + sp_offset) = (uint64_t)count;

    return base_vaddr + sp_offset;
}
