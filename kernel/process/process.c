#include <process/process.h>
#include <sched/sched.h>
#include <mm/heap.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <arch/x86_64/cpu/elf.h>
#include <arch/x86_64/cpu/gdt.h>
#include <arch/x86_64/io.h>
#include <fs/vfs.h>
#include <syscall/syscall.h>
#include <init/version.h>
#include <compat/linux.h>
#include <arch/x86_64/drivers/bga.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

static struct process *process_table[MAX_PROCESSES];
static pid_t next_pid = 1;

static struct process init_process;

void process_init(void) {
    memset(process_table, 0, sizeof(process_table));
    memset(&init_process, 0, sizeof(struct process));

    init_process.pid = 1;
    init_process.ppid = 0;
    strcpy(init_process.name, "init");
    init_process.state = PROC_RUNNING;
    init_process.uid = 0;
    init_process.gid = 0;
    init_process.euid = 0;
    init_process.egid = 0;
    init_process.pml4 = vmm_get_kernel_pml4();
    init_process.mmap_current = MMAP_BASE_ADDR;
    strcpy(init_process.cwd, "/");
    init_process.main_thread = sched_get_current_thread();
    if (init_process.main_thread) {
        init_process.main_thread->process = &init_process;
    }

    process_table[1] = &init_process;

    klog(KLOG_INFO, "Process management subsystem initialized (Init PID 1 online)\n");
}

struct process *process_get_current(void) {
    struct thread *t = sched_get_current_thread();
    if (t && t->process) {
        return (struct process *)t->process;
    }
    return &init_process;
}

struct process *process_get_by_pid(pid_t pid) {
    if (pid >= 1 && pid < MAX_PROCESSES) {
        return process_table[pid];
    }
    return NULL;
}

struct process *process_create(const char *name, struct process *parent) {
    pid_t pid = -1;
    for (pid_t i = 1; i < MAX_PROCESSES; i++) {
        pid_t candidate = (next_pid + i - 1) % (MAX_PROCESSES - 1) + 1;
        if (!process_table[candidate]) {
            pid = candidate;
            next_pid = (candidate % (MAX_PROCESSES - 1)) + 1;
            break;
        }
    }

    if (pid == -1) {
        return NULL;
    }

    struct process *proc = (struct process *)kzalloc(sizeof(struct process));
    if (!proc) return NULL;

    proc->pid = pid;
    proc->ppid = parent ? parent->pid : 1;
    strncpy(proc->name, name ? name : "process", sizeof(proc->name) - 1);
    proc->state = PROC_EMBRYO;
    proc->parent = parent;
    proc->mmap_current = MMAP_BASE_ADDR;

    if (parent) {
        proc->uid = parent->uid;
        proc->gid = parent->gid;
        proc->euid = parent->euid;
        proc->egid = parent->egid;
        strcpy(proc->cwd, parent->cwd);

        /* Clone file descriptor table */
        for (int i = 0; i < MAX_FD; i++) {
            proc->files[i] = parent->files[i];
            if (proc->files[i]) {
                proc->files[i]->ref_count++;
            }
        }

        /* Link into parent's children list */
        proc->sibling = parent->children;
        parent->children = proc;
    } else {
        strcpy(proc->cwd, "/");
    }

    process_table[pid] = proc;
    return proc;
}

pid_t sys_getpid(void) {
    return process_get_current()->pid;
}

pid_t sys_getppid(void) {
    return process_get_current()->ppid;
}

uid_t sys_getuid(void) {
    return process_get_current()->uid;
}

gid_t sys_getgid(void) {
    return process_get_current()->gid;
}

uid_t sys_geteuid(void) {
    return process_get_current()->euid;
}

gid_t sys_getegid(void) {
    return process_get_current()->egid;
}

int64_t sys_setuid(uid_t uid) {
    struct process *proc = process_get_current();
    if (!proc) return -1;
    if (proc->euid != 0 && proc->uid != uid) {
        return -1; /* -EPERM */
    }
    proc->uid = uid;
    proc->euid = uid;
    return 0;
}

int64_t sys_setgid(gid_t gid) {
    struct process *proc = process_get_current();
    if (!proc) return -1;
    if (proc->euid != 0 && proc->gid != gid) {
        return -1; /* -EPERM */
    }
    proc->gid = gid;
    proc->egid = gid;
    return 0;
}

