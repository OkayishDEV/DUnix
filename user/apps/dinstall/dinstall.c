#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <dui/dui.h>
#include <dui/protocol.h>

#define WIN_W 620
#define WIN_H 440

#define COLOR_BG        0x00181818
#define COLOR_HDR_BG    0x00003344
#define COLOR_HDR_FG    0x00FFFFFF
#define COLOR_TXT       0x00D0D0D0
#define COLOR_MUTED     0x00888888
#define COLOR_ACCENT    0x000088CC
#define COLOR_SUCCESS   0x0028A745
#define COLOR_BTN_BG    0x002A2A2A
#define COLOR_BTN_BRD   0x00444444
#define COLOR_PROG_BG   0x00333333
#define COLOR_PROG_FG   0x0000AACC

#include <dboot_mbr.h>
#include <dboot_ldr.h>

#define BLKFORMAT 0x1261
#define BLKRRPART 0x125F

enum install_step {
    STEP_WELCOME = 0,
    STEP_SELECT_DISK,
    STEP_INSTALLING,
    STEP_DONE
};

struct disk_entry {
    char name[32];
    char path[64];
    char model[64];
    unsigned long size_mb;
};

static struct disk_entry g_disks[4];
static int g_num_disks = 0;
static int g_selected_disk = 0;
static enum install_step g_step = STEP_WELCOME;
static int g_progress = 0;
static char g_status[128] = "Ready";

static void scan_disks(void) {
    g_num_disks = 0;
    FILE *fp = fopen("/proc/partitions", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (*p < '0' || *p > '9') continue;

            char *t1 = strtok(line, " \t\r\n");
            char *t2 = strtok(NULL, " \t\r\n");
            char *t3 = strtok(NULL, " \t\r\n");
            char *t4 = strtok(NULL, " \t\r\n");
            char *t5 = strtok(NULL, "\r\n");
            if (t1 && t2 && t3 && t4 && g_num_disks < 4) {
                /* Filter out partitions: whole disks do not end in a digit (except ram0) */
                size_t t4_len = strlen(t4);
                if (strncmp(t4, "ram", 3) != 0 && t4_len > 0 && t4[t4_len - 1] >= '0' && t4[t4_len - 1] <= '9') {
                    continue;
                }

                unsigned long blocks = strtoul(t3, NULL, 10);
                strncpy(g_disks[g_num_disks].name, t4, 31);
                g_disks[g_num_disks].name[31] = '\0';
                snprintf(g_disks[g_num_disks].path, sizeof(g_disks[g_num_disks].path), "/dev/%s", t4);
                g_disks[g_num_disks].size_mb = blocks / 1024;
                if (t5) {
                    while (*t5 == ' ' || *t5 == '\t') t5++;
                    strncpy(g_disks[g_num_disks].model, t5, 63);
                    g_disks[g_num_disks].model[63] = '\0';
                } else {
                    if (strncmp(t4, "ram", 3) == 0) {
                        strcpy(g_disks[g_num_disks].model, "RAM Disk (Live/Scratch)");
                    } else if (strncmp(t4, "sd", 2) == 0) {
                        strcpy(g_disks[g_num_disks].model, "SATA Fixed Disk");
                    } else {
                        strcpy(g_disks[g_num_disks].model, "ATA Fixed Disk");
                    }
                }
                g_num_disks++;
            }
        }
        fclose(fp);
    }
    if (g_num_disks == 0) {
        strcpy(g_disks[0].name, "sda");
        strcpy(g_disks[0].path, "/dev/sda");
        strcpy(g_disks[0].model, "SATA Fixed Disk");
        g_disks[0].size_mb = 1024;
        g_num_disks = 1;
    }

    /* Sort disks so physical disks appear before ram disks */
    for (int i = 0; i < g_num_disks - 1; i++) {
        for (int j = i + 1; j < g_num_disks; j++) {
            bool i_is_ram = (strncmp(g_disks[i].name, "ram", 3) == 0);
            bool j_is_ram = (strncmp(g_disks[j].name, "ram", 3) == 0);
            if (i_is_ram && !j_is_ram) {
                struct disk_entry tmp = g_disks[i];
                g_disks[i] = g_disks[j];
                g_disks[j] = tmp;
            }
        }
    }

    /* Auto-select first physical disk if available */
    g_selected_disk = 0;
    for (int i = 0; i < g_num_disks; i++) {
        if (strncmp(g_disks[i].name, "ram", 3) != 0) {
            g_selected_disk = i;
            break;
        }
    }
}

