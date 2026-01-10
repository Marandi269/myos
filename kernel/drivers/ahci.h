/*
 * ahci.h - AHCI (SATA) Driver
 *
 * Advanced Host Controller Interface for SATA devices
 */

#ifndef _AHCI_H
#define _AHCI_H

#include "types.h"

/* PCI Class/Subclass for AHCI */
#define AHCI_CLASS          0x01    /* Mass Storage */
#define AHCI_SUBCLASS       0x06    /* SATA */
#define AHCI_PROG_IF        0x01    /* AHCI 1.0 */

/* AHCI HBA Memory Registers */
typedef volatile struct {
    uint32_t cap;           /* 0x00: Host Capabilities */
    uint32_t ghc;           /* 0x04: Global Host Control */
    uint32_t is;            /* 0x08: Interrupt Status */
    uint32_t pi;            /* 0x0C: Ports Implemented */
    uint32_t vs;            /* 0x10: Version */
    uint32_t ccc_ctl;       /* 0x14: Command Completion Coalescing Control */
    uint32_t ccc_ports;     /* 0x18: Command Completion Coalescing Ports */
    uint32_t em_loc;        /* 0x1C: Enclosure Management Location */
    uint32_t em_ctl;        /* 0x20: Enclosure Management Control */
    uint32_t cap2;          /* 0x24: Host Capabilities Extended */
    uint32_t bohc;          /* 0x28: BIOS/OS Handoff Control and Status */
    uint8_t  reserved[0xA0 - 0x2C];
    uint8_t  vendor[0x100 - 0xA0];
} __attribute__((packed)) ahci_hba_mem_t;

/* GHC bits */
#define AHCI_GHC_HR         (1 << 0)    /* HBA Reset */
#define AHCI_GHC_IE         (1 << 1)    /* Interrupt Enable */
#define AHCI_GHC_MRSM       (1 << 2)    /* MSI Revert to Single Message */
#define AHCI_GHC_AE         (1 << 31)   /* AHCI Enable */

/* CAP bits */
#define AHCI_CAP_S64A       (1 << 31)   /* 64-bit Addressing */
#define AHCI_CAP_SNCQ       (1 << 30)   /* Native Command Queuing */
#define AHCI_CAP_SSNTF      (1 << 29)   /* SNotification */
#define AHCI_CAP_SMPS       (1 << 28)   /* Mechanical Presence Switch */
#define AHCI_CAP_SSS        (1 << 27)   /* Staggered Spin-up */
#define AHCI_CAP_SALP       (1 << 26)   /* Aggressive Link Power Management */
#define AHCI_CAP_SAL        (1 << 25)   /* Activity LED */
#define AHCI_CAP_SCLO       (1 << 24)   /* Command List Override */
#define AHCI_CAP_NCS(cap)   (((cap) >> 8) & 0x1F)   /* Number of Command Slots */
#define AHCI_CAP_NP(cap)    ((cap) & 0x1F)          /* Number of Ports */

/* AHCI Port Registers (offset 0x100 + port * 0x80) */
typedef volatile struct {
    uint32_t clb;           /* 0x00: Command List Base Address (low) */
    uint32_t clbu;          /* 0x04: Command List Base Address (high) */
    uint32_t fb;            /* 0x08: FIS Base Address (low) */
    uint32_t fbu;           /* 0x0C: FIS Base Address (high) */
    uint32_t is;            /* 0x10: Interrupt Status */
    uint32_t ie;            /* 0x14: Interrupt Enable */
    uint32_t cmd;           /* 0x18: Command and Status */
    uint32_t reserved0;     /* 0x1C */
    uint32_t tfd;           /* 0x20: Task File Data */
    uint32_t sig;           /* 0x24: Signature */
    uint32_t ssts;          /* 0x28: SATA Status */
    uint32_t sctl;          /* 0x2C: SATA Control */
    uint32_t serr;          /* 0x30: SATA Error */
    uint32_t sact;          /* 0x34: SATA Active */
    uint32_t ci;            /* 0x38: Command Issue */
    uint32_t sntf;          /* 0x3C: SATA Notification */
    uint32_t fbs;           /* 0x40: FIS-based Switching Control */
    uint32_t devslp;        /* 0x44: Device Sleep */
    uint8_t  reserved1[0x70 - 0x48];
    uint8_t  vendor[0x80 - 0x70];
} __attribute__((packed)) ahci_port_t;

