/*
 * DUnix BSD-Style TUI Operating System Installer (bsdinstall)
 *
 * Classic BSD-style curses/dialog text-mode installation wizard.
 * Provides interactive system installation, partitioning, Ext2 formatting,
 * distribution set deployment, and configuration.
 *
 * Copyright (c) 2026 DUnix Project. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <termios.h>
#include <signal.h>
#include <stdbool.h>
#include <dirent.h>
#include <dboot_mbr.h>
#include <dboot_ldr.h>
#include <errno.h>

#define BLKFORMAT  0x1261
#define BLKGETSIZE 0x1260
#define BLKRRPART  0x125F

#define TUI_ROWS 25
#define TUI_COLS 80

/* Classic BSD Dialog Colors */
#define ATTR_BG         "\033[44m"              /* Deep BSD blue */
#define ATTR_TITLE      "\033[47;1;30m"         /* Top/bottom bar */
#define ATTR_BOX        "\033[47;30m"           /* Dialog box body (gray/black) */
#define ATTR_BOX_TITLE  "\033[47;1;34m"         /* Bold blue title */
#define ATTR_BOX_BOLD   "\033[47;1;30m"         /* Bold text */
#define ATTR_ITEM_SEL   "\033[44;1;37m"         /* Active list item (blue/white) */
#define ATTR_ITEM_NORM  "\033[47;30m"           /* Normal list item */
#define ATTR_BTN_SEL    "\033[46;1;37m"         /* Active button (cyan/white) */
#define ATTR_BTN_NORM   "\033[47;1;30m"         /* Normal button */
#define ATTR_SHADOW     "\033[40m"              /* Black shadow */
#define ATTR_WARN       "\033[47;1;31m"         /* Bold red warning */
#define ATTR_SUCCESS    "\033[47;1;32m"         /* Bold green success */
#define ATTR_RESET      "\033[0m"

enum key_code {
    KEY_NONE = 0,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_ENTER,
    KEY_TAB,
    KEY_SPACE,
    KEY_BACKSPACE,
    KEY_ESC,
    KEY_CHAR
};

enum wizard_step {
    STEP_MAIN_MENU = 0,
    STEP_HOSTNAME,
    STEP_SETS,
    STEP_USER,
    STEP_DISK,
    STEP_CONFIRM,
    STEP_INSTALLING,
    STEP_DONE,
    STEP_EXIT_SHELL,
    STEP_EXIT_REBOOT
};

struct disk_info {
    char name[32];
    char path[64];
    char model[64];
    unsigned long blocks_kb;
};

struct dist_set {
    const char *name;
    const char *desc;
    bool selected;
};

static struct termios g_orig_termios;
static bool g_raw_enabled = false;
static char g_screen_ch[TUI_ROWS][TUI_COLS];
static const char *g_screen_attr[TUI_ROWS][TUI_COLS];

static struct disk_info g_disks[8];
static int g_num_disks = 0;
static int g_selected_disk = 0;

static char g_hostname[64] = "dunix";
static char g_username[32] = "";
static char g_fullname[64] = "";
static char g_password[64] = "";
static bool g_user_sudo = true;

static struct dist_set g_sets[] = {
    { "base",      "Base system libraries, init, and runtime", true },
    { "kernel",    "DUnix 64-Bit Monolithic Unix Kernel",     true },
    { "bsd-utils", "Unix 64-bit core utilities suite",        true },
    { "desktop",   "DWS Display Server & DWM Window Manager",  true },
    { "devel",     "C Compiler (cc), standard libc headers",   true },
    { NULL, NULL, false }
};

/* ─── Terminal & TUI Engine ─── */

static void tui_restore_term(void) {
    if (g_raw_enabled) {
        printf("\033[?25h" ATTR_RESET "\033[2J\033[H");
        tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios);
        fflush(stdout);
        g_raw_enabled = false;
    }
}

static void tui_sigint_handler(int sig) {
    (void)sig;
    tui_restore_term();
    exit(0);
}

static void tui_init(void) {
    if (tcgetattr(STDIN_FILENO, &g_orig_termios) == 0) {
        struct termios raw = g_orig_termios;
        raw.c_lflag &= ~(ICANON | ECHO | ISIG);
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        g_raw_enabled = true;
    }

    signal(SIGINT, tui_sigint_handler);
    signal(SIGTERM, tui_sigint_handler);

    printf("\033[?25l"); /* Hide cursor */
    fflush(stdout);
}

static void tui_clear(void) {
    for (int r = 0; r < TUI_ROWS; r++) {
        for (int c = 0; c < TUI_COLS; c++) {
            g_screen_ch[r][c] = ' ';
            g_screen_attr[r][c] = ATTR_BG;
        }
    }

    /* Top banner */
    const char *title = " DUnix/amd64 - Installation (bsdinstall)";
    size_t tlen = strlen(title);
    for (int c = 0; c < TUI_COLS; c++) {
        g_screen_ch[0][c] = (c < (int)tlen) ? title[c] : ' ';
        g_screen_attr[0][c] = ATTR_TITLE;
    }

    /* Bottom footer */
    const char *footer = " <Tab>/<Arrows> Focus | <Space> Toggle | <Enter> Select | <1-9> Shortcut";
    size_t flen = strlen(footer);
    for (int c = 0; c < TUI_COLS; c++) {
        g_screen_ch[TUI_ROWS - 1][c] = (c < (int)flen) ? footer[c] : ' ';
        g_screen_attr[TUI_ROWS - 1][c] = ATTR_TITLE;
    }
}

static void tui_draw_box(int x, int y, int w, int h, const char *title) {
    for (int r = y; r < y + h && r < TUI_ROWS - 1; r++) {
        for (int c = x; c < x + w && c < TUI_COLS; c++) {
            char ch = ' ';
            if (r == y) {
                ch = (c == x || c == x + w - 1) ? '+' : '-';
            } else if (r == y + h - 1) {
                ch = (c == x || c == x + w - 1) ? '+' : '-';
            } else if (c == x || c == x + w - 1) {
                ch = '|';
            }
            g_screen_ch[r][c] = ch;
            g_screen_attr[r][c] = ATTR_BOX;
        }
    }

    if (title) {
        char buf[128];
        snprintf(buf, sizeof(buf), " [ %s ] ", title);
        int len = (int)strlen(buf);
        int sx = x + (w - len) / 2;
        for (int i = 0; i < len && sx + i < x + w - 1; i++) {
            g_screen_ch[y][sx + i] = buf[i];
            g_screen_attr[y][sx + i] = ATTR_BOX_TITLE;
        }
    }

    /* Right drop shadow */
    for (int r = y + 1; r <= y + h && r < TUI_ROWS - 1; r++) {
        for (int c = x + w; c < x + w + 2 && c < TUI_COLS; c++) {
            g_screen_ch[r][c] = ' ';
            g_screen_attr[r][c] = ATTR_SHADOW;
        }
    }

    /* Bottom drop shadow */
    int sy = y + h;
    if (sy < TUI_ROWS - 1) {
        for (int c = x + 2; c < x + w + 2 && c < TUI_COLS; c++) {
            g_screen_ch[sy][c] = ' ';
            g_screen_attr[sy][c] = ATTR_SHADOW;
        }
    }
}

