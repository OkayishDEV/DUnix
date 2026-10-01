#include <fs/procfs.h>
#include <fs/ramfs.h>
#include <fs/block.h>
#include <process/process.h>
#include <sched/sched.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <init/version.h>
#include <boot/boot_info.h>
#include <net/net.h>
#include <arch/x86_64/drivers/pit.h>
#include <arch/x86_64/drivers/dmi.h>
#include <arch/x86_64/drivers/pci.h>
#include <arch/x86_64/cpu/cpuid.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

#define PROCFS_BUF_SIZE 2048

static struct vfs_ops proc_version_ops;
static struct vfs_ops proc_cpuinfo_ops;
static struct vfs_ops proc_meminfo_ops;
static struct vfs_ops proc_uptime_ops;
static struct vfs_ops proc_mounts_ops;
static struct vfs_ops proc_cmdline_ops;
static struct vfs_ops proc_partitions_ops;
static struct vfs_ops proc_net_dev_ops;
static struct vfs_ops proc_dmi_ops;
static struct vfs_ops proc_pci_ops;

static ssize_t proc_version_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node;
    char buf[256];
    ksnprintf(buf, sizeof(buf), "DUnix version %s (x86_64) (gcc) #1 Sun Sep 13 2026\n", DUNIX_VERSION_STRING);

    size_t len = strlen(buf);
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;

    memcpy(buffer, buf + offset, size);
    return (ssize_t)size;
}

static ssize_t proc_cpuinfo_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node;
    struct cpu_info cpu;
    cpu_detect(&cpu);

    char buf[1024];
    ksnprintf(buf, sizeof(buf),
             "processor\t: 0\n"
             "vendor_id\t: %s\n"
             "cpu family\t: %u\n"
             "model\t\t: %u\n"
             "model name\t: %s\n"
             "stepping\t: %u\n"
             "cpu MHz\t\t: 2400.000\n"
             "cache size\t: 4096 KB\n"
             "cpu cores\t: 2\n"
             "siblings\t: 2\n"
             "flags\t\t: %s\n",
             cpu.vendor,
             cpu.family,
             cpu.model,
             cpu.brand[0] ? cpu.brand : "x86_64 Processor",
             cpu.stepping,
             "fpu vme de pse tsc msr pae mce cx8 apic sep mtrr pge mca cmov pat pse36 clflush mmx fxsr sse sse2 syscall nx lm");

    size_t len = strlen(buf);
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;

    memcpy(buffer, buf + offset, size);
    return (ssize_t)size;
}

static ssize_t proc_meminfo_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node;
    size_t total_kb = pmm_get_total_memory_kb();
    size_t free_kb  = pmm_get_free_memory_kb();
    size_t heap_free_kb = kheap_get_free_bytes() / 1024;
    size_t heap_total_kb = (kheap_get_free_bytes() + kheap_get_used_bytes()) / 1024;

    char buf[512];
    ksnprintf(buf, sizeof(buf),
             "MemTotal:       %lu kB\n"
             "MemFree:        %lu kB\n"
             "MemAvailable:   %lu kB\n"
             "Buffers:             0 kB\n"
             "Cached:              0 kB\n"
             "HeapTotal:      %lu kB\n"
             "HeapFree:       %lu kB\n",
             total_kb, free_kb, free_kb, heap_total_kb, heap_free_kb);

    size_t len = strlen(buf);
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;

    memcpy(buffer, buf + offset, size);
    return (ssize_t)size;
}

static ssize_t proc_uptime_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node;
    uint64_t ticks = pit_get_ticks();
    uint64_t secs = ticks / PIT_TARGET_HZ;
    uint64_t frac = (ticks % PIT_TARGET_HZ) * 100 / PIT_TARGET_HZ;

    char buf[64];
    ksnprintf(buf, sizeof(buf), "%lu.%02lu %lu.%02lu\n", secs, frac, secs, frac);

    size_t len = strlen(buf);
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;

    memcpy(buffer, buf + offset, size);
    return (ssize_t)size;
}

static ssize_t proc_mounts_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node;
    const char *buf =
        "rootfs / ramfs rw 0 0\n"
        "devfs /dev devfs rw 0 0\n"
        "procfs /proc procfs rw 0 0\n";

    size_t len = strlen(buf);
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;

    memcpy(buffer, buf + offset, size);
    return (ssize_t)size;
}

