#ifndef _ARCH_X86_64_DRIVERS_AHCI_H
#define _ARCH_X86_64_DRIVERS_AHCI_H

#include <dunix/types.h>
#include <dunix/stdbool.h>
#include <fs/block.h>

#define AHCI_MAX_DRIVES      8
#define AHCI_MMIO_BASE       0xFFFFFFFFD0000000ULL

/* HBA GHC bit definitions */
#define AHCI_GHC_HR          (1U << 0)   /* HBA Reset */
#define AHCI_GHC_IE          (1U << 1)   /* Interrupt Enable */
#define AHCI_GHC_AE          (1U << 31)  /* AHCI Enable */

/* Port CMD bit definitions */
#define AHCI_PORT_CMD_ST     (1U << 0)   /* Start */
#define AHCI_PORT_CMD_SUD    (1U << 1)   /* Spin-Up Device */
#define AHCI_PORT_CMD_POD    (1U << 2)   /* Power On Device */
#define AHCI_PORT_CMD_FRE    (1U << 4)   /* FIS Receive Enable */
#define AHCI_PORT_CMD_FR     (1U << 14)  /* FIS Receive Running */
#define AHCI_PORT_CMD_CR     (1U << 15)  /* Command List Running */

/* Port Signatures */
#define AHCI_DEV_SATA        0x00000101  /* SATA drive */
#define AHCI_DEV_SATAPI      0xEB140101  /* SATAPI drive */
#define AHCI_DEV_SEMB        0xC33C0101  /* Enclosure management */
#define AHCI_DEV_PM          0x96690101  /* Port multiplier */

/* ATA Commands */
#define ATA_CMD_READ_DMA_EXT  0x25
#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_IDENTIFY      0xEC
#define ATA_CMD_FLUSH_EXT     0xEA

/* Port memory registers */
struct ahci_port {
    uint32_t clb;       /* 0x00: Command List Base Address (Low) */
    uint32_t clbu;      /* 0x04: Command List Base Address (High) */
    uint32_t fb;        /* 0x08: FIS Base Address (Low) */
    uint32_t fbu;       /* 0x0C: FIS Base Address (High) */
    uint32_t is;        /* 0x10: Interrupt Status */
    uint32_t ie;        /* 0x14: Interrupt Enable */
    uint32_t cmd;       /* 0x18: Command and Status */
    uint32_t rsv0;      /* 0x1C: Reserved */
    uint32_t tfd;       /* 0x20: Task File Data */
    uint32_t sig;       /* 0x24: Signature */
    uint32_t ssts;      /* 0x28: Serial ATA Status (SCR0: SStatus) */
    uint32_t sctl;      /* 0x2C: Serial ATA Control (SCR2: SControl) */
    uint32_t serr;      /* 0x30: Serial ATA Error (SCR1: SError) */
    uint32_t sact;      /* 0x34: Serial ATA Active (SCR3: SActive) */
    uint32_t ci;        /* 0x38: Command Issue */
    uint32_t sntf;      /* 0x3C: Serial ATA Notification */
    uint32_t fbs;       /* 0x40: FIS-based Switching Control */
    uint32_t devslp;    /* 0x44: Device Sleep */
    uint8_t  rsv1[0x70 - 0x48];
    uint8_t  vendor[0x80 - 0x70];
} __attribute__((packed));