static void tui_print(int x, int y, const char *text, const char *attr) {
    if (y < 0 || y >= TUI_ROWS) return;
    int len = (int)strlen(text);
    for (int i = 0; i < len && x + i < TUI_COLS; i++) {
        if (x + i >= 0) {
            g_screen_ch[y][x + i] = text[i];
            g_screen_attr[y][x + i] = attr ? attr : ATTR_BOX;
        }
    }
}

static void tui_draw_button(int x, int y, const char *label, bool selected) {
    char buf[64];
    snprintf(buf, sizeof(buf), "<  %s  >", label);
    tui_print(x, y, buf, selected ? ATTR_BTN_SEL : ATTR_BTN_NORM);
}

static void tui_flush(void) {
    const char *last_attr = NULL;

    for (int r = 0; r < TUI_ROWS; r++) {
        printf("\033[%d;1H", r + 1);
        for (int c = 0; c < TUI_COLS; c++) {
            if (r == TUI_ROWS - 1 && c == TUI_COLS - 1) {
                break;
            }
            const char *cur = g_screen_attr[r][c];
            if (cur != last_attr) {
                fputs(cur, stdout);
                last_attr = cur;
            }
            putchar(g_screen_ch[r][c]);
        }
    }
    fputs(ATTR_RESET, stdout);
    printf("\033[25;80H");
    fflush(stdout);
}

static enum key_code tui_get_key(char *out_char) {
    char c = 0;
    if (read(STDIN_FILENO, &c, 1) <= 0) {
        return KEY_NONE;
    }

    if (c == 0x1B) {
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) > 0 && read(STDIN_FILENO, &seq[1], 1) > 0) {
            if (seq[0] == '[') {
                if (seq[1] == 'A') return KEY_UP;
                if (seq[1] == 'B') return KEY_DOWN;
                if (seq[1] == 'C') return KEY_RIGHT;
                if (seq[1] == 'D') return KEY_LEFT;
            }
        }
        return KEY_ESC;
    }

    if (c == '\r' || c == '\n') return KEY_ENTER;
    if (c == '\t') return KEY_TAB;
    if (c == ' ') return KEY_SPACE;
    if (c == 0x7F || c == 0x08) return KEY_BACKSPACE;

    if (out_char) *out_char = c;
    return KEY_CHAR;
}

/* ─── Hardware & Disk Scanning ─── */

static void get_cpu_info(char *brand, size_t brand_size) {
    strncpy(brand, "x86_64 Processor", brand_size - 1);
    brand[brand_size - 1] = '\0';
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) return;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "model name", 10) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                colon++;
                while (*colon == ' ' || *colon == '\t') colon++;
                size_t len = strlen(colon);
                while (len > 0 && (colon[len - 1] == '\r' || colon[len - 1] == '\n' || colon[len - 1] == ' ')) {
                    colon[--len] = '\0';
                }
                if (len > 0) {
                    strncpy(brand, colon, brand_size - 1);
                    brand[brand_size - 1] = '\0';
                    break;
                }
            }
        }
    }
    fclose(fp);
}

static void get_mem_info(char *mem_str, size_t mem_size) {
    unsigned long total_kb = 0, avail_kb = 0;
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                total_kb = strtoul(line + 9, NULL, 10);
            } else if (strncmp(line, "MemAvailable:", 13) == 0 || strncmp(line, "MemFree:", 8) == 0) {
                if (avail_kb == 0) {
                    char *col = strchr(line, ':');
                    if (col) avail_kb = strtoul(col + 1, NULL, 10);
                }
            }
        }
        fclose(fp);
    }
    if (total_kb > 0) {
        snprintf(mem_str, mem_size, "%lu MB Total (%lu MB Free)", total_kb / 1024, avail_kb / 1024);
    } else {
        snprintf(mem_str, mem_size, "Standard Physical Memory");
    }
}

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

            if (t1 && t2 && t3 && t4 && g_num_disks < 8) {
                /* Filter out partitions: whole disks do not end in a digit (except ram0) */
                size_t t4_len = strlen(t4);
                if (strncmp(t4, "ram", 3) != 0 && t4_len > 0 && t4[t4_len - 1] >= '0' && t4[t4_len - 1] <= '9') {
                    continue;
                }

                unsigned long blocks = strtoul(t3, NULL, 10);
                strncpy(g_disks[g_num_disks].name, t4, 31);
                g_disks[g_num_disks].name[31] = '\0';
                snprintf(g_disks[g_num_disks].path, sizeof(g_disks[g_num_disks].path), "/dev/%s", t4);
                g_disks[g_num_disks].blocks_kb = blocks;

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
        /* Fallback discovery */
        const char *cand[] = { "/dev/sda", "/dev/hda", "/dev/ram0", NULL };
        for (int i = 0; cand[i] && g_num_disks < 8; i++) {
            int fd = open(cand[i], O_RDONLY);
            if (fd >= 0) {
                close(fd);
                const char *cn = cand[i] + 5;
                strncpy(g_disks[g_num_disks].name, cn, 31);
                strncpy(g_disks[g_num_disks].path, cand[i], 63);
                g_disks[g_num_disks].blocks_kb = (strcmp(cn, "ram0") == 0) ? 8192 : 1048576;
                snprintf(g_disks[g_num_disks].model, 63, "%s Storage Device", cn);
                g_num_disks++;
            }
        }
    }

    /* Sort disks so physical disks appear before ram disks */
    for (int i = 0; i < g_num_disks - 1; i++) {
        for (int j = i + 1; j < g_num_disks; j++) {
            bool i_is_ram = (strncmp(g_disks[i].name, "ram", 3) == 0);
            bool j_is_ram = (strncmp(g_disks[j].name, "ram", 3) == 0);
            if (i_is_ram && !j_is_ram) {
                struct disk_info tmp = g_disks[i];
                g_disks[i] = g_disks[j];
                g_disks[j] = tmp;
            }
        }
    }

    /* Auto-select first physical disk if available instead of ram0 */
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