/* Port CMD bits */
#define AHCI_PORT_CMD_ST    (1 << 0)    /* Start */
#define AHCI_PORT_CMD_SUD   (1 << 1)    /* Spin-Up Device */
#define AHCI_PORT_CMD_POD   (1 << 2)    /* Power On Device */
#define AHCI_PORT_CMD_CLO   (1 << 3)    /* Command List Override */
#define AHCI_PORT_CMD_FRE   (1 << 4)    /* FIS Receive Enable */
#define AHCI_PORT_CMD_CCS(cmd) (((cmd) >> 8) & 0x1F)  /* Current Command Slot */
#define AHCI_PORT_CMD_MPSS  (1 << 13)   /* Mechanical Presence Switch State */
#define AHCI_PORT_CMD_FR    (1 << 14)   /* FIS Receive Running */
#define AHCI_PORT_CMD_CR    (1 << 15)   /* Command List Running */
#define AHCI_PORT_CMD_CPS   (1 << 16)   /* Cold Presence State */
#define AHCI_PORT_CMD_PMA   (1 << 17)   /* Port Multiplier Attached */
#define AHCI_PORT_CMD_HPCP  (1 << 18)   /* Hot Plug Capable Port */
#define AHCI_PORT_CMD_MPSP  (1 << 19)   /* Mechanical Presence Switch Attached */
#define AHCI_PORT_CMD_CPD   (1 << 20)   /* Cold Presence Detection */
#define AHCI_PORT_CMD_ESP   (1 << 21)   /* External SATA Port */
#define AHCI_PORT_CMD_FBSCP (1 << 22)   /* FIS-based Switching Capable Port */
#define AHCI_PORT_CMD_APSTE (1 << 23)   /* Automatic Partial to Slumber Transitions Enabled */
#define AHCI_PORT_CMD_ATAPI (1 << 24)   /* Device is ATAPI */
#define AHCI_PORT_CMD_DLAE  (1 << 25)   /* Drive LED on ATAPI Enable */
#define AHCI_PORT_CMD_ALPE  (1 << 26)   /* Aggressive Link Power Management Enable */
#define AHCI_PORT_CMD_ASP   (1 << 27)   /* Aggressive Slumber/Partial */
#define AHCI_PORT_CMD_ICC(cmd) (((cmd) >> 28) & 0xF)  /* Interface Communication Control */

/* Port SSTS bits */
#define AHCI_PORT_SSTS_DET(ssts)  ((ssts) & 0xF)       /* Device Detection */
#define AHCI_PORT_SSTS_SPD(ssts)  (((ssts) >> 4) & 0xF)  /* Current Interface Speed */
#define AHCI_PORT_SSTS_IPM(ssts)  (((ssts) >> 8) & 0xF)  /* Interface Power Management */

/* DET values */
#define AHCI_PORT_DET_NONE      0   /* No device detected */
#define AHCI_PORT_DET_PRESENT   1   /* Device present, no Phy communication */
#define AHCI_PORT_DET_PHY       3   /* Device present and Phy communication established */
#define AHCI_PORT_DET_OFFLINE   4   /* Phy in offline mode */

/* Port TFD bits */
#define AHCI_PORT_TFD_ERR   (1 << 0)    /* Error */
#define AHCI_PORT_TFD_DRQ   (1 << 3)    /* Data Request */
#define AHCI_PORT_TFD_BSY   (1 << 7)    /* Busy */

