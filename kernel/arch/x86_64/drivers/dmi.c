#include <arch/x86_64/drivers/dmi.h>
#include <arch/x86_64/mm/paging.h>
#include <dunix/string.h>
#include <dunix/kprintf.h>

struct smbios_header {
    uint8_t  type;
    uint8_t  length;
    uint16_t handle;
} __attribute__((packed));

struct smbios_entry {
    char     anchor[4];         /* "_SM_" */
    uint8_t  checksum;
    uint8_t  length;
    uint8_t  major;
    uint8_t  minor;
    uint16_t max_size;
    uint8_t  entry_rev;
    uint8_t  formatted[5];
    char     dmi_anchor[5];     /* "_DMI_" */
    uint8_t  dmi_checksum;
    uint16_t table_length;
    uint32_t table_address;
    uint16_t num_structures;
    uint8_t  bcd_revision;
} __attribute__((packed));

struct smbios3_entry {
    char     anchor[5];         /* "_SM3_" */
    uint8_t  checksum;
    uint8_t  length;
    uint8_t  major;
    uint8_t  minor;
    uint8_t  docrev;
    uint8_t  entry_rev;
    uint8_t  reserved;
    uint32_t table_max_size;
    uint64_t table_address;
} __attribute__((packed));

static struct dmi_system_info g_dmi;
static bool g_dmi_found = false;

static const char *get_string(const struct smbios_header *hdr, uint8_t idx) {
    if (idx == 0) return "";
    const char *p = (const char *)hdr + hdr->length;
    uint8_t cur = 1;
    while (*p) {
        if (cur == idx) return p;
        p += strlen(p) + 1;
        cur++;
    }
    return "";
}

static void copy_field(char *dst, size_t dst_len, const char *src) {
    if (!src || !*src) return;
    /* Trim leading and trailing spaces */
    while (*src == ' ' || *src == '\t') src++;
    strncpy(dst, src, dst_len - 1);
    dst[dst_len - 1] = '\0';
    size_t l = strlen(dst);
    while (l > 0 && (dst[l - 1] == ' ' || dst[l - 1] == '\t' || dst[l - 1] == '\r' || dst[l - 1] == '\n')) {
        dst[--l] = '\0';
    }
}

static void parse_smbios_table(uint64_t table_addr, size_t table_len) {
    const uint8_t *cur = (const uint8_t *)PHYS_TO_VIRT(table_addr);
    const uint8_t *end = cur + table_len;

    while (cur + sizeof(struct smbios_header) <= end) {
        const struct smbios_header *hdr = (const struct smbios_header *)cur;
        if (hdr->type == 127) { /* End-of-table */
            break;
        }
        if (hdr->length < sizeof(struct smbios_header)) {
            break;
        }

        if (hdr->type == 0 && hdr->length >= 9) {
            /* Type 0: BIOS Information */
            const uint8_t *data = cur;
            copy_field(g_dmi.bios_vendor, sizeof(g_dmi.bios_vendor), get_string(hdr, data[4]));
            copy_field(g_dmi.bios_version, sizeof(g_dmi.bios_version), get_string(hdr, data[5]));
            copy_field(g_dmi.bios_date, sizeof(g_dmi.bios_date), get_string(hdr, data[8]));
        } else if (hdr->type == 1 && hdr->length >= 8) {
            /* Type 1: System Information */
            const uint8_t *data = cur;
            copy_field(g_dmi.sys_vendor, sizeof(g_dmi.sys_vendor), get_string(hdr, data[4]));
            copy_field(g_dmi.product_name, sizeof(g_dmi.product_name), get_string(hdr, data[5]));
            copy_field(g_dmi.product_version, sizeof(g_dmi.product_version), get_string(hdr, data[6]));
            copy_field(g_dmi.product_serial, sizeof(g_dmi.product_serial), get_string(hdr, data[7]));
        } else if (hdr->type == 2 && hdr->length >= 6) {
            /* Type 2: Baseboard / Motherboard */
            const uint8_t *data = cur;
            copy_field(g_dmi.board_vendor, sizeof(g_dmi.board_vendor), get_string(hdr, data[4]));
            copy_field(g_dmi.board_name, sizeof(g_dmi.board_name), get_string(hdr, data[5]));
        } else if (hdr->type == 3 && hdr->length >= 6) {
            /* Type 3: Enclosure / Chassis */
            const uint8_t *data = cur;
            uint8_t ctype = data[5] & 0x7F;
            const char *cname = "Desktop";
            if (ctype == 8 || ctype == 9 || ctype == 10 || ctype == 14) cname = "Laptop";
            else if (ctype == 17 || ctype == 23) cname = "Server";
            else if (ctype == 30 || ctype == 31) cname = "Tablet";
            copy_field(g_dmi.chassis_type, sizeof(g_dmi.chassis_type), cname);
        }

        /* Skip past formatted area and string table (which ends with double null) */
        cur += hdr->length;
        while (cur + 1 < end && (cur[0] != 0 || cur[1] != 0)) {
            cur++;
        }
        cur += 2; /* Skip the \0\0 */
    }
}