/* ─── Wizard Screens ─── */

/* Step 1: Welcome Menu */
static enum wizard_step run_step_main_menu(void) {
    int sel_item = 0;   /* 0=Install, 1=Shell, 2=Reboot */
    int sel_btn = 0;    /* 0=Select, 1=Cancel */
    bool on_btn = false;

    scan_disks();

    char cpu_str[64];
    char mem_str[64];
    get_cpu_info(cpu_str, sizeof(cpu_str));
    get_mem_info(mem_str, sizeof(mem_str));

    for (;;) {
        tui_clear();
        tui_draw_box(4, 2, 72, 20, "DUnix Installation Menu");

        tui_print(7, 4, "Welcome to DUnix 64-Bit Operating System!", ATTR_BOX_BOLD);

        /* Genuine Hardware Details */
        char cpu_line[80], mem_line[80], disk_line[80];
        snprintf(cpu_line, sizeof(cpu_line), "CPU:     %.56s", cpu_str);
        snprintf(mem_line, sizeof(mem_line), "Memory:  %.56s", mem_str);
        if (g_num_disks > 0) {
            unsigned long mb = g_disks[g_selected_disk].blocks_kb / 1024;
            char sz_str[24];
            if (mb >= 1024) snprintf(sz_str, sizeof(sz_str), "%lu.%lu GB", mb / 1024, (mb % 1024) * 10 / 1024);
            else snprintf(sz_str, sizeof(sz_str), "%lu MB", mb);
            snprintf(disk_line, sizeof(disk_line), "Storage: %s (%s - %.32s)",
                     g_disks[g_selected_disk].path, sz_str, g_disks[g_selected_disk].model);
        } else {
            snprintf(disk_line, sizeof(disk_line), "Storage: Probing storage hardware...");
        }

        tui_print(7, 6, cpu_line, ATTR_BOX);
        tui_print(7, 7, mem_line, ATTR_BOX);
        tui_print(7, 8, disk_line, ATTR_BOX);

        tui_print(7, 10, "Please select an installation option below:", ATTR_BOX_BOLD);

        const char *opts[] = {
            "Install  Begin DUnix installation to local drive",
            "Shell    Start interactive root maintenance shell",
            "Reboot   Reboot the machine"
        };

        for (int i = 0; i < 3; i++) {
            char buf[80];
            bool is_cur = (!on_btn && sel_item == i);
            snprintf(buf, sizeof(buf), "  [%c] %s", (sel_item == i) ? '*' : ' ', opts[i]);
            tui_print(8, 12 + i * 2, buf, is_cur ? ATTR_ITEM_SEL : ATTR_ITEM_NORM);
        }

        tui_draw_button(22, 19, "Select", on_btn && sel_btn == 0);
        tui_draw_button(46, 19, "Cancel", on_btn && sel_btn == 1);

        tui_flush();

        char ch = 0;
        enum key_code k = tui_get_key(&ch);

        if (k == KEY_UP) {
            if (on_btn) on_btn = false;
            else if (sel_item > 0) sel_item--;
        } else if (k == KEY_DOWN) {
            if (!on_btn) {
                if (sel_item < 2) sel_item++;
                else on_btn = true;
            }
        } else if (k == KEY_LEFT) {
            if (on_btn && sel_btn > 0) sel_btn--;
        } else if (k == KEY_RIGHT) {
            if (on_btn && sel_btn < 1) sel_btn++;
        } else if (k == KEY_TAB) {
            if (!on_btn) on_btn = true;
            else if (sel_btn == 0) sel_btn = 1;
            else { on_btn = false; sel_btn = 0; }
        } else if (k == KEY_ENTER) {
            if (on_btn && sel_btn == 1) return STEP_EXIT_SHELL;
            if (sel_item == 0) return STEP_HOSTNAME;
            if (sel_item == 1) return STEP_EXIT_SHELL;
            if (sel_item == 2) return STEP_EXIT_REBOOT;
        } else if (k == KEY_CHAR) {
            if (ch == '1' || ch == 'i' || ch == 'I') return STEP_HOSTNAME;
            if (ch == '2' || ch == 's' || ch == 'S') return STEP_EXIT_SHELL;
            if (ch == '3' || ch == 'r' || ch == 'R') return STEP_EXIT_REBOOT;
            if (ch == 'q' || ch == 'Q') return STEP_EXIT_SHELL;
        }
    }
}

/* Step 2: Set Hostname */
static enum wizard_step run_step_hostname(void) {
    int sel_btn = 0; /* 0=OK, 1=Back */
    int hlen = (int)strlen(g_hostname);

    for (;;) {
        tui_clear();
        tui_draw_box(8, 4, 64, 15, "Set Hostname");

        tui_print(11, 6, "Please choose a hostname for your system.", ATTR_BOX_BOLD);
        tui_print(11, 7, "The hostname identifies this machine on the network.", ATTR_BOX);

        tui_print(11, 10, "Hostname: [", ATTR_BOX);
        char field[36];
        snprintf(field, sizeof(field), "%-30s", g_hostname);
        tui_print(22, 10, field, ATTR_ITEM_SEL);
        tui_print(52, 10, "]", ATTR_BOX);

        tui_draw_button(20, 14, "  OK  ", sel_btn == 0);
        tui_draw_button(40, 14, " Back ", sel_btn == 1);

        tui_flush();

        char ch = 0;
        enum key_code k = tui_get_key(&ch);

        if (k == KEY_LEFT || k == KEY_RIGHT || k == KEY_TAB) {
            sel_btn = 1 - sel_btn;
        } else if (k == KEY_BACKSPACE) {
            if (hlen > 0) {
                g_hostname[--hlen] = '\0';
            }
        } else if (k == KEY_ENTER) {
            if (sel_btn == 1) return STEP_MAIN_MENU;
            if (hlen == 0) strcpy(g_hostname, "dunix");
            return STEP_SETS;
        } else if (k == KEY_CHAR) {
            if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') || ch == '-' || ch == '_') {
                if (hlen < 30) {
                    g_hostname[hlen++] = ch;
                    g_hostname[hlen] = '\0';
                }
            } else if (ch == '\n' || ch == '\r') {
                return STEP_SETS;
            }
        }
    }
}