int64_t sys_seteuid(uid_t euid) {
    struct process *proc = process_get_current();
    if (!proc) return -1;
    if (proc->euid != 0 && proc->uid != euid) {
        return -1; /* -EPERM */
    }
    proc->euid = euid;
    return 0;
}

int64_t sys_setegid(gid_t egid) {
    struct process *proc = process_get_current();
    if (!proc) return -1;
    if (proc->euid != 0 && proc->gid != egid) {
        return -1; /* -EPERM */
    }
    proc->egid = egid;
    return 0;
}

int64_t sys_uname(struct utsname *buf) {
    if (!buf) return -14; /* -EFAULT */

    memset(buf, 0, sizeof(struct utsname));
    strcpy(buf->sysname, "DUnix");
    strcpy(buf->nodename, "dunix");
    strcpy(buf->release, "0.8.0-milestone15");
    strcpy(buf->version, "#1 Sun Sep 13 2026 (gcc 64-bit)");
    strcpy(buf->machine, "x86_64");
    strcpy(buf->domainname, "localdomain");
    return 0;
}

extern void fork_child_trampoline(void);

int64_t sys_fork(struct syscall_frame *frame) {
    if (!frame) return -14; /* -EFAULT */
    struct process *parent = process_get_current();
    if (!parent) return -1;

    struct process *child = process_create(parent->name, parent);
    if (!child) return -12; /* -ENOMEM */

    /* Clone virtual address space */
    child->pml4 = vmm_clone_address_space(parent->pml4);
    if (!child->pml4) {
        process_table[child->pid] = NULL;
        kfree(child);
        return -12;
    }

    child->brk_start = parent->brk_start;
    child->brk_current = parent->brk_current;
    child->mmap_current = parent->mmap_current;
    child->fs_base = parent->fs_base;
    child->gs_base = parent->gs_base;
    child->is_linux_compat = parent->is_linux_compat;

    /* Create child execution thread */
    struct thread *child_thread = (struct thread *)kzalloc(sizeof(struct thread));
    if (!child_thread) {
        vmm_destroy_address_space(child->pml4);
        process_table[child->pid] = NULL;
        kfree(child);
        return -12;
    }

    child_thread->tid = child->pid;
    strncpy(child_thread->name, child->name, sizeof(child_thread->name) - 1);
    child_thread->pml4 = child->pml4;
    child_thread->priority = 1;
    child_thread->time_slice = DEFAULT_TIME_SLICE;
    child_thread->fs_base = parent->fs_base;
    child_thread->process = child;

    child_thread->kernel_stack = (uint64_t)kmalloc(THREAD_STACK_SIZE);
    if (!child_thread->kernel_stack) {
        kfree(child_thread);
        vmm_destroy_address_space(child->pml4);
        process_table[child->pid] = NULL;
        kfree(child);
        return -12;
    }
    child_thread->kernel_stack_top = child_thread->kernel_stack + THREAD_STACK_SIZE;

    /* Copy syscall_frame to child kernel stack */
    uint64_t sp = child_thread->kernel_stack_top;
    sp -= sizeof(struct syscall_frame);
    memcpy((void *)sp, frame, sizeof(struct syscall_frame));

    /* Prepare switch_context stack frame */
    uint64_t *stack_ptr = (uint64_t *)sp;
    *(--stack_ptr) = (uint64_t)fork_child_trampoline;
    *(--stack_ptr) = 0; /* r15 */
    *(--stack_ptr) = 0; /* r14 */
    *(--stack_ptr) = 0; /* r13 */
    *(--stack_ptr) = 0; /* r12 */
    *(--stack_ptr) = 0; /* rbp */
    *(--stack_ptr) = 0; /* rbx */

    child_thread->rsp = (uint64_t)stack_ptr;
    child->main_thread = child_thread;
    child->state = PROC_RUNNING;

    sched_add_runnable(child_thread);

    return child->pid;
}