/* Port signature values */
#define AHCI_SIG_ATA        0x00000101  /* SATA drive */
#define AHCI_SIG_ATAPI      0xEB140101  /* SATAPI drive */
#define AHCI_SIG_SEMB       0xC33C0101  /* Enclosure management bridge */
#define AHCI_SIG_PM         0x96690101  /* Port multiplier */

/* Port IS bits (Interrupt Status) */
#define AHCI_PORT_IS_DHRS   (1 << 0)    /* Device to Host Register FIS Interrupt */
#define AHCI_PORT_IS_PSS    (1 << 1)    /* PIO Setup FIS Interrupt */
#define AHCI_PORT_IS_DSS    (1 << 2)    /* DMA Setup FIS Interrupt */
#define AHCI_PORT_IS_SDBS   (1 << 3)    /* Set Device Bits Interrupt */
#define AHCI_PORT_IS_UFS    (1 << 4)    /* Unknown FIS Interrupt */
#define AHCI_PORT_IS_DPS    (1 << 5)    /* Descriptor Processed */
#define AHCI_PORT_IS_PCS    (1 << 6)    /* Port Connect Change Status */
#define AHCI_PORT_IS_DMPS   (1 << 7)    /* Device Mechanical Presence Status */
#define AHCI_PORT_IS_PRCS   (1 << 22)   /* PhyRdy Change Status */
#define AHCI_PORT_IS_IPMS   (1 << 23)   /* Incorrect Port Multiplier Status */
#define AHCI_PORT_IS_OFS    (1 << 24)   /* Overflow Status */
#define AHCI_PORT_IS_INFS   (1 << 26)   /* Interface Non-fatal Error Status */
#define AHCI_PORT_IS_IFS    (1 << 27)   /* Interface Fatal Error Status */
#define AHCI_PORT_IS_HBDS   (1 << 28)   /* Host Bus Data Error Status */
#define AHCI_PORT_IS_HBFS   (1 << 29)   /* Host Bus Fatal Error Status */
#define AHCI_PORT_IS_TFES   (1 << 30)   /* Task File Error Status */
#define AHCI_PORT_IS_CPDS   (1 << 31)   /* Cold Port Detect Status */

/* Command Header (32 bytes) */
typedef struct {
    uint16_t flags;         /* Command FIS length, ATAPI, Write, Prefetchable */
    uint16_t prdtl;         /* Physical Region Descriptor Table Length */
    uint32_t prdbc;         /* Physical Region Descriptor Byte Count */
    uint32_t ctba;          /* Command Table Base Address (low) */
    uint32_t ctbau;         /* Command Table Base Address (high) */
    uint32_t reserved[4];
} __attribute__((packed)) ahci_cmd_header_t;

/* Command header flags */
#define AHCI_CMD_FIS_LEN(len)   ((len) & 0x1F)      /* Command FIS length in DWORDs */
#define AHCI_CMD_ATAPI          (1 << 5)             /* ATAPI command */
#define AHCI_CMD_WRITE          (1 << 6)             /* Write command */
#define AHCI_CMD_PREFETCH       (1 << 7)             /* Prefetchable */
#define AHCI_CMD_RESET          (1 << 8)             /* Reset */
#define AHCI_CMD_BIST           (1 << 9)             /* BIST */
#define AHCI_CMD_CLR_BUSY       (1 << 10)            /* Clear Busy upon R_OK */
#define AHCI_CMD_PMP(pmp)       (((pmp) & 0xF) << 12) /* Port Multiplier Port */

/* Physical Region Descriptor Table Entry (16 bytes) */
typedef struct {
    uint32_t dba;           /* Data Base Address (low) */
    uint32_t dbau;          /* Data Base Address (high) */
    uint32_t reserved;
    uint32_t dbc;           /* Data Byte Count (bit 0 must be 1 for interrupt) */
} __attribute__((packed)) ahci_prdt_entry_t;