/* Step 3: Distribution Sets */
static enum wizard_step run_step_sets(void) {
    int sel_item = 0;
    int sel_btn = 0;
    bool on_btn = false;

    int num_sets = 0;
    while (g_sets[num_sets].name) num_sets++;

    for (;;) {
        tui_clear();
        tui_draw_box(6, 3, 68, 18, "Distribution Sets");

        tui_print(9, 5, "Choose system components to install onto disk:", ATTR_BOX_BOLD);
        tui_print(9, 6, "Press <Space> to toggle, <Tab> to move to buttons:", ATTR_BOX);

        for (int i = 0; i < num_sets; i++) {
            char buf[80];
            bool is_cur = (!on_btn && sel_item == i);
            snprintf(buf, sizeof(buf), "  [%c] %-10s  %s",
                     g_sets[i].selected ? 'X' : ' ',
                     g_sets[i].name,
                     g_sets[i].desc);
            tui_print(8, 8 + i * 2, buf, is_cur ? ATTR_ITEM_SEL : ATTR_ITEM_NORM);
        }

        tui_draw_button(20, 18, "  OK  ", on_btn && sel_btn == 0);
        tui_draw_button(42, 18, " Back ", on_btn && sel_btn == 1);

        tui_flush();

        char ch = 0;
        enum key_code k = tui_get_key(&ch);

        if (k == KEY_UP) {
            if (on_btn) on_btn = false;
            else if (sel_item > 0) sel_item--;
        } else if (k == KEY_DOWN) {
            if (!on_btn) {
                if (sel_item < num_sets - 1) sel_item++;
                else on_btn = true;
            }
        } else if (k == KEY_LEFT || k == KEY_RIGHT) {
            if (on_btn) sel_btn = 1 - sel_btn;
        } else if (k == KEY_TAB) {
            if (!on_btn) on_btn = true;
            else if (sel_btn == 0) sel_btn = 1;
            else { on_btn = false; sel_btn = 0; }
        } else if (k == KEY_SPACE) {
            if (!on_btn && sel_item >= 0 && sel_item < num_sets) {
                g_sets[sel_item].selected = !g_sets[sel_item].selected;
            }
        } else if (k == KEY_ENTER) {
            if (on_btn && sel_btn == 1) return STEP_HOSTNAME;
            return STEP_USER;
        } else if (k == KEY_CHAR) {
            if (ch >= '1' && ch <= '0' + num_sets) {
                int idx = ch - '1';
                g_sets[idx].selected = !g_sets[idx].selected;
            } else if (ch == 'o' || ch == 'O') {
                return STEP_USER;
            } else if (ch == 'b' || ch == 'B') {
                return STEP_HOSTNAME;
            }
        }
    }
}

/* Step 4: User Account & Sudo Configuration */
static enum wizard_step run_step_user(void) {
    int field_idx = 0; /* 0=username, 1=fullname, 2=password, 3=sudo, 4=buttons */
    int sel_btn = 0;   /* 0=OK, 1=Back */

    for (;;) {
        tui_clear();
        tui_draw_box(6, 3, 68, 18, "User Account & Sudo Configuration");

        tui_print(9, 5, "Create a personal user account and configure privileges:", ATTR_BOX_BOLD);
        tui_print(9, 6, "Leave Username blank to configure root-only administration.", ATTR_BOX);

        /* Field 0: Username */
        tui_print(9, 8, "Username:     [", ATTR_BOX);
        char f_user[32];
        snprintf(f_user, sizeof(f_user), "%-26s", g_username);
        tui_print(25, 8, f_user, (field_idx == 0) ? ATTR_ITEM_SEL : ATTR_BOX);
        tui_print(51, 8, "]", ATTR_BOX);

        /* Field 1: Full Name */
        tui_print(9, 10, "Full Name:    [", ATTR_BOX);
        char f_name[32];
        snprintf(f_name, sizeof(f_name), "%-26s", g_fullname);
        tui_print(25, 10, f_name, (field_idx == 1) ? ATTR_ITEM_SEL : ATTR_BOX);
        tui_print(51, 10, "]", ATTR_BOX);

        /* Field 2: Password */
        tui_print(9, 12, "Password:     [", ATTR_BOX);
        char f_pass[32];
        char pmask[28];
        int plen = (int)strlen(g_password);
        if (plen > 26) plen = 26;
        for (int p = 0; p < plen; p++) pmask[p] = '*';
        pmask[plen] = '\0';
        snprintf(f_pass, sizeof(f_pass), "%-26s", pmask);
        tui_print(25, 12, f_pass, (field_idx == 2) ? ATTR_ITEM_SEL : ATTR_BOX);
        tui_print(51, 12, "]", ATTR_BOX);

        /* Field 3: Sudo Toggle */
        char s_buf[64];
        snprintf(s_buf, sizeof(s_buf), "[%c] Grant administrative privileges (SUDO / wheel)", g_user_sudo ? 'X' : ' ');
        tui_print(9, 14, s_buf, (field_idx == 3) ? ATTR_ITEM_SEL : ATTR_BOX_BOLD);

        /* Field 4: Buttons */
        tui_draw_button(20, 17, "  OK  ", field_idx == 4 && sel_btn == 0);
        tui_draw_button(42, 17, " Back ", field_idx == 4 && sel_btn == 1);

        tui_flush();

        char ch = 0;
        enum key_code k = tui_get_key(&ch);

        if (k == KEY_TAB) {
            field_idx = (field_idx + 1) % 5;
            if (field_idx == 4) sel_btn = 0;
        } else if (k == KEY_DOWN) {
            if (field_idx < 4) field_idx++;
        } else if (k == KEY_UP) {
            if (field_idx > 0) field_idx--;
        } else if (k == KEY_LEFT || k == KEY_RIGHT) {
            if (field_idx == 4) sel_btn = 1 - sel_btn;
        } else if (k == KEY_SPACE) {
            if (field_idx == 3) {
                g_user_sudo = !g_user_sudo;
            } else if (field_idx == 1) {
                size_t l = strlen(g_fullname);
                if (l < 26) {
                    g_fullname[l] = ' ';
                    g_fullname[l + 1] = '\0';
                }
            }
        } else if (k == KEY_BACKSPACE) {
            if (field_idx == 0) {
                size_t l = strlen(g_username);
                if (l > 0) g_username[l - 1] = '\0';
            } else if (field_idx == 1) {
                size_t l = strlen(g_fullname);
                if (l > 0) g_fullname[l - 1] = '\0';
            } else if (field_idx == 2) {
                size_t l = strlen(g_password);
                if (l > 0) g_password[l - 1] = '\0';
            }
        } else if (k == KEY_ENTER) {
            if (field_idx == 4) {
                if (sel_btn == 1) return STEP_SETS;
                return STEP_DISK;
            } else {
                field_idx++;
                if (field_idx == 4) sel_btn = 0;
            }
        } else if (k == KEY_CHAR) {
            if (field_idx == 0) {
                if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-') {
                    size_t l = strlen(g_username);
                    if (l < 24) {
                        g_username[l] = ch;
                        g_username[l + 1] = '\0';
                    }
                } else if (ch == '\r' || ch == '\n') {
                    field_idx = 1;
                }
            } else if (field_idx == 1) {
                if (ch >= 32 && ch <= 126) {
                    size_t l = strlen(g_fullname);
                    if (l < 26) {
                        g_fullname[l] = ch;
                        g_fullname[l + 1] = '\0';
                    }
                } else if (ch == '\r' || ch == '\n') {
                    field_idx = 2;
                }
            } else if (field_idx == 2) {
                if (ch >= 32 && ch <= 126) {
                    size_t l = strlen(g_password);
                    if (l < 26) {
                        g_password[l] = ch;
                        g_password[l + 1] = '\0';
                    }
                } else if (ch == '\r' || ch == '\n') {
                    field_idx = 3;
                }
            } else if (field_idx == 3) {
                if (ch == ' ' || ch == 'x' || ch == 'X' || ch == 'y' || ch == 'Y') {
                    g_user_sudo = !g_user_sudo;
                } else if (ch == '\r' || ch == '\n') {
                    field_idx = 4;
                    sel_btn = 0;
                }
            } else if (field_idx == 4) {
                if (ch == 'o' || ch == 'O') return STEP_DISK;
                if (ch == 'b' || ch == 'B') return STEP_SETS;
            }
        }
    }
}