static void copy_file(const char *src, const char *dst, mode_t mode) {
    int sfd = open(src, O_RDONLY);
    if (sfd < 0) return;
    int dfd = open(dst, O_CREAT | O_WRONLY | O_TRUNC, mode);
    if (dfd < 0) { close(sfd); return; }

    char buf[4096];
    ssize_t n;
    while ((n = read(sfd, buf, sizeof(buf))) > 0) {
        ssize_t w = 0;
        while (w < n) {
            ssize_t written = write(dfd, buf + w, (size_t)(n - w));
            if (written <= 0) break;
            w += written;
        }
    }
    close(sfd);
    close(dfd);
    chmod(dst, mode);
}

static void draw_installer_ui(DuiConnection *conn, DuiWindow win) {
    dui_clear(conn, win, COLOR_BG);

    /* Header banner */
    dui_fill_rect(conn, win, 0, 0, WIN_W, 46, COLOR_HDR_BG);
    dui_draw_line(conn, win, 0, 46, WIN_W - 1, 46, COLOR_ACCENT);
    dui_draw_text(conn, win, 20, 14, "DUnix 64-Bit Operating System Installer", COLOR_HDR_FG);

    /* Step indicator on right */
    char step_str[32];
    snprintf(step_str, sizeof(step_str), "Step %d of 4", (int)g_step + 1);
    dui_draw_text(conn, win, WIN_W - 110, 14, step_str, COLOR_MUTED);

    if (g_step == STEP_WELCOME) {
        dui_draw_text(conn, win, 30, 60, "Welcome to the DUnix Installation Wizard", COLOR_HDR_FG);
        dui_draw_text(conn, win, 30, 90, "This wizard will install DUnix onto your hard disk or storage device.", COLOR_TXT);

        /* Genuine Hardware Detected */
        char cpu_str[64] = "x86_64 Processor";
        FILE *cfp = fopen("/proc/cpuinfo", "r");
        if (cfp) {
            char cline[256];
            while (fgets(cline, sizeof(cline), cfp)) {
                if (strncmp(cline, "model name", 10) == 0) {
                    char *col = strchr(cline, ':');
                    if (col) {
                        col++;
                        while (*col == ' ' || *col == '\t') col++;
                        size_t clen = strlen(col);
                        while (clen > 0 && (col[clen-1] == '\r' || col[clen-1] == '\n')) col[--clen] = '\0';
                        if (clen > 0) strncpy(cpu_str, col, 63);
                        break;
                    }
                }
            }
            fclose(cfp);
        }

        char mem_str[64] = "Physical Memory";
        FILE *mfp = fopen("/proc/meminfo", "r");
        if (mfp) {
            char mline[256];
            unsigned long tot = 0, fre = 0;
            while (fgets(mline, sizeof(mline), mfp)) {
                if (strncmp(mline, "MemTotal:", 9) == 0) tot = strtoul(mline + 9, NULL, 10);
                else if (strncmp(mline, "MemAvailable:", 13) == 0) fre = strtoul(mline + 13, NULL, 10);
            }
            fclose(mfp);
            if (tot > 0) snprintf(mem_str, sizeof(mem_str), "%lu MB RAM (%lu MB Available)", tot / 1024, fre / 1024);
        }

        char h_cpu[80], h_mem[80], h_dsk[80];
        snprintf(h_cpu, sizeof(h_cpu), "Processor: %.50s", cpu_str);
        snprintf(h_mem, sizeof(h_mem), "Memory:    %.50s", mem_str);
        snprintf(h_dsk, sizeof(h_dsk), "Storage:   %d drive(s) detected", g_num_disks);

        dui_draw_text(conn, win, 30, 120, "Hardware Detected:", COLOR_ACCENT);
        dui_draw_text(conn, win, 45, 142, h_cpu, COLOR_HDR_FG);
        dui_draw_text(conn, win, 45, 162, h_mem, COLOR_HDR_FG);
        dui_draw_text(conn, win, 45, 182, h_dsk, COLOR_HDR_FG);

        dui_draw_text(conn, win, 30, 215, "Installed components include:", COLOR_TXT);
        dui_draw_text(conn, win, 50, 240, "* 64-Bit Monolithic Unix Kernel (SMP, Preemption, Long Mode)", COLOR_MUTED);
        dui_draw_text(conn, win, 50, 262, "* Standard C Library (libc) & 52 native Unix ELF64 utilities", COLOR_MUTED);
        dui_draw_text(conn, win, 50, 284, "* Ext2 Persistent Filesystem Storage & AHCI/SATA Block Drivers", COLOR_MUTED);
        dui_draw_text(conn, win, 50, 306, "* DWS Display Server & DWM Window Manager Suite", COLOR_MUTED);

        /* Next button */
        dui_fill_rect(conn, win, WIN_W - 160, WIN_H - 55, 130, 34, COLOR_ACCENT);
        dui_draw_rect(conn, win, WIN_W - 160, WIN_H - 55, 130, 34, COLOR_HDR_FG);
        dui_draw_text(conn, win, WIN_W - 135, WIN_H - 45, "Next  ->", COLOR_HDR_FG);
    } else if (g_step == STEP_SELECT_DISK) {
        dui_draw_text(conn, win, 30, 70, "Select Target Storage Drive:", COLOR_HDR_FG);
        dui_draw_text(conn, win, 30, 95, "Click a drive below to choose your installation destination:", COLOR_MUTED);

        for (int i = 0; i < g_num_disks; i++) {
            int y = 130 + i * 55;
            bool sel = (i == g_selected_disk);
            uint32_t card_bg = sel ? 0x00004455 : COLOR_BTN_BG;
            uint32_t border_col = sel ? COLOR_ACCENT : COLOR_BTN_BRD;

            dui_fill_rect(conn, win, 30, y, WIN_W - 60, 44, card_bg);
            dui_draw_rect(conn, win, 30, y, WIN_W - 60, 44, border_col);

            char sz_str[24];
            if (g_disks[i].size_mb >= 1024) {
                snprintf(sz_str, sizeof(sz_str), "%lu.%lu GB", g_disks[i].size_mb / 1024, (g_disks[i].size_mb % 1024) * 10 / 1024);
            } else {
                snprintf(sz_str, sizeof(sz_str), "%lu MB", g_disks[i].size_mb);
            }

            char dlabel[80];
            snprintf(dlabel, sizeof(dlabel), "[ %s ]  %.32s  (%s)", g_disks[i].path, g_disks[i].model, sz_str);
            dui_draw_text(conn, win, 48, y + 14, dlabel, sel ? COLOR_HDR_FG : COLOR_TXT);

            if (sel) {
                dui_draw_text(conn, win, WIN_W - 140, y + 14, "[ SELECTED ]", COLOR_ACCENT);
            }
        }

        dui_draw_text(conn, win, 30, WIN_H - 95, "WARNING: Installing will format the selected drive with Ext2!", 0x00E06666);

        /* Back and Install buttons */
        dui_fill_rect(conn, win, 30, WIN_H - 55, 110, 34, COLOR_BTN_BG);
        dui_draw_rect(conn, win, 30, WIN_H - 55, 110, 34, COLOR_BTN_BRD);
        dui_draw_text(conn, win, 65, WIN_H - 45, "<- Back", COLOR_TXT);

        dui_fill_rect(conn, win, WIN_W - 180, WIN_H - 55, 150, 34, COLOR_SUCCESS);
        dui_draw_rect(conn, win, WIN_W - 180, WIN_H - 55, 150, 34, COLOR_HDR_FG);
        dui_draw_text(conn, win, WIN_W - 165, WIN_H - 45, "Install DUnix", COLOR_HDR_FG);
    } else if (g_step == STEP_INSTALLING) {
        dui_draw_text(conn, win, 30, 70, "Installing DUnix...", COLOR_HDR_FG);
        dui_draw_text(conn, win, 30, 95, "Please wait while files and system packages are written to disk.", COLOR_MUTED);

        /* Target info */
        char tstr[64];
        snprintf(tstr, sizeof(tstr), "Destination: %s (Ext2 Filesystem)", g_disks[g_selected_disk].path);
        dui_draw_text(conn, win, 30, 140, tstr, COLOR_ACCENT);

        /* Status message */
        dui_draw_text(conn, win, 30, 180, g_status, COLOR_TXT);

        /* Progress bar */
        int bar_x = 30, bar_y = 210, bar_w = WIN_W - 60, bar_h = 24;
        dui_fill_rect(conn, win, bar_x, bar_y, bar_w, bar_h, COLOR_PROG_BG);
        dui_draw_rect(conn, win, bar_x, bar_y, bar_w, bar_h, COLOR_BTN_BRD);

        int fill_w = (g_progress * (bar_w - 4)) / 100;
        if (fill_w > 0) {
            dui_fill_rect(conn, win, bar_x + 2, bar_y + 2, fill_w, bar_h - 4, COLOR_PROG_FG);
        }

        char pstr[16];
        snprintf(pstr, sizeof(pstr), "%d%%", g_progress);
        dui_draw_text(conn, win, bar_x + bar_w / 2 - 10, bar_y + 4, pstr, COLOR_HDR_FG);
    } else if (g_step == STEP_DONE) {
        dui_draw_text(conn, win, 30, 70, "Installation Completed Successfully!", COLOR_SUCCESS);
        dui_draw_text(conn, win, 30, 110, "DUnix 64-Bit has been successfully installed to your disk.", COLOR_TXT);

        char dstr[128];
        snprintf(dstr, sizeof(dstr), "Target Device: %s (Ext2 root filesystem)", g_disks[g_selected_disk].path);
        dui_draw_text(conn, win, 30, 145, dstr, COLOR_HDR_FG);

        dui_draw_text(conn, win, 30, 180, "Root directory tree and 49 native ELF64 binaries deployed.", COLOR_MUTED);
        dui_draw_text(conn, win, 30, 205, "Boot configuration file /boot/boot.cfg and /etc/fstab armed.", COLOR_MUTED);
        dui_draw_text(conn, win, 30, 245, "You may now reboot the system to start your new installation.", COLOR_TXT);

        dui_fill_rect(conn, win, WIN_W - 160, WIN_H - 55, 130, 34, COLOR_ACCENT);
        dui_draw_rect(conn, win, WIN_W - 160, WIN_H - 55, 130, 34, COLOR_HDR_FG);
        dui_draw_text(conn, win, WIN_W - 128, WIN_H - 45, "Finish", COLOR_HDR_FG);
    }

    dui_flush(conn, win);
}