static ssize_t proc_cmdline_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node;
    char buf[256];
    if (g_boot_info.cmdline[0]) {
        ksnprintf(buf, sizeof(buf), "%s\n", g_boot_info.cmdline);
    } else {
        struct block_device *first_real = block_device_get_first();
        const char *root_dev = "ram0";
        while (first_real) {
            if (strncmp(first_real->name, "sd", 2) == 0 || strncmp(first_real->name, "hd", 2) == 0) {
                root_dev = first_real->name;
                break;
            }
            first_real = first_real->next;
        }
        ksnprintf(buf, sizeof(buf), "BOOT_IMAGE=/build/dunix.bin root=/dev/%s rw\n", root_dev);
    }

    size_t len = strlen(buf);
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;

    memcpy(buffer, buf + offset, size);
    return (ssize_t)size;
}

static ssize_t proc_partitions_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node;
    char buf[1024];
    int pos = ksnprintf(buf, sizeof(buf), "major minor  #blocks  name     model\n\n");

    struct block_device *bdev = block_device_get_first();
    int minor = 0;
    while (bdev && pos < (int)sizeof(buf) - 80) {
        int major = (strncmp(bdev->name, "ram", 3) == 0) ? 1 : 8;
        uint64_t blocks = (uint64_t)bdev->total_blocks * (bdev->block_size ? bdev->block_size : 512) / 1024;
        const char *model = bdev->model[0] ? bdev->model : (major == 1 ? "RAM Disk" : "Generic Fixed Disk");
        pos += ksnprintf(buf + pos, sizeof(buf) - pos, "%4d %7d %10lu %-8s %s\n", major, minor++, blocks, bdev->name, model);
        bdev = bdev->next;
    }

    size_t len = (size_t)pos;
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;
    memcpy(buffer, buf + offset, size);
    return (ssize_t)size;
}

static ssize_t proc_net_dev_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node;
    char buf[1024];
    int pos = ksnprintf(buf, sizeof(buf),
        "Inter-|   Receive                                                |  Transmit\n"
        " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n");

    struct net_if *cur = net_get_all();
    while (cur && pos < (int)sizeof(buf) - 128) {
        pos += ksnprintf(buf + pos, sizeof(buf) - pos,
            "%6s: %8lu %7lu    0    0    0     0          0         0 %8lu %7lu    0    0    0     0       0          0\n",
            cur->name, cur->rx_bytes, cur->rx_packets, cur->tx_bytes, cur->tx_packets);
        cur = cur->next;
    }

    size_t len = (size_t)pos;
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;
    memcpy(buffer, buf + offset, size);
    return (ssize_t)size;
}

static ssize_t proc_dmi_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node;
    const struct dmi_system_info *dmi = dmi_get_info();
    char buf[512];
    int pos = ksnprintf(buf, sizeof(buf),
        "sys_vendor:      %s\n"
        "product_name:    %s\n"
        "product_version: %s\n"
        "bios_vendor:     %s\n"
        "bios_version:    %s\n"
        "bios_date:       %s\n"
        "board_name:      %s\n"
        "chassis_type:    %s\n",
        dmi->sys_vendor[0] ? dmi->sys_vendor : "PC Compatible",
        dmi->product_name[0] ? dmi->product_name : "x86_64 Machine",
        dmi->product_version[0] ? dmi->product_version : "1.0",
        dmi->bios_vendor[0] ? dmi->bios_vendor : "Unknown",
        dmi->bios_version[0] ? dmi->bios_version : "Unknown",
        dmi->bios_date[0] ? dmi->bios_date : "Unknown",
        dmi->board_name[0] ? dmi->board_name : "Unknown",
        dmi->chassis_type[0] ? dmi->chassis_type : "Desktop");

    size_t len = (size_t)pos;
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;
    memcpy(buffer, buf + offset, size);
    return (ssize_t)size;
}