int64_t sys_execve(const char *filename, char *const argv[], char *const envp[]) {
    if (!filename) return -14; /* -EFAULT */

    struct process *proc = process_get_current();
    if (!proc) return -1;

    /* Read binary file from VFS */
    struct vfs_node *node = vfs_lookup(filename);
    if (!node) {
        return -2; /* -ENOENT */
    }

    size_t file_size = (size_t)node->length;
    if (file_size == 0) {
        return -8; /* -ENOEXEC */
    }

    uint8_t *elf_buf = (uint8_t *)kmalloc(file_size);
    if (!elf_buf) return -12; /* -ENOMEM */

    ssize_t bytes_read = vfs_read(node, 0, file_size, elf_buf);
    if (bytes_read <= 0 || (size_t)bytes_read < file_size) {
        kfree(elf_buf);
        return -5; /* -EIO */
    }

    /* Check for Shebang (#!) script execution */
    if (file_size >= 2 && elf_buf[0] == '#' && elf_buf[1] == '!') {
        char interp[128];
        char *line_end = strchr((char *)elf_buf, '\n');
        size_t line_len = line_end ? (size_t)(line_end - (char *)elf_buf) : file_size;
        if (line_len > sizeof(interp) - 1) line_len = sizeof(interp) - 1;

        char *p = (char *)elf_buf + 2;
        while (*p == ' ' || *p == '\t') p++;

        int i = 0;
        while (p < (char *)elf_buf + line_len && *p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && i < 127) {
            interp[i++] = *p++;
        }
        interp[i] = '\0';

        /* Handle /usr/bin/env <cmd> */
        if (strcmp(interp, "/usr/bin/env") == 0 || strcmp(interp, "/bin/env") == 0) {
            while (p < (char *)elf_buf + line_len && (*p == ' ' || *p == '\t')) p++;
            i = 0;
            char env_cmd[64];
            while (p < (char *)elf_buf + line_len && *p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n' && i < 63) {
                env_cmd[i++] = *p++;
            }
            env_cmd[i] = '\0';
            if (strcmp(env_cmd, "bash") == 0 || strcmp(env_cmd, "sh") == 0) {
                strcpy(interp, "/bin/sh");
            } else {
                strcpy(interp, "/bin/");
                strncat(interp, env_cmd, sizeof(interp) - 6);
            }
        }

        /* Map /bin/bash or /usr/bin/bash to /bin/sh */
        if (strcmp(interp, "/bin/bash") == 0 || strcmp(interp, "/usr/bin/bash") == 0) {
            strcpy(interp, "/bin/sh");
        }

        kfree(elf_buf);

        /* Construct new argv array: [interp, filename, argv[1], ...] */
        char *new_argv[32];
        new_argv[0] = interp;
        new_argv[1] = (char *)filename;
        int dst = 2;
        if (argv && argv[0]) {
            for (int src = 1; argv[src] && dst < 31; src++) {
                new_argv[dst++] = argv[src];
            }
        }
        new_argv[dst] = NULL;
        return sys_execve(interp, new_argv, envp);
    }

    /* Create fresh new address space */
    uint64_t *new_pml4 = vmm_create_address_space();
    if (!new_pml4) {
        kfree(elf_buf);
        return -12;
    }

    struct elf_info info;
    if (!elf_load(elf_buf, file_size, new_pml4, &info)) {
        vmm_destroy_address_space(new_pml4);
        kfree(elf_buf);
        return -8; /* -ENOEXEC */
    }

    if (strncmp(filename, "/compat/linux", 13) == 0 || strstr(filename, "linux_") != NULL || strstr(filename, "links") != NULL || strstr(filename, "lynx") != NULL) {
        info.is_linux = true;
    }

    if (envp) {
        for (int e = 0; envp[e]; e++) {
            if (strncmp(envp[e], "LINUX_COMPAT=", 13) == 0) {
                info.is_linux = true;
                break;
            }
        }
    }

    kfree(elf_buf);

    /* Count argc */
    int argc = 0;
    if (argv) {
        while (argv[argc]) argc++;
    }

    /* Setup user stack in new address space with System V AMD64 ABI & auxv */
    uint64_t user_rsp = elf_setup_user_stack(new_pml4, argc, argv, envp, &info);

    /* Clean old address space */
    uint64_t *old_pml4 = proc->pml4;
    proc->pml4 = new_pml4;
    proc->brk_start = info.brk_val;
    proc->brk_current = info.brk_val;
    proc->mmap_current = MMAP_BASE_ADDR;
    proc->fs_base = 0;
    proc->gs_base = 0;
    proc->clear_child_tid = NULL;
    proc->is_linux_compat = info.is_linux;
    strncpy(proc->name, filename, sizeof(proc->name) - 1);

    if (proc->main_thread) {
        proc->main_thread->pml4 = new_pml4;
        proc->main_thread->fs_base = 0;
    }

    /* Reset IA32_FS_BASE hardware MSR to 0 for fresh binary */
    wrmsr(0xC0000100, 0);

    /* Apply SUID and SGID bits if set on the executable node */
    if (node->mask & 04000) {
        proc->euid = node->uid;
    }
    if (node->mask & 02000) {
        proc->egid = node->gid;
    }

    vmm_switch_pml4(new_pml4);
    if (old_pml4 && old_pml4 != vmm_get_kernel_pml4()) {
        vmm_destroy_address_space(old_pml4);
    }

    /* Jump to userspace */
    enter_usermode(info.entry_point, user_rsp);
    return 0;
}