/* DBC flags */
#define AHCI_PRDT_DBC(count)    (((count) - 1) & 0x3FFFFF)  /* Byte count (max 4MB) */
#define AHCI_PRDT_INTERRUPT     (1 << 31)                   /* Interrupt on completion */

/* Command Table */
typedef struct {
    uint8_t cfis[64];           /* Command FIS */
    uint8_t acmd[16];           /* ATAPI Command */
    uint8_t reserved[48];
    ahci_prdt_entry_t prdt[8];  /* Physical Region Descriptor Table */
} __attribute__((packed)) ahci_cmd_table_t;

/* FIS Types */
#define FIS_TYPE_REG_H2D    0x27    /* Register FIS - host to device */
#define FIS_TYPE_REG_D2H    0x34    /* Register FIS - device to host */
#define FIS_TYPE_DMA_ACT    0x39    /* DMA activate FIS */
#define FIS_TYPE_DMA_SETUP  0x41    /* DMA setup FIS */
#define FIS_TYPE_DATA       0x46    /* Data FIS */
#define FIS_TYPE_BIST       0x58    /* BIST activate FIS */
#define FIS_TYPE_PIO_SETUP  0x5F    /* PIO setup FIS */
#define FIS_TYPE_DEV_BITS   0xA1    /* Set device bits FIS */

/* Register FIS - Host to Device (H2D) */
typedef struct {
    uint8_t fis_type;       /* FIS_TYPE_REG_H2D */
    uint8_t pmport:4;       /* Port multiplier */
    uint8_t rsv0:3;         /* Reserved */
    uint8_t c:1;            /* 1: Command, 0: Control */
    uint8_t command;        /* Command register */
    uint8_t featurel;       /* Feature register (low) */
    uint8_t lba0;           /* LBA low register */
    uint8_t lba1;           /* LBA mid register */
    uint8_t lba2;           /* LBA high register */
    uint8_t device;         /* Device register */
    uint8_t lba3;           /* LBA register (high) */
    uint8_t lba4;           /* LBA register (exp) */
    uint8_t lba5;           /* LBA register (exp) */
    uint8_t featureh;       /* Feature register (high) */
    uint8_t countl;         /* Count register (low) */
    uint8_t counth;         /* Count register (high) */
    uint8_t icc;            /* Isochronous command completion */
    uint8_t control;        /* Control register */
    uint8_t rsv1[4];        /* Reserved */
} __attribute__((packed)) fis_reg_h2d_t;

/* ATA Commands */
#define ATA_CMD_READ_DMA_EX     0x25    /* READ DMA EXT */
#define ATA_CMD_WRITE_DMA_EX    0x35    /* WRITE DMA EXT */
#define ATA_CMD_IDENTIFY        0xEC    /* IDENTIFY DEVICE */
#define ATA_CMD_FLUSH_CACHE_EX  0xEA    /* FLUSH CACHE EXT */

/* AHCI device info */
typedef struct {
    bool present;
    uint8_t port;
    uint32_t signature;
    char model[41];
    char serial[21];
    uint64_t sectors;       /* Total sectors (LBA) */
    uint32_t sector_size;   /* Bytes per sector (usually 512) */
} ahci_device_t;

/* Maximum ports and devices */
#define AHCI_MAX_PORTS      32
#define AHCI_MAX_DEVICES    8

/* Block device interface (from ide.h) */
typedef struct block_device block_device_t;

/* Function declarations */
int ahci_init(void);
int ahci_read(int dev, uint64_t lba, uint32_t count, void *buf);
int ahci_write(int dev, uint64_t lba, uint32_t count, void *buf);
int ahci_get_device_count(void);
ahci_device_t *ahci_get_device(int index);

/* Block device interface */
block_device_t *ahci_get_block_device(int index);

#endif /* _AHCI_H */