static ssize_t proc_pci_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    (void)node;
    char buf[2048];
    int pos = ksnprintf(buf, sizeof(buf), "PCI Devices:\n");

    struct pci_device *dev = pci_get_device_list();
    while (dev && pos < (int)sizeof(buf) - 140) {
        const char *desc = "Unknown PCI Device";
        if (dev->class_code == PCI_CLASS_DISPLAY) {
            if (dev->vendor_id == 0x1234 && dev->device_id == 0x1111) desc = "BGA / Bochs / QEMU VGA Compatible Display Controller";
            else if (dev->vendor_id == 0x8086) desc = "Intel Graphics Display Controller";
            else if (dev->vendor_id == 0x10DE) desc = "NVIDIA Graphics Display Controller";
            else if (dev->vendor_id == 0x1002) desc = "AMD/ATI Radeon Graphics Controller";
            else if (dev->vendor_id == 0x15AD) desc = "VMware SVGA II Adapter";
            else if (dev->vendor_id == 0x1AF4) desc = "VirtIO GPU Display Adapter";
            else desc = "VGA / Display Controller";
        } else if (dev->class_code == PCI_CLASS_NETWORK) {
            if (dev->vendor_id == 0x8086) desc = "Intel PRO/1000 Gigabit Ethernet";
            else if (dev->vendor_id == 0x10EC) desc = "Realtek RTL8139 / RTL8169 Ethernet";
            else desc = "Network Controller";
        } else if (dev->class_code == PCI_CLASS_STORAGE) {
            if (dev->subclass == PCI_SUBCLASS_SATA) desc = "SATA AHCI Controller";
            else if (dev->subclass == PCI_SUBCLASS_IDE) desc = "IDE Controller";
            else if (dev->subclass == PCI_SUBCLASS_NVME) desc = "NVM Express Controller";
            else desc = "Mass Storage Controller";
        } else if (dev->class_code == PCI_CLASS_BRIDGE) {
            desc = "PCI Host / ISA Bridge";
        }

        pos += ksnprintf(buf + pos, sizeof(buf) - pos,
            "%02x:%02x.%x %04x:%04x (Class %02x:%02x, IRQ %u) - %s\n",
            dev->bus, dev->device, dev->function,
            dev->vendor_id, dev->device_id,
            dev->class_code, dev->subclass, dev->irq_line, desc);
        dev = dev->next;
    }

    size_t len = (size_t)pos;
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;
    memcpy(buffer, buf + offset, size);
    return (ssize_t)size;
}

/* Dynamic Process Information Ops */
static ssize_t proc_pid_status_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    pid_t pid = (pid_t)node->inode;
    struct process *p = process_get_by_pid(pid);
    if (!p) return 0;

    const char *state_str = "R (running)";
    if (p->state == PROC_SLEEPING) state_str = "S (sleeping)";
    else if (p->state == PROC_ZOMBIE) state_str = "Z (zombie)";
    else if (p->state == PROC_EMBRYO) state_str = "D (disk sleep)";

    char buf[512];
    ksnprintf(buf, sizeof(buf),
             "Name:\t%s\n"
             "State:\t%s\n"
             "Tgid:\t%d\n"
             "Pid:\t%d\n"
             "PPid:\t%d\n"
             "Uid:\t%u\t%u\t%u\t%u\n"
             "Gid:\t%u\t%u\t%u\t%u\n"
             "VmSize:\t%lu kB\n",
             p->name,
             state_str,
             p->pid,
             p->pid,
             p->ppid,
             p->uid, p->euid, p->uid, p->euid,
             p->gid, p->egid, p->gid, p->egid,
             p->brk_current ? (p->brk_current - p->brk_start) / 1024 + 128 : 128);

    size_t len = strlen(buf);
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;

    memcpy(buffer, buf + offset, size);
    return (ssize_t)size;
}

static ssize_t proc_pid_cmdline_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    pid_t pid = (pid_t)node->inode;
    struct process *p = process_get_by_pid(pid);
    if (!p) return 0;

    char buf[128];
    ksnprintf(buf, sizeof(buf), "%s\n", p->name);

    size_t len = strlen(buf);
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;

    memcpy(buffer, buf + offset, size);
    return (ssize_t)size;
}

