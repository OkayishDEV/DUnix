#include <fs/initrd_bins.h>
#include <fs/vfs.h>
#include <fs/ramfs.h>
#include <dunix/kprintf.h>

#define INSTALL_BIN(dir, name) \
    if (bin_##name##_data_len > 0) ramfs_create_file(dir, #name, bin_##name##_data, bin_##name##_data_len, 0755);

void vfs_populate_binaries(void) {
    struct vfs_node *bin_dir = vfs_lookup("/bin");
    struct vfs_node *sbin_dir = vfs_lookup("/sbin");

    if (sbin_dir) {
        INSTALL_BIN(sbin_dir, init)
        INSTALL_BIN(sbin_dir, reboot)
        INSTALL_BIN(sbin_dir, poweroff)
    }

    if (bin_dir) {
        /* Core Unix Suite (Milestones 10, 16) */
        INSTALL_BIN(bin_dir, sh)
        INSTALL_BIN(bin_dir, cat)
        INSTALL_BIN(bin_dir, ls)
        INSTALL_BIN(bin_dir, echo)
        INSTALL_BIN(bin_dir, mkdir)
        INSTALL_BIN(bin_dir, rm)
        INSTALL_BIN(bin_dir, pwd)
        INSTALL_BIN(bin_dir, cp)
        INSTALL_BIN(bin_dir, ps)
        INSTALL_BIN(bin_dir, grep)
        INSTALL_BIN(bin_dir, head)
        INSTALL_BIN(bin_dir, tail)
        INSTALL_BIN(bin_dir, wc)
        INSTALL_BIN(bin_dir, find)
        INSTALL_BIN(bin_dir, touch)
        INSTALL_BIN(bin_dir, chmod)
        INSTALL_BIN(bin_dir, chown)
        INSTALL_BIN(bin_dir, kill)
        INSTALL_BIN(bin_dir, sleep)
        INSTALL_BIN(bin_dir, env)
        INSTALL_BIN(bin_dir, uname)
        INSTALL_BIN(bin_dir, dmesg)
        INSTALL_BIN(bin_dir, df)
        INSTALL_BIN(bin_dir, mount)
        INSTALL_BIN(bin_dir, umount)
        INSTALL_BIN(bin_dir, true)
        INSTALL_BIN(bin_dir, false)
        INSTALL_BIN(bin_dir, login)
        INSTALL_BIN(bin_dir, vedit)

        /* Extended POSIX Core Utilities & Timekeeping */
        INSTALL_BIN(bin_dir, date)
        INSTALL_BIN(bin_dir, reboot)
        INSTALL_BIN(bin_dir, poweroff)
        INSTALL_BIN(bin_dir, clear)
        INSTALL_BIN(bin_dir, tee)
        INSTALL_BIN(bin_dir, sort)
        INSTALL_BIN(bin_dir, uniq)
        INSTALL_BIN(bin_dir, tr)
        INSTALL_BIN(bin_dir, seq)
        INSTALL_BIN(bin_dir, yes)
        INSTALL_BIN(bin_dir, more)
        INSTALL_BIN(bin_dir, beep)
        INSTALL_BIN(bin_dir, du)
        INSTALL_BIN(bin_dir, strings)
        INSTALL_BIN(bin_dir, hexdump)
        INSTALL_BIN(bin_dir, diff)
        INSTALL_BIN(bin_dir, cmp)
        INSTALL_BIN(bin_dir, uptime)
        INSTALL_BIN(bin_dir, sed)
        if (bin_sudo_data_len > 0) {
            struct vfs_node *sn = ramfs_create_file(bin_dir, "sudo", bin_sudo_data, bin_sudo_data_len, 04755);
            if (sn) {
                sn->uid = 0;
                sn->gid = 0;
                sn->mask = 04755;
            }
        }
        INSTALL_BIN(bin_dir, whoami)
        INSTALL_BIN(bin_dir, id)

        /* Networking Suite (Milestone 17) */
        INSTALL_BIN(bin_dir, ifconfig)
        INSTALL_BIN(bin_dir, ping)
        INSTALL_BIN(bin_dir, nc)
        INSTALL_BIN(bin_dir, wget)
        INSTALL_BIN(bin_dir, httpd)

        /* Development & Tooling Suite (Milestone 18) */
        INSTALL_BIN(bin_dir, cc)
        INSTALL_BIN(bin_dir, cal)
        INSTALL_BIN(bin_dir, bc)
        INSTALL_BIN(bin_dir, xargs)
        INSTALL_BIN(bin_dir, which)
        INSTALL_BIN(bin_dir, basename)
        INSTALL_BIN(bin_dir, dirname)
        INSTALL_BIN(bin_dir, neofetch)

        /* DUnix Native Desktop (Milestone 21 - DWS Windowing System) */
        INSTALL_BIN(bin_dir, dws)
        INSTALL_BIN(bin_dir, dterm)
        INSTALL_BIN(bin_dir, dweb)
        INSTALL_BIN(bin_dir, browser)
        INSTALL_BIN(bin_dir, drm_info)
        INSTALL_BIN(bin_dir, glgears)
        INSTALL_BIN(bin_dir, dfiles)
        INSTALL_BIN(bin_dir, dclock)
        INSTALL_BIN(bin_dir, dcalc)
        INSTALL_BIN(bin_dir, dsession)

        /* System Installation & Storage Tools */
        INSTALL_BIN(bin_dir, mkfs)
        INSTALL_BIN(bin_dir, installer)
        INSTALL_BIN(bin_dir, dinstall)

        /* Linux Compatibility Layer Tools & Binaries */
        INSTALL_BIN(bin_dir, linux)
        INSTALL_BIN(bin_dir, linux_hello)
        INSTALL_BIN(bin_dir, linux_uname)
        INSTALL_BIN(bin_dir, links)

        /* Dual TUI/GUI Games */
        INSTALL_BIN(bin_dir, snake)
        INSTALL_BIN(bin_dir, minesweeper)
        if (bin_minesweeper_data_len > 0) {
            ramfs_create_file(bin_dir, "mines", bin_minesweeper_data, bin_minesweeper_data_len, 0755);
        }
        INSTALL_BIN(bin_dir, 2048)
    }

    struct vfs_node *compat_bin = vfs_lookup("/compat/linux/bin");
    if (compat_bin) {
        INSTALL_BIN(compat_bin, linux_hello)
        INSTALL_BIN(compat_bin, linux_uname)
        INSTALL_BIN(compat_bin, links)
    }

    klog(KLOG_INFO, "Populated full Unix suite in /bin (74 binaries + DUnix Native Desktop + Linux Compat Layer)\n");
}