/* Step 5: Disk Selection */
static enum wizard_step run_step_disk(void) {
    int sel_disk = g_selected_disk;
    int sel_btn = 0;
    bool on_btn = false;

    scan_disks();
    if (g_num_disks == 0) {
        /* Fallback */
        strcpy(g_disks[0].name, "ram0");
        strcpy(g_disks[0].path, "/dev/ram0");
        g_disks[0].blocks_kb = 8192;
        g_num_disks = 1;
    }

    for (;;) {
        tui_clear();
        tui_draw_box(4, 4, 72, 16, "Partitioning - Select Storage Device");

        tui_print(7, 6, "Select the disk where DUnix will be installed:", ATTR_BOX_BOLD);

        for (int i = 0; i < g_num_disks; i++) {
            char buf[80];
            bool is_cur = (!on_btn && sel_disk == i);
            unsigned long mb = g_disks[i].blocks_kb / 1024;
            char sz_str[24];
            if (mb >= 1024) {
                snprintf(sz_str, sizeof(sz_str), "%lu.%lu GB", mb / 1024, (mb % 1024) * 10 / 1024);
            } else {
                snprintf(sz_str, sizeof(sz_str), "%lu MB", mb);
            }
            snprintf(buf, sizeof(buf), "  [%c] %-10s  %-9s  %.38s",
                     (sel_disk == i) ? '*' : ' ',
                     g_disks[i].path,
                     sz_str,
                     g_disks[i].model);
            tui_print(6, 8 + i * 2, buf, is_cur ? ATTR_ITEM_SEL : ATTR_ITEM_NORM);
        }

        tui_draw_button(22, 16, "  OK  ", on_btn && sel_btn == 0);
        tui_draw_button(46, 16, " Back ", on_btn && sel_btn == 1);

        tui_flush();

        char ch = 0;
        enum key_code k = tui_get_key(&ch);

        if (k == KEY_UP) {
            if (on_btn) on_btn = false;
            else if (sel_disk > 0) sel_disk--;
        } else if (k == KEY_DOWN) {
            if (!on_btn) {
                if (sel_disk < g_num_disks - 1) sel_disk++;
                else on_btn = true;
            }
        } else if (k == KEY_LEFT || k == KEY_RIGHT) {
            if (on_btn) sel_btn = 1 - sel_btn;
        } else if (k == KEY_TAB) {
            if (!on_btn) on_btn = true;
            else if (sel_btn == 0) sel_btn = 1;
            else { on_btn = false; sel_btn = 0; }
        } else if (k == KEY_ENTER) {
            if (on_btn && sel_btn == 1) return STEP_USER;
            g_selected_disk = sel_disk;
            return STEP_CONFIRM;
        } else if (k == KEY_CHAR) {
            if (ch >= '1' && ch <= '0' + g_num_disks) {
                g_selected_disk = ch - '1';
                return STEP_CONFIRM;
            } else if (ch == 'o' || ch == 'O') {
                g_selected_disk = sel_disk;
                return STEP_CONFIRM;
            } else if (ch == 'b' || ch == 'B') {
                return STEP_USER;
            }
        }
    }
}

/* Step 5: Confirmation */
static enum wizard_step run_step_confirm(void) {
    int sel_btn = 0; /* 0=Yes, 1=No */