static ssize_t proc_pid_stat_read(struct vfs_node *node, uint64_t offset, size_t size, void *buffer) {
    pid_t pid = (pid_t)node->inode;
    struct process *p = process_get_by_pid(pid);
    if (!p) return 0;

    char state_ch = 'R';
    if (p->state == PROC_SLEEPING) state_ch = 'S';
    else if (p->state == PROC_ZOMBIE) state_ch = 'Z';

    char buf[256];
    ksnprintf(buf, sizeof(buf), "%d (%s) %c %d 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n",
             p->pid, p->name, state_ch, p->ppid);

    size_t len = strlen(buf);
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;

    memcpy(buffer, buf + offset, size);
    return (ssize_t)size;
}

static struct vfs_ops pid_status_ops;
static struct vfs_ops pid_cmdline_ops;
static struct vfs_ops pid_stat_ops;

static struct vfs_node *proc_root_node = NULL;

static void create_proc_file(struct vfs_node *dir, const char *name, struct vfs_ops *ops) {
    struct vfs_node *f = ramfs_create_file(dir, name, NULL, 0, 0444);
    if (f) {
        f->ops = ops;
    }
}

void procfs_init(void) {
    proc_version_ops.read = proc_version_read;
    proc_cpuinfo_ops.read = proc_cpuinfo_read;
    proc_meminfo_ops.read = proc_meminfo_read;
    proc_uptime_ops.read = proc_uptime_read;
    proc_mounts_ops.read = proc_mounts_read;
    proc_cmdline_ops.read = proc_cmdline_read;
    proc_partitions_ops.read = proc_partitions_read;
    proc_net_dev_ops.read = proc_net_dev_read;
    proc_dmi_ops.read = proc_dmi_read;
    proc_pci_ops.read = proc_pci_read;

    pid_status_ops.read = proc_pid_status_read;
    pid_cmdline_ops.read = proc_pid_cmdline_read;
    pid_stat_ops.read = proc_pid_stat_read;
}

struct vfs_node *procfs_mount(void) {
    procfs_init();

    proc_root_node = ramfs_create_root();
    strcpy(proc_root_node->name, "proc");
    proc_root_node->mask = 0555;

    create_proc_file(proc_root_node, "version", &proc_version_ops);
    create_proc_file(proc_root_node, "cpuinfo", &proc_cpuinfo_ops);
    create_proc_file(proc_root_node, "meminfo", &proc_meminfo_ops);
    create_proc_file(proc_root_node, "uptime",  &proc_uptime_ops);
    create_proc_file(proc_root_node, "mounts",  &proc_mounts_ops);
    create_proc_file(proc_root_node, "cmdline", &proc_cmdline_ops);
    create_proc_file(proc_root_node, "partitions", &proc_partitions_ops);
    create_proc_file(proc_root_node, "dmi",     &proc_dmi_ops);
    create_proc_file(proc_root_node, "pci",     &proc_pci_ops);

    struct vfs_node *proc_net_dir = ramfs_create_dir(proc_root_node, "net", 0555);
    if (proc_net_dir) {
        create_proc_file(proc_net_dir, "dev", &proc_net_dev_ops);
    }

    /* Pre-populate PID directories for initial processes */
    for (pid_t i = 1; i <= 16; i++) {
        char pid_str[16];
        ksnprintf(pid_str, sizeof(pid_str), "%d", i);
        struct vfs_node *pdir = ramfs_create_dir(proc_root_node, pid_str, 0555);
        if (pdir) {
            pdir->inode = (uint32_t)i;

            struct vfs_node *f_status = ramfs_create_file(pdir, "status", NULL, 0, 0444);
            if (f_status) { f_status->inode = (uint32_t)i; f_status->ops = &pid_status_ops; }

            struct vfs_node *f_cmdline = ramfs_create_file(pdir, "cmdline", NULL, 0, 0444);
            if (f_cmdline) { f_cmdline->inode = (uint32_t)i; f_cmdline->ops = &pid_cmdline_ops; }

            struct vfs_node *f_stat = ramfs_create_file(pdir, "stat", NULL, 0, 0444);
            if (f_stat) { f_stat->inode = (uint32_t)i; f_stat->ops = &pid_stat_ops; }
        }
    }

    klog(KLOG_INFO, "ProcFS initialized and mounted at /proc\n");
    return proc_root_node;
}
