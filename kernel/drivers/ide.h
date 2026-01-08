/*
 * ide.h - IDE/ATA disk driver header
 */

#ifndef IDE_H
#define IDE_H

#include "types.h"

/* IDE I/O ports (Primary channel) */
#define IDE_DATA        0x1F0   /* Data register (R/W) */
#define IDE_ERROR       0x1F1   /* Error register (R) / Features (W) */
#define IDE_FEATURES    0x1F1
#define IDE_SECCOUNT    0x1F2   /* Sector count */
#define IDE_LBA0        0x1F3   /* LBA low byte */
#define IDE_LBA1        0x1F4   /* LBA mid byte */
#define IDE_LBA2        0x1F5   /* LBA high byte */
#define IDE_DRIVE       0x1F6   /* Drive/Head register */
#define IDE_STATUS      0x1F7   /* Status (R) / Command (W) */
#define IDE_COMMAND     0x1F7

/* IDE control port */
#define IDE_CTRL        0x3F6   /* Alternate Status (R) / Control (W) */

/* Secondary channel ports */
#define IDE2_DATA       0x170
#define IDE2_ERROR      0x171
#define IDE2_SECCOUNT   0x172
#define IDE2_LBA0       0x173
#define IDE2_LBA1       0x174
#define IDE2_LBA2       0x175
#define IDE2_DRIVE      0x176
#define IDE2_STATUS     0x177
#define IDE2_CTRL       0x376

/* Status register bits */
#define IDE_SR_ERR      0x01    /* Error */
#define IDE_SR_IDX      0x02    /* Index */
#define IDE_SR_CORR     0x04    /* Corrected data */
#define IDE_SR_DRQ      0x08    /* Data request */
#define IDE_SR_DSC      0x10    /* Drive seek complete */
#define IDE_SR_DWF      0x20    /* Drive write fault */
#define IDE_SR_DRDY     0x40    /* Drive ready */
#define IDE_SR_BSY      0x80    /* Busy */

/* Error register bits */
#define IDE_ER_AMNF     0x01    /* Address mark not found */
#define IDE_ER_TK0NF    0x02    /* Track 0 not found */
#define IDE_ER_ABRT     0x04    /* Command aborted */
#define IDE_ER_MCR      0x08    /* Media change request */
#define IDE_ER_IDNF     0x10    /* ID mark not found */
#define IDE_ER_MC       0x20    /* Media changed */
#define IDE_ER_UNC      0x40    /* Uncorrectable data error */
#define IDE_ER_BBK      0x80    /* Bad block detected */

/* ATA commands */
#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_READ_PIO_EXT    0x24
#define ATA_CMD_READ_DMA        0xC8
#define ATA_CMD_READ_DMA_EXT    0x25
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_WRITE_PIO_EXT   0x34
#define ATA_CMD_WRITE_DMA       0xCA
#define ATA_CMD_WRITE_DMA_EXT   0x35
#define ATA_CMD_CACHE_FLUSH     0xE7
#define ATA_CMD_CACHE_FLUSH_EXT 0xEA
#define ATA_CMD_PACKET          0xA0
#define ATA_CMD_IDENTIFY        0xEC
#define ATA_CMD_IDENTIFY_PACKET 0xA1

/* Drive select values */
#define IDE_DRIVE_MASTER    0xA0    /* Select master drive with LBA mode */
#define IDE_DRIVE_SLAVE     0xB0    /* Select slave drive with LBA mode */
#define IDE_DRIVE_LBA       0x40    /* LBA mode bit */

/* IDE device types */
#define IDE_TYPE_NONE       0
#define IDE_TYPE_ATA        1
#define IDE_TYPE_ATAPI      2

/* Sector size */
#define IDE_SECTOR_SIZE     512

/* Maximum wait time in loop iterations */
#define IDE_TIMEOUT         100000

/* IDE channel structure */
typedef struct ide_channel {
    uint16_t base;          /* I/O base port */
    uint16_t ctrl;          /* Control port */
    uint16_t bmide;         /* Bus master IDE (for DMA) */
    uint8_t  nIEN;          /* nIEN (interrupt enable) */
} ide_channel_t;

/* IDE device structure */
typedef struct ide_device {
    uint8_t  present;       /* Device is present */
    uint8_t  channel;       /* 0 = Primary, 1 = Secondary */
    uint8_t  drive;         /* 0 = Master, 1 = Slave */
    uint8_t  type;          /* ATA or ATAPI */
    uint16_t signature;
    uint16_t capabilities;
    uint32_t command_sets;
    uint64_t size;          /* Size in sectors */
    char     model[41];     /* Model string */
} ide_device_t;

/* Block device interface */
typedef struct block_device {
    char name[16];              /* Device name (e.g., "hda") */
    uint64_t size;              /* Total size in bytes */
    uint32_t block_size;        /* Block size (usually 512) */
    int (*read)(struct block_device *dev, uint64_t lba,
                void *buf, size_t count);
    int (*write)(struct block_device *dev, uint64_t lba,
                 const void *buf, size_t count);
    void *priv;                 /* Private driver data */
} block_device_t;

/* Functions */
void ide_init(void);
int ide_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, void *buf);
int ide_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, const void *buf);

/* Block device interface */
block_device_t *ide_get_device(int index);
int ide_device_count(void);

#endif /* IDE_H */