void dmi_init(void) {
    memset(&g_dmi, 0, sizeof(g_dmi));

    /* Scan BIOS memory area 0xF0000 - 0xFFFFF */
    const uint8_t *bios_mem = (const uint8_t *)PHYS_TO_VIRT(0xF0000ULL);

    /* Search for SMBIOS 3.x (_SM3_) */
    for (size_t offset = 0; offset <= 0x10000 - sizeof(struct smbios3_entry); offset += 16) {
        if (memcmp(bios_mem + offset, "_SM3_", 5) == 0) {
            const struct smbios3_entry *entry = (const struct smbios3_entry *)(bios_mem + offset);
            uint8_t sum = 0;
            for (uint8_t i = 0; i < entry->length; i++) {
                sum += ((const uint8_t *)entry)[i];
            }
            if (sum == 0 && entry->table_address != 0) {
                parse_smbios_table(entry->table_address, entry->table_max_size);
                g_dmi_found = true;
                break;
            }
        }
    }

    /* Search for SMBIOS 2.x (_SM_) if 3.x wasn't found */
    if (!g_dmi_found) {
        for (size_t offset = 0; offset <= 0x10000 - sizeof(struct smbios_entry); offset += 16) {
            if (memcmp(bios_mem + offset, "_SM_", 4) == 0) {
                const struct smbios_entry *entry = (const struct smbios_entry *)(bios_mem + offset);
                uint8_t sum = 0;
                for (uint8_t i = 0; i < entry->length; i++) {
                    sum += ((const uint8_t *)entry)[i];
                }
                if (sum == 0 && memcmp(entry->dmi_anchor, "_DMI_", 5) == 0 && entry->table_address != 0) {
                    parse_smbios_table(entry->table_address, entry->table_length);
                    g_dmi_found = true;
                    break;
                }
            }
        }
    }

    if (g_dmi_found) {
        klog(KLOG_INFO, "DMI / SMBIOS: Hardware identified as '%s %s' (%s)\n",
             g_dmi.sys_vendor[0] ? g_dmi.sys_vendor : "Generic",
             g_dmi.product_name[0] ? g_dmi.product_name : "PC",
             g_dmi.product_version[0] ? g_dmi.product_version : "v1.0");
    } else {
        /* Fallback generic */
        strncpy(g_dmi.sys_vendor, "PC Compatible", sizeof(g_dmi.sys_vendor) - 1);
        strncpy(g_dmi.product_name, "x86_64 Machine", sizeof(g_dmi.product_name) - 1);
        klog(KLOG_INFO, "DMI / SMBIOS: Table not found in 0xF0000; using generic hardware profile\n");
    }
}

const struct dmi_system_info *dmi_get_info(void) {
    return &g_dmi;
}