int64_t sys_exit(int status) {
    struct process *proc = process_get_current();
    if (!proc) return -1;

    proc->exit_code = status;
    proc->state = PROC_ZOMBIE;

    /* Reparent children to Init (PID 1) */
    struct process *child = proc->children;
    while (child) {
        struct process *next = child->sibling;
        child->parent = &init_process;
        child->sibling = init_process.children;
        init_process.children = child;
        child = next;
    }
    proc->children = NULL;

    /* Close all open file descriptors */
    for (int i = 0; i < MAX_FD; i++) {
        if (proc->files[i]) {
            vfs_close_fd(proc->files[i]);
            proc->files[i] = NULL;
        }
    }

    /* Terminate current thread and yield */
    thread_exit();
    return 0;
}

int64_t sys_waitpid(pid_t pid, int *status, int options) {
    struct process *parent = process_get_current();
    if (!parent) return -1;

    for (;;) {
        struct process *prev = NULL;
        struct process *curr = parent->children;
        bool has_children = false;

        while (curr) {
            if (pid == -1 || curr->pid == pid) {
                has_children = true;
                if (curr->state == PROC_ZOMBIE) {
                    pid_t child_pid = curr->pid;
                    if (status) {
                        *status = (curr->exit_code & 0xFF) << 8;
                    }

                    /* Remove from children list */
                    if (prev) {
                        prev->sibling = curr->sibling;
                    } else {
                        parent->children = curr->sibling;
                    }

                    if (curr->pml4 && curr->pml4 != vmm_get_kernel_pml4()) {
                        vmm_destroy_address_space(curr->pml4);
                    }

                    if (curr->main_thread) {
                        if (curr->main_thread->kernel_stack) {
                            kfree((void *)curr->main_thread->kernel_stack);
                        }
                        kfree(curr->main_thread);
                        curr->main_thread = NULL;
                    }

                    process_table[curr->pid] = NULL;
                    kfree(curr);
                    return child_pid;
                }
            }
            prev = curr;
            curr = curr->sibling;
        }

        if (!has_children) {
            return -10; /* -ECHILD */
        }

        if (options & WNOHANG) {
            return 0;
        }

        /* Yield and wait for child to exit */
        sched_sleep(10);
    }
}

int64_t sys_brk(void *addr) {
    struct process *proc = process_get_current();
    if (!proc) return -1;

    uint64_t new_brk = (uint64_t)addr;
    if (new_brk == 0 || new_brk < proc->brk_start) {
        return proc->brk_current;
    }

    if (new_brk > proc->brk_current) {
        uint64_t start_page = ALIGN_UP(proc->brk_current, PAGE_SIZE_4K);
        uint64_t end_page = ALIGN_UP(new_brk, PAGE_SIZE_4K);

        for (uint64_t p = start_page; p < end_page; p += PAGE_SIZE_4K) {
            paddr_t frame = pmm_alloc_frame();
            if (!frame) {
                return proc->brk_current;
            }
            vmm_map_page(proc->pml4, p, frame, VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER);
        }
    }

    proc->brk_current = new_brk;
    return new_brk;
}