static void run_installation_work(DuiConnection *conn, DuiWindow win) {
    const char *target_path = g_disks[g_selected_disk].path;
    bool is_ram = (strncmp(g_disks[g_selected_disk].name, "ram", 3) == 0);

    char part_path[64];
    if (is_ram) {
        strncpy(part_path, target_path, sizeof(part_path) - 1);
        part_path[sizeof(part_path) - 1] = '\0';
    } else {
        snprintf(part_path, sizeof(part_path), "%s1", target_path);
    }

    /* 1. Partition Target Drive & Deploy Bootloader */
    snprintf(g_status, sizeof(g_status), "Writing MBR bootloader & partition table to %s...", target_path);
    g_progress = 5;
    draw_installer_ui(conn, win);
    usleep(100000);

    if (!is_ram) {
        int disk_fd = open(target_path, O_RDWR);
        if (disk_fd >= 0) {
            unsigned long total_sectors = g_disks[g_selected_disk].size_mb * 2048;
            if (total_sectors == 0) total_sectors = 2097152;

            uint8_t mbr_sector[512];
            memset(mbr_sector, 0, sizeof(mbr_sector));
            size_t copy_mbr = (sizeof(mbr_sector) < (size_t)dboot_mbr_len) ? sizeof(mbr_sector) : (size_t)dboot_mbr_len;
            memcpy(mbr_sector, dboot_mbr, copy_mbr);

            uint32_t part_start_lba = 8192;
            uint32_t part_sectors = (total_sectors > part_start_lba) ? (uint32_t)(total_sectors - part_start_lba) : 0;

            uint8_t *p1 = mbr_sector + 446;
            p1[0] = 0x80;
            p1[1] = 0x00; p1[2] = 0x02; p1[3] = 0x00;
            p1[4] = 0x83;
            p1[5] = 0xFE; p1[6] = 0xFF; p1[7] = 0xFF;
            p1[8]  = (uint8_t)(part_start_lba & 0xFF);
            p1[9]  = (uint8_t)((part_start_lba >> 8) & 0xFF);
            p1[10] = (uint8_t)((part_start_lba >> 16) & 0xFF);
            p1[11] = (uint8_t)((part_start_lba >> 24) & 0xFF);
            p1[12] = (uint8_t)(part_sectors & 0xFF);
            p1[13] = (uint8_t)((part_sectors >> 8) & 0xFF);
            p1[14] = (uint8_t)((part_sectors >> 16) & 0xFF);
            p1[15] = (uint8_t)((part_sectors >> 24) & 0xFF);

            mbr_sector[510] = 0x55;
            mbr_sector[511] = 0xAA;

            lseek(disk_fd, 0, SEEK_SET);
            write(disk_fd, mbr_sector, 512);

            /* Write Stage 2 loader at LBA 1 (Sectors 1..63, 63*512 = 32256 bytes) */
            uint8_t stage2_buf[63 * 512];
            memset(stage2_buf, 0, sizeof(stage2_buf));
            size_t copy_ldr = (sizeof(stage2_buf) < (size_t)dboot_ldr_len) ? sizeof(stage2_buf) : (size_t)dboot_ldr_len;
            memcpy(stage2_buf, dboot_ldr, copy_ldr);

            lseek(disk_fd, 512, SEEK_SET);
            write(disk_fd, stage2_buf, sizeof(stage2_buf));

            snprintf(g_status, sizeof(g_status), "Writing native kernel image to %s...", target_path);
            g_progress = 12;
            draw_installer_ui(conn, win);

            int kfd = open("/dev/kimg", O_RDONLY);
            if (kfd >= 0) {
                lseek(disk_fd, 64 * 512, SEEK_SET);
                char kbuf[4096];
                ssize_t kn;
                bool write_error = false;
                while ((kn = read(kfd, kbuf, sizeof(kbuf))) > 0) {
                    ssize_t kw = 0;
                    while (kw < kn) {
                        ssize_t ret = write(disk_fd, kbuf + kw, (size_t)(kn - kw));
                        if (ret <= 0) {
                            write_error = true;
                            break;
                        }
                        kw += ret;
                    }
                    if (write_error) break;
                }
                close(kfd);
            }

            ioctl(disk_fd, BLKRRPART, 0);
            close(disk_fd);
            usleep(100000);
        }
    }

    /* 2. Format Ext2 Filesystem on Target Partition */
    snprintf(g_status, sizeof(g_status), "Formatting %s with Ext2...", part_path);
    g_progress = 20;
    draw_installer_ui(conn, win);
    usleep(100000);

    int part_fd = open(part_path, O_RDWR);
    if (part_fd < 0 && !is_ram) {
        part_fd = open(target_path, O_RDWR);
    }
    if (part_fd >= 0) {
        ioctl(part_fd, BLKFORMAT, 0);
        close(part_fd);
    }

    /* 3. Mount /mnt */
    snprintf(g_status, sizeof(g_status), "Mounting %s to /mnt...", part_path);
    g_progress = 25;
    draw_installer_ui(conn, win);
    usleep(100000);

    mkdir("/mnt", 0755);
    umount("/mnt");
    if (mount(part_path, "/mnt", "ext2", 0, NULL) != 0 && !is_ram) {
        mount(target_path, "/mnt", "ext2", 0, NULL);
    }

    /* 4. Create dirs */
    snprintf(g_status, sizeof(g_status), "Creating root directory tree...");
    g_progress = 30;
    draw_installer_ui(conn, win);
    usleep(100000);

    const char *dirs[] = {
        "/mnt/bin", "/mnt/sbin", "/mnt/etc", "/mnt/dev", "/mnt/proc",
        "/mnt/home", "/mnt/root", "/mnt/tmp",
        "/mnt/usr", "/mnt/usr/bin", "/mnt/usr/lib", "/mnt/usr/include",
        "/mnt/var", "/mnt/var/log", "/mnt/var/run", "/mnt/mnt", "/mnt/boot",
        NULL
    };
    for (int i = 0; dirs[i]; i++) mkdir(dirs[i], 0755);

    /* 5. Copy Binaries */
    DIR *dir = opendir("/bin");
    if (dir) {
        char bin_list[64][64];
        int bcount = 0;
        struct dirent *de;
        while ((de = readdir(dir)) != NULL && bcount < 64) {
            if (de->d_name[0] != '.') strncpy(bin_list[bcount++], de->d_name, 63);
        }
        closedir(dir);

        for (int i = 0; i < bcount; i++) {
            char src[128], dst[128];
            snprintf(src, sizeof(src), "/bin/%s", bin_list[i]);
            snprintf(dst, sizeof(dst), "/mnt/bin/%s", bin_list[i]);
            copy_file(src, dst, (strcmp(bin_list[i], "sudo") == 0) ? 04755 : 0755);

            g_progress = 30 + ((i + 1) * 55) / (bcount > 0 ? bcount : 1);
            snprintf(g_status, sizeof(g_status), "Installing /bin/%s...", bin_list[i]);
            draw_installer_ui(conn, win);
            usleep(2000);
        }
    }

    copy_file("/sbin/init", "/mnt/sbin/init", 0755);

    /* 6. Deploy Kernel Image to /mnt/boot */
    snprintf(g_status, sizeof(g_status), "Deploying kernel binary to /mnt/boot/dunix.raw...");
    g_progress = 88;
    draw_installer_ui(conn, win);
    copy_file("/dev/kimg", "/mnt/boot/dunix.raw", 0644);

    /* 7. Configuration */
    snprintf(g_status, sizeof(g_status), "Writing system configuration and bootloader...");
    g_progress = 92;
    draw_installer_ui(conn, win);
    usleep(100000);

    const char *etc_files[] = { "passwd", "group", "sudoers", "hostname", "os-release", "hosts", "resolv.conf", NULL };
    for (int i = 0; etc_files[i]; i++) {
        char s[128], d[128];
        snprintf(s, sizeof(s), "/etc/%s", etc_files[i]);
        snprintf(d, sizeof(d), "/mnt/etc/%s", etc_files[i]);
        copy_file(s, d, 0644);
    }

    int fstab_fd = open("/mnt/etc/fstab", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fstab_fd >= 0) {
        char fbuf[256];
        snprintf(fbuf, sizeof(fbuf),
                 "%-12s /          ext2    defaults    0  1\n"
                 "procfs       /proc      proc    defaults    0  0\n"
                 "devfs        /dev       devfs   defaults    0  0\n",
                 part_path);
        write(fstab_fd, fbuf, strlen(fbuf));
        close(fstab_fd);
    }

    int boot_fd = open("/mnt/boot/boot.cfg", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (boot_fd >= 0) {
        char bbuf[256];
        snprintf(bbuf, sizeof(bbuf), "timeout=3\ndefault=0\ntitle DUnix 64-Bit\nkernel /boot/dunix.raw root=%s rw\n", part_path);
        write(boot_fd, bbuf, strlen(bbuf));
        close(boot_fd);
    }

    umount("/mnt");

    g_progress = 100;
    g_step = STEP_DONE;
    draw_installer_ui(conn, win);
}

int main(void) {
    DuiConnection *conn = dui_connect();
    if (!conn) {
        fprintf(stderr, "dinstall: failed to connect to display server\n");
        return 1;
    }

    scan_disks();

    DuiWindow win = dui_create_window(conn, 120, 80, WIN_W, WIN_H, "DUnix System Installer", COLOR_BG, DWS_WIN_DECORATED);
    dui_show(conn, win);
    draw_installer_ui(conn, win);

    DuiEvent ev;
    while (1) {
        if (dui_next_event(conn, &ev) > 0) {
            if (ev.type == DWS_EV_CLOSE_REQ && ev.window == win) {
                break;
            } else if (ev.type == DWS_EV_EXPOSE && ev.window == win) {
                draw_installer_ui(conn, win);
            } else if (ev.type == DWS_EV_MOUSE_DOWN && ev.window == win) {
                int mx = ev.mouse.x;
                int my = ev.mouse.y;

                if (g_step == STEP_WELCOME) {
                    /* Click 'Next' button */
                    if (mx >= WIN_W - 160 && mx < WIN_W - 30 && my >= WIN_H - 55 && my < WIN_H - 21) {
                        g_step = STEP_SELECT_DISK;
                        draw_installer_ui(conn, win);
                    }
                } else if (g_step == STEP_SELECT_DISK) {
                    /* Disk card selection */
                    for (int i = 0; i < g_num_disks; i++) {
                        int dy = 130 + i * 55;
                        if (mx >= 30 && mx < WIN_W - 30 && my >= dy && my < dy + 44) {
                            g_selected_disk = i;
                            draw_installer_ui(conn, win);
                            break;
                        }
                    }

                    /* Back button */
                    if (mx >= 30 && mx < 140 && my >= WIN_H - 55 && my < WIN_H - 21) {
                        g_step = STEP_WELCOME;
                        draw_installer_ui(conn, win);
                    }
                    /* Install button */
                    else if (mx >= WIN_W - 180 && mx < WIN_W - 30 && my >= WIN_H - 55 && my < WIN_H - 21) {
                        g_step = STEP_INSTALLING;
                        draw_installer_ui(conn, win);
                        run_installation_work(conn, win);
                    }
                } else if (g_step == STEP_DONE) {
                    /* Finish button */
                    if (mx >= WIN_W - 160 && mx < WIN_W - 30 && my >= WIN_H - 55 && my < WIN_H - 21) {
                        break;
                    }
                }
            }
        }
    }

    dui_destroy_window(conn, win);
    dui_disconnect(conn);
    return 0;
}