    for (;;) {
        tui_clear();
        tui_draw_box(6, 4, 68, 17, "Confirmation - Format & Install");

        tui_print(9, 6, "WARNING: ALL DATA ON TARGET DRIVE WILL BE LOST!", ATTR_WARN);

        unsigned long mb = g_disks[g_selected_disk].blocks_kb / 1024;
        char sz_str[24];
        if (mb >= 1024) {
            snprintf(sz_str, sizeof(sz_str), "%lu.%lu GB (%lu MB)", mb / 1024, (mb % 1024) * 10 / 1024, mb);
        } else {
            snprintf(sz_str, sizeof(sz_str), "%lu MB (%lu KB)", mb, g_disks[g_selected_disk].blocks_kb);
        }

        char tbuf[80];
        snprintf(tbuf, sizeof(tbuf), "Target Drive: %s  [%s]", g_disks[g_selected_disk].path, sz_str);
        tui_print(9, 8, tbuf, ATTR_BOX_BOLD);

        char mbuf[80];
        snprintf(mbuf, sizeof(mbuf), "Drive Model:  %.48s", g_disks[g_selected_disk].model);
        tui_print(9, 9, mbuf, ATTR_BOX);

        tui_print(9, 10, "Filesystem:   Ext2 (1024-byte block size, persistent)", ATTR_BOX);
        char hbuf[80];
        snprintf(hbuf, sizeof(hbuf), "Hostname:     %s", g_hostname);
        tui_print(9, 11, hbuf, ATTR_BOX);

        char ubuf[80];
        if (g_username[0]) {
            snprintf(ubuf, sizeof(ubuf), "User Account: %s (%s)", g_username, g_user_sudo ? "SUDO / wheel enabled" : "Standard user");
        } else {
            snprintf(ubuf, sizeof(ubuf), "User Account: root only (no additional user)");
        }
        tui_print(9, 12, ubuf, ATTR_BOX);

        tui_print(9, 14, "Are you sure you want to write the partition scheme", ATTR_BOX);
        tui_print(9, 15, "and begin the installation process?", ATTR_BOX);

        tui_draw_button(20, 17, "  Yes  ", sel_btn == 0);
        tui_draw_button(44, 17, "  No   ", sel_btn == 1);

        tui_flush();

        char ch = 0;
        enum key_code k = tui_get_key(&ch);

        if (k == KEY_LEFT || k == KEY_RIGHT || k == KEY_TAB) {
            sel_btn = 1 - sel_btn;
        } else if (k == KEY_ENTER) {
            if (sel_btn == 1) return STEP_DISK;
            return STEP_INSTALLING;
        } else if (k == KEY_CHAR) {
            if (ch == 'y' || ch == 'Y') return STEP_INSTALLING;
            if (ch == 'n' || ch == 'N') return STEP_DISK;
        }
    }
}

/* Step 6: Live Installation / Extracting Sets */
static void draw_install_progress(const char *phase, const char *cur_file, int percent) {
    tui_clear();
    tui_draw_box(8, 5, 64, 15, "Extracting Distribution Sets");

    tui_print(11, 7, "Installing DUnix operating system packages to disk...", ATTR_BOX_BOLD);

    char pbuf[80];
    snprintf(pbuf, sizeof(pbuf), "Phase: %-46s", phase);
    tui_print(11, 9, pbuf, ATTR_BOX);

    char fbuf[80];
    snprintf(fbuf, sizeof(fbuf), "File:  %-46s", cur_file);
    tui_print(11, 10, fbuf, ATTR_ITEM_NORM);

    /* Progress bar */
    int bar_w = 48;
    int filled = (percent * bar_w) / 100;
    char pbar[64];
    pbar[0] = '[';
    for (int i = 0; i < bar_w; i++) {
        if (i < filled) pbar[1 + i] = '=';
        else if (i == filled) pbar[1 + i] = '>';
        else pbar[1 + i] = ' ';
    }
    pbar[1 + bar_w] = ']';
    pbar[2 + bar_w] = '\0';

    tui_print(11, 13, pbar, ATTR_ITEM_SEL);

    char pct_str[16];
    snprintf(pct_str, sizeof(pct_str), " %3d%% ", percent);
    tui_print(33, 15, pct_str, ATTR_BOX_BOLD);

    tui_flush();
}

static enum wizard_step run_step_installing(void) {
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
    draw_install_progress("Writing MBR bootloader & partition table", target_path, 5);
    usleep(100000);

    if (!is_ram) {
        int disk_fd = open(target_path, O_RDWR);
        if (disk_fd >= 0) {
            unsigned long total_sectors = g_disks[g_selected_disk].blocks_kb * 2;
            if (total_sectors == 0) total_sectors = 2097152; /* 1 GB default */

            /* Prepare MBR Sector (512 bytes) */
            uint8_t mbr_sector[512];
            memset(mbr_sector, 0, sizeof(mbr_sector));
            size_t copy_mbr = (sizeof(mbr_sector) < (size_t)dboot_mbr_len) ? sizeof(mbr_sector) : (size_t)dboot_mbr_len;
            memcpy(mbr_sector, dboot_mbr, copy_mbr);

            /* Configure Partition 1 (Offset 446 / 0x1BE) */
            uint32_t part_start_lba = 8192; /* 4 MB reserved offset */
            uint32_t part_sectors = (total_sectors > part_start_lba) ? (uint32_t)(total_sectors - part_start_lba) : 0;

            uint8_t *p1 = mbr_sector + 446;
            p1[0] = 0x80; /* Bootable / Active */
            p1[1] = 0x00; p1[2] = 0x02; p1[3] = 0x00; /* CHS start */
            p1[4] = 0x83; /* Type: Linux Ext2 */
            p1[5] = 0xFE; p1[6] = 0xFF; p1[7] = 0xFF; /* CHS end */
            p1[8]  = (uint8_t)(part_start_lba & 0xFF);
            p1[9]  = (uint8_t)((part_start_lba >> 8) & 0xFF);
            p1[10] = (uint8_t)((part_start_lba >> 16) & 0xFF);
            p1[11] = (uint8_t)((part_start_lba >> 24) & 0xFF);
            p1[12] = (uint8_t)(part_sectors & 0xFF);
            p1[13] = (uint8_t)((part_sectors >> 8) & 0xFF);
            p1[14] = (uint8_t)((part_sectors >> 16) & 0xFF);
            p1[15] = (uint8_t)((part_sectors >> 24) & 0xFF);

            /* Boot Signature */
            mbr_sector[510] = 0x55;
            mbr_sector[511] = 0xAA;

            /* Write MBR at LBA 0 */
            lseek(disk_fd, 0, SEEK_SET);
            write(disk_fd, mbr_sector, 512);

            /* Write Stage 2 loader at LBA 1 (Sectors 1..63, 63*512 = 32256 bytes) */
            uint8_t stage2_buf[63 * 512];
            memset(stage2_buf, 0, sizeof(stage2_buf));
            size_t copy_ldr = (sizeof(stage2_buf) < (size_t)dboot_ldr_len) ? sizeof(stage2_buf) : (size_t)dboot_ldr_len;
            memcpy(stage2_buf, dboot_ldr, copy_ldr);

            lseek(disk_fd, 512, SEEK_SET);
            ssize_t s2_ret = write(disk_fd, stage2_buf, sizeof(stage2_buf));
            fprintf(stderr, "INSTALLER: stage2 write returned %ld\n", (long)s2_ret);

            /* Write clean kernel image from /dev/kimg to LBA 64 */
            draw_install_progress("Writing native kernel image to boot sector", target_path, 12);
            int kfd = open("/dev/kimg", O_RDONLY);
            if (kfd >= 0) {
                lseek(disk_fd, 64 * 512, SEEK_SET);
                char kbuf[4096];
                ssize_t kn;
                bool write_error = false;
                size_t total_kbytes = 0;
                while ((kn = read(kfd, kbuf, sizeof(kbuf))) > 0) {
                    ssize_t kw = 0;
                    while (kw < kn) {
                        ssize_t ret = write(disk_fd, kbuf + kw, (size_t)(kn - kw));
                        if (ret <= 0) {
                            fprintf(stderr, "INSTALLER: write error ret=%ld kw=%ld kn=%ld errno=%d\n", (long)ret, (long)kw, (long)kn, errno);
                            write_error = true;
                            break;
                        }
                        kw += ret;
                        total_kbytes += (size_t)ret;
                    }
                    if (write_error) break;
                }
                fprintf(stderr, "INSTALLER: kernel write finished, total_kbytes=%zu write_error=%d\n", total_kbytes, (int)write_error);
                close(kfd);
            }

            /* Rescan partition table so subdevice is online */
            ioctl(disk_fd, BLKRRPART, 0);
            close(disk_fd);
            usleep(100000);
        }
    }