/* Generic Host Control (HBA Memory) */
struct ahci_hba_mem {
    uint32_t cap;       /* 0x00: Host Capabilities */
    uint32_t ghc;       /* 0x04: Global Host Control */
    uint32_t is;        /* 0x08: Interrupt Status */
    uint32_t pi;        /* 0x0C: Ports Implemented */
    uint32_t vs;        /* 0x10: AHCI Version */
    uint32_t ccc_ctl;   /* 0x14: Command Coalescing Control */
    uint32_t ccc_pts;   /* 0x18: Command Coalescing Ports */
    uint32_t em_loc;    /* 0x1C: Enclosure Management Location */
    uint32_t em_ctl;    /* 0x20: Enclosure Management Control */
    uint32_t cap2;      /* 0x24: Host Capabilities Extended */
    uint32_t bohc;      /* 0x28: BIOS/OS Handoff Control and Status */
    uint8_t  rsv[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
    struct ahci_port ports[32]; /* 0x100 - 0x10FF */
} __attribute__((packed));

/* AHCI Command Header (32 bytes) */
struct ahci_cmd_header {
    uint8_t  cfl:5;     /* Command FIS length in DWORDS */
    uint8_t  a:1;       /* ATAPI */
    uint8_t  w:1;       /* Write (1 = Write to device, 0 = Read from device) */
    uint8_t  p:1;       /* Prefetchable */
    uint8_t  r:1;       /* Reset */
    uint8_t  b:1;       /* BIST */
    uint8_t  c:1;       /* Clear Busy upon R_OK */
    uint8_t  rsv0:1;
    uint8_t  pmp:4;     /* Port multiplier port */
    uint16_t prdtl;     /* Number of PRDT entries */
    uint32_t prdbc;     /* PRD Byte Count transferred */
    uint32_t ctba;      /* Command Table Descriptor Base Address Low */
    uint32_t ctbau;     /* Command Table Descriptor Base Address High */
    uint32_t rsv1[4];
} __attribute__((packed));

/* AHCI PRDT Entry (16 bytes) */
struct ahci_prdt_entry {
    uint32_t dba;       /* Data Base Address Low */
    uint32_t dbau;      /* Data Base Address High */
    uint32_t rsv0;
    uint32_t dbc;       /* Byte count - 1 (bits 0..21), bit 31 = Interrupt on completion */
} __attribute__((packed));

/* AHCI Command Table */
struct ahci_cmd_table {
    uint8_t  cfis[64];
    uint8_t  acmd[16];
    uint8_t  rsv[48];
    struct ahci_prdt_entry prdt_entries[1];
} __attribute__((packed));

/* FIS Register - Host to Device (Type 0x27, 20 bytes) */
struct fis_reg_h2d {
    uint8_t  fis_type;  /* 0x27 */
    uint8_t  pm_c;      /* Port multiplier (bits 3:0), C bit (bit 7: 1 = Command) */
    uint8_t  command;   /* Command register */
    uint8_t  feature_low; /* Feature register 7:0 */
    uint8_t  lba0;      /* LBA 7:0 */
    uint8_t  lba1;      /* LBA 15:8 */
    uint8_t  lba2;      /* LBA 23:16 */
    uint8_t  device;    /* Device register (bit 6 = 1 for LBA) */
    uint8_t  lba3;      /* LBA 31:24 */
    uint8_t  lba4;      /* LBA 39:32 */
    uint8_t  lba5;      /* LBA 47:40 */
    uint8_t  feature_high; /* Feature register 15:8 */
    uint8_t  count_low; /* Sector count 7:0 */
    uint8_t  count_high;/* Sector count 15:8 */
    uint8_t  icc;       /* Isochronous command completion */
    uint8_t  control;   /* Control register */
    uint8_t  rsv[4];
} __attribute__((packed));

struct ahci_drive {
    bool                present;
    uint8_t             port_no;
    volatile struct ahci_port *port;
    char                model[48];
    char                serial[24];
    uint64_t            sectors;
    uint32_t            sector_size;
    paddr_t             dma_page_phys;   /* Command list + FIS + Cmd Table */
    paddr_t             bounce_phys;     /* 4096-byte bounce buffer */
    struct block_device bdev;
};

void ahci_init(void);
int  ahci_read_sectors(struct ahci_drive *drive, uint64_t lba, size_t count, void *buf);
int  ahci_write_sectors(struct ahci_drive *drive, uint64_t lba, size_t count, const void *buf);

#endif /* _ARCH_X86_64_DRIVERS_AHCI_H */