int64_t sys_getcwd(char *buf, size_t size) {
    if (!buf || size == 0) return -14; /* -EFAULT */
    struct process *proc = process_get_current();
    size_t len = strlen(proc->cwd);
    if (size < len + 1) {
        return -34; /* -ERANGE */
    }
    memcpy(buf, proc->cwd, len + 1);
    return (int64_t)buf;
}

int64_t sys_chdir(const char *path) {
    if (!path) return -14; /* -EFAULT */
    struct process *proc = process_get_current();

    struct vfs_node *node = vfs_lookup(path);
    if (!node) {
        return -2; /* -ENOENT */
    }
    if ((node->flags & VFS_DIRECTORY) != VFS_DIRECTORY) {
        return -20; /* -ENOTDIR */
    }

    if (path[0] == '/') {
        strncpy(proc->cwd, path, sizeof(proc->cwd) - 1);
    } else {
        if (strcmp(proc->cwd, "/") != 0) {
            strncat(proc->cwd, "/", sizeof(proc->cwd) - strlen(proc->cwd) - 1);
        }
        strncat(proc->cwd, path, sizeof(proc->cwd) - strlen(proc->cwd) - 1);
    }

    return 0;
}

void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    if (length == 0) return MAP_FAILED;

    struct process *proc = process_get_current();
    if (!proc) return MAP_FAILED;

    uint64_t vaddr = 0;
    if (addr && (flags & MAP_FIXED)) {
        vaddr = (uint64_t)addr;
    } else {
        vaddr = proc->mmap_current;
        proc->mmap_current += ALIGN_UP(length, PAGE_SIZE_4K);
    }

    size_t pages = (length + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
    uint64_t pte_flags = VMM_FLAG_PRESENT | VMM_FLAG_USER;
    if (prot & PROT_WRITE) pte_flags |= VMM_FLAG_WRITABLE;
    if (!(prot & PROT_EXEC)) pte_flags |= VMM_FLAG_NX;

    bool is_device_vram = false;
    paddr_t device_base = 0;
    if (!(flags & MAP_ANONYMOUS) && fd >= 0 && fd < MAX_FD && proc->files[fd]) {
        struct file *f = proc->files[fd];
        if (f && f->node) {
            if (strcmp(f->node->name, "fb0") == 0 ||
                strcmp(f->node->name, "card0") == 0 ||
                strcmp(f->node->name, "renderD128") == 0) {
                is_device_vram = true;
                device_base = bga_get_phys_addr();
            }
        }
    }

    for (size_t i = 0; i < pages; i++) {
        if (is_device_vram) {
            paddr_t frame = device_base + offset + i * PAGE_SIZE_4K;
            vmm_map_page(proc->pml4, vaddr + i * PAGE_SIZE_4K, frame, pte_flags | PTE_PCD);
        } else {
            paddr_t frame = pmm_alloc_frame();
            if (!frame) {
                return MAP_FAILED;
            }

            if (!(flags & MAP_ANONYMOUS) && fd >= 0 && fd < MAX_FD && proc->files[fd]) {
                struct file *f = proc->files[fd];
                vfs_read(f->node, offset + i * PAGE_SIZE_4K, PAGE_SIZE_4K, (void *)PHYS_TO_VIRT(frame));
            }

            vmm_map_page(proc->pml4, vaddr + i * PAGE_SIZE_4K, frame, pte_flags);
        }
    }

    return (void *)vaddr;
}

int64_t sys_munmap(void *addr, size_t length) {
    if (!addr || length == 0) return -22; /* -EINVAL */

    struct process *proc = process_get_current();
    if (!proc) return -1;

    uint64_t vaddr = (uint64_t)addr;
    size_t pages = (length + PAGE_SIZE_4K - 1) / PAGE_SIZE_4K;
    paddr_t vram_base = bga_get_phys_addr();
    paddr_t vram_end = vram_base + 16 * 1024 * 1024;

    for (size_t i = 0; i < pages; i++) {
        paddr_t paddr = vmm_get_phys(proc->pml4, vaddr + i * PAGE_SIZE_4K);
        if (paddr) {
            /* Only free RAM frames from physical allocator, never hardware VRAM! */
            if (paddr < vram_base || paddr >= vram_end) {
                pmm_free_frame(paddr);
            }
        }
        vmm_unmap_page(proc->pml4, vaddr + i * PAGE_SIZE_4K);
    }

    return 0;
}