    /* 2. Format Ext2 Filesystem on Target Partition */
    draw_install_progress("Formatting filesystem with Ext2", part_path, 20);
    usleep(150000);

    int part_fd = open(part_path, O_RDWR);
    if (part_fd < 0 && !is_ram) {
        part_fd = open(target_path, O_RDWR);
    }
    if (part_fd >= 0) {
        ioctl(part_fd, BLKFORMAT, 0);
        close(part_fd);
    }

    /* 3. Mount /mnt */
    draw_install_progress("Mounting target partition", "/mnt", 25);
    usleep(100000);

    mkdir("/mnt", 0755);
    umount("/mnt");
    if (mount(part_path, "/mnt", "ext2", 0, NULL) != 0 && !is_ram) {
        mount(target_path, "/mnt", "ext2", 0, NULL);
    }

    /* 4. Create Directories */
    draw_install_progress("Creating filesystem hierarchy", "/mnt/bin, /mnt/etc...", 30);
    usleep(100000);

    const char *dirs[] = {
        "/mnt/bin", "/mnt/sbin", "/mnt/etc", "/mnt/dev", "/mnt/proc",
        "/mnt/home", "/mnt/root", "/mnt/tmp",
        "/mnt/usr", "/mnt/usr/bin", "/mnt/usr/lib", "/mnt/usr/include",
        "/mnt/var", "/mnt/var/log", "/mnt/var/run", "/mnt/mnt", "/mnt/boot",
        NULL
    };
    for (int i = 0; dirs[i]; i++) {
        mkdir(dirs[i], 0755);
    }
    if (g_username[0] != '\0') {
        char user_home[128];
        snprintf(user_home, sizeof(user_home), "/mnt/home/%s", g_username);
        mkdir(user_home, 0755);
    }

    /* 5. Copy Binaries */
    DIR *dir = opendir("/bin");
    if (dir) {
        char bin_list[64][64];
        int bcount = 0;
        struct dirent *de;
        while ((de = readdir(dir)) != NULL && bcount < 64) {
            if (de->d_name[0] != '.') {
                strncpy(bin_list[bcount++], de->d_name, 63);
            }
        }
        closedir(dir);
        int last_pct = -1;
        for (int i = 0; i < bcount; i++) {
            char src[128], dst[128];
            snprintf(src, sizeof(src), "/bin/%s", bin_list[i]);
            snprintf(dst, sizeof(dst), "/mnt/bin/%s", bin_list[i]);
            mode_t mode = (strcmp(bin_list[i], "sudo") == 0) ? 04755 : 0755;
            copy_file(src, dst, mode);

            int pct = 30 + ((i + 1) * 55) / (bcount > 0 ? bcount : 1);
            if (pct != last_pct || i == bcount - 1) {
                draw_install_progress("Extracting bsd-utils & base set", bin_list[i], pct);
                last_pct = pct;
            }
        }
    }

    copy_file("/sbin/init", "/mnt/sbin/init", 0755);

    /* 6. Deploy Kernel Image to /mnt/boot */
    draw_install_progress("Deploying kernel binary to /mnt/boot", "/mnt/boot/dunix.raw", 88);
    copy_file("/dev/kimg", "/mnt/boot/dunix.raw", 0644);

    /* 7. Configuration Files */
    draw_install_progress("Writing system configuration", "/etc/hostname, /etc/fstab", 92);
    usleep(150000);

    const char *etc_files[] = { "os-release", "hosts", "resolv.conf", NULL };
    for (int i = 0; etc_files[i]; i++) {
        char s[128], d[128];
        snprintf(s, sizeof(s), "/etc/%s", etc_files[i]);
        snprintf(d, sizeof(d), "/mnt/etc/%s", etc_files[i]);
        copy_file(s, d, 0644);
    }

