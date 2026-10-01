#ifndef _ARCH_X86_64_CPU_ELF_H
#define _ARCH_X86_64_CPU_ELF_H

#include <dunix/types.h>
#include <dunix/stdbool.h>

#define EI_NIDENT 16

#define ELFMAG0 0x7F
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1
#define ET_EXEC 2
#define ET_DYN  3
#define EM_X86_64 62

#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_TLS     7
#define PT_GNU_STACK 0x6474E551

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

typedef struct {
    uint8_t  e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
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

typedef struct {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} Elf64_Shdr;

struct elf_info {
    uint64_t entry_point;
    uint64_t brk_val;
    uint64_t phdr_vaddr;
    uint64_t phnum;
    bool     is_pie;
    bool     is_linux;
    char     interp[128];
};

bool     elf_validate(const void *elf_data, size_t size);
bool     elf_load(const void *elf_data, size_t size, uint64_t *pml4, struct elf_info *info);
uint64_t elf_setup_user_stack(uint64_t *pml4, int argc, char *const argv[], char *const envp[], const struct elf_info *info);

#endif /* _ARCH_X86_64_CPU_ELF_H */