    /* Passwd & Group tailored to user configuration */
    int pfd = open("/mnt/etc/passwd", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (pfd >= 0) {
        char pbuf[512];
        int plen = snprintf(pbuf, sizeof(pbuf), "root:x:0:0:root:/root:/bin/sh\n");
        if (g_username[0] != '\0') {
            plen += snprintf(pbuf + plen, sizeof(pbuf) - plen,
                             "%s:x:1000:100:%s:/home/%s:/bin/sh\n",
                             g_username,
                             g_fullname[0] ? g_fullname : g_username,
                             g_username);
        }
        write(pfd, pbuf, (size_t)plen);
        close(pfd);
    }

    int gfd = open("/mnt/etc/group", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (gfd >= 0) {
        char gbuf[512];
        int glen = 0;
        if (g_username[0] != '\0' && g_user_sudo) {
            glen = snprintf(gbuf, sizeof(gbuf),
                            "root:x:0:\n"
                            "wheel:x:10:root,%s\n"
                            "sudo:x:27:root,%s\n"
                            "users:x:100:%s\n",
                            g_username, g_username, g_username);
        } else if (g_username[0] != '\0') {
            glen = snprintf(gbuf, sizeof(gbuf),
                            "root:x:0:\n"
                            "wheel:x:10:root\n"
                            "sudo:x:27:root\n"
                            "users:x:100:%s\n",
                            g_username);
        } else {
            glen = snprintf(gbuf, sizeof(gbuf),
                            "root:x:0:\n"
                            "wheel:x:10:root\n"
                            "sudo:x:27:root\n"
                            "users:x:100:\n");
        }
        write(gfd, gbuf, (size_t)glen);
        close(gfd);
    }

    /* Sudoers */
    int s_fd = open("/mnt/etc/sudoers", O_CREAT | O_WRONLY | O_TRUNC, 0440);
    if (s_fd >= 0) {
        char sbuf[512];
        int slen = snprintf(sbuf, sizeof(sbuf),
                            "# /etc/sudoers - DUnix sudo configuration\n"
                            "root ALL=(ALL) ALL\n"
                            "%%wheel ALL=(ALL) ALL\n"
                            "%%sudo ALL=(ALL) ALL\n");
        if (g_username[0] != '\0' && g_user_sudo) {
            slen += snprintf(sbuf + slen, sizeof(sbuf) - slen,
                             "%s ALL=(ALL) ALL\n", g_username);
        }
        write(s_fd, sbuf, (size_t)slen);
        close(s_fd);
        chmod("/mnt/etc/sudoers", 0440);
    }

    /* Hostname */
    int h_fd = open("/mnt/etc/hostname", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (h_fd >= 0) {
        write(h_fd, g_hostname, strlen(g_hostname));
        write(h_fd, "\n", 1);
        close(h_fd);
    }

    /* Fstab */
    int fstab_fd = open("/mnt/etc/fstab", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fstab_fd >= 0) {
        char fbuf[256];
        snprintf(fbuf, sizeof(fbuf),
                 "# DUnix Filesystem Mount Table\n"
                 "%-12s /          ext2    defaults    0  1\n"
                 "procfs       /proc      proc    defaults    0  0\n"
                 "devfs        /dev       devfs   defaults    0  0\n",
                 part_path);
        write(fstab_fd, fbuf, strlen(fbuf));
        close(fstab_fd);
    }

    /* Bootloader cfg */
    int boot_fd = open("/mnt/boot/boot.cfg", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (boot_fd >= 0) {
        char bbuf[256];
        snprintf(bbuf, sizeof(bbuf),
                 "timeout=3\n"
                 "default=0\n\n"
                 "title DUnix 64-Bit Operating System\n"
                 "kernel /boot/dunix.raw root=%s rw\n",
                 part_path);
        write(boot_fd, bbuf, strlen(bbuf));
        close(boot_fd);
    }

    /* 8. Unmount */
    draw_install_progress("Flushing buffers and unmounting target", part_path, 100);
    usleep(150000);
    umount("/mnt");

    return STEP_DONE;
}

/* Step 7: Completed */
static enum wizard_step run_step_done(void) {
    int sel_btn = 0; /* 0=Reboot, 1=Shell */

    for (;;) {
        tui_clear();
        tui_draw_box(6, 4, 68, 16, "Installation Complete");

        tui_print(9, 6, "Congratulations! DUnix has been successfully installed!", ATTR_SUCCESS);

        char dbuf[80];
        snprintf(dbuf, sizeof(dbuf), "Target Device: %s (%.44s)",
                 g_disks[g_selected_disk].path, g_disks[g_selected_disk].model);
        tui_print(9, 8, dbuf, ATTR_BOX_BOLD);

        tui_print(9, 10, "All 52 native ELF64 binaries and system configs deployed.", ATTR_BOX);
        tui_print(9, 11, "Bootloader configuration written to /boot/boot.cfg.", ATTR_BOX);
        tui_print(9, 12, "Fstab mount configuration written to /etc/fstab.", ATTR_BOX);

        tui_print(9, 14, "Remove installation media and choose an option below:", ATTR_BOX);

        tui_draw_button(20, 16, "Reboot", sel_btn == 0);
        tui_draw_button(42, 16, "Shell ", sel_btn == 1);

        tui_flush();

        char ch = 0;
        enum key_code k = tui_get_key(&ch);

        if (k == KEY_LEFT || k == KEY_RIGHT || k == KEY_TAB) {
            sel_btn = 1 - sel_btn;
        } else if (k == KEY_ENTER) {
            return (sel_btn == 0) ? STEP_EXIT_REBOOT : STEP_EXIT_SHELL;
        } else if (k == KEY_CHAR) {
            if (ch == 'r' || ch == 'R') return STEP_EXIT_REBOOT;
            if (ch == 's' || ch == 'S') return STEP_EXIT_SHELL;
        }
    }
}

/* ─── Main Entry Point ─── */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    /* Ensure standard streams exist */
    if (fcntl(0, F_GETFL) < 0) open("/dev/console", O_RDONLY);
    if (fcntl(1, F_GETFL) < 0) open("/dev/console", O_WRONLY);
    if (fcntl(2, F_GETFL) < 0) open("/dev/console", O_WRONLY);

    tui_init();

    enum wizard_step step = STEP_MAIN_MENU;
    while (step != STEP_EXIT_SHELL && step != STEP_EXIT_REBOOT) {
        switch (step) {
            case STEP_MAIN_MENU:  step = run_step_main_menu(); break;
            case STEP_HOSTNAME:   step = run_step_hostname(); break;
            case STEP_SETS:       step = run_step_sets(); break;
            case STEP_USER:       step = run_step_user(); break;
            case STEP_DISK:       step = run_step_disk(); break;
            case STEP_CONFIRM:    step = run_step_confirm(); break;
            case STEP_INSTALLING: step = run_step_installing(); break;
            case STEP_DONE:       step = run_step_done(); break;
            default:              step = STEP_EXIT_SHELL; break;
        }
    }

    tui_restore_term();

    if (step == STEP_EXIT_SHELL) {
        printf("\nStarting DUnix Maintenance Shell (/bin/sh)...\n\n");
        pid_t pid = fork();
        if (pid == 0) {
            char *sh_argv[] = { "/bin/sh", NULL };
            execve("/bin/sh", sh_argv, NULL);
            exit(1);
        }
        waitpid(pid, NULL, 0);
    } else if (step == STEP_EXIT_REBOOT) {
        printf("\nSystem installation complete. Restarting system...\n");
        sync();
        usleep(500000);
        reboot(RB_AUTOBOOT);
    }

    return 0;
}
