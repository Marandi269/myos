/*
 * ide.c - IDE/ATA disk driver
 *
 * Implements PIO mode IDE disk access for read/write operations.
 * Supports LBA28 addressing (up to 128GB).
 */

#include "ide.h"
#include "../lib/kprintf.h"
#include "../lib/string.h"

/* Port I/O functions */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void insw(uint16_t port, void *addr, int cnt) {
    __asm__ volatile(
        "rep insw"
        : "+D"(addr), "+c"(cnt)
        : "d"(port)
        : "memory"
    );
}

static inline void outsw(uint16_t port, const void *addr, int cnt) {
    __asm__ volatile(
        "rep outsw"
        : "+S"(addr), "+c"(cnt)
        : "d"(port)
    );
}

/* IDE channels */
static ide_channel_t channels[2] = {
    { .base = IDE_DATA, .ctrl = IDE_CTRL, .bmide = 0, .nIEN = 2 },
    { .base = IDE2_DATA, .ctrl = IDE2_CTRL, .bmide = 0, .nIEN = 2 }
};

/* IDE devices */
#define MAX_IDE_DEVICES 4
static ide_device_t ide_devices[MAX_IDE_DEVICES];
static int ide_device_num = 0;

/* Block devices */
static block_device_t blk_devices[MAX_IDE_DEVICES];

/* Small delay for IDE timing */
static void ide_delay(ide_channel_t *ch) {
    /* Read alternate status register 4 times for 400ns delay */
    inb(ch->ctrl);
    inb(ch->ctrl);
    inb(ch->ctrl);
    inb(ch->ctrl);
}

/* Wait for drive to be ready (not busy) */
static int ide_wait(ide_channel_t *ch, int check_drq) {
    int timeout = IDE_TIMEOUT;

    /* Wait for BSY to clear */
    while ((inb(ch->base + 7) & IDE_SR_BSY) && --timeout > 0)
        ;

    if (timeout <= 0)
        return -1;

    if (check_drq) {
        uint8_t status = inb(ch->base + 7);
        if (status & IDE_SR_ERR)
            return -2;
        if (status & IDE_SR_DWF)
            return -3;
        if (!(status & IDE_SR_DRQ))
            return -4;
    }

    return 0;
}

/* Wait until drive is ready */
static int ide_wait_ready(ide_channel_t *ch) {
    int timeout = IDE_TIMEOUT;

    while (--timeout > 0) {
        uint8_t status = inb(ch->base + 7);
        if (!(status & IDE_SR_BSY) && (status & IDE_SR_DRDY))
            return 0;
    }

    return -1;
}

/* Select drive */
static void ide_select_drive(int channel, int drive) {
    ide_channel_t *ch = &channels[channel];
    uint8_t val = 0xA0 | (drive << 4) | IDE_DRIVE_LBA;
    outb(ch->base + 6, val);
    ide_delay(ch);
}

/* Identify drive */
static int ide_identify(int channel, int drive, uint16_t *buf) {
    ide_channel_t *ch = &channels[channel];

    /* Select drive */
    ide_select_drive(channel, drive);

    /* Disable interrupts */
    outb(ch->ctrl, ch->nIEN);

    /* Send IDENTIFY command */
    outb(ch->base + 2, 0);  /* Sector count */
    outb(ch->base + 3, 0);  /* LBA low */
    outb(ch->base + 4, 0);  /* LBA mid */
    outb(ch->base + 5, 0);  /* LBA high */
    outb(ch->base + 7, ATA_CMD_IDENTIFY);

    ide_delay(ch);

    /* Check if drive exists */
    uint8_t status = inb(ch->base + 7);
    if (status == 0)
        return -1;  /* No drive */

    /* Wait for drive to be ready */
    int timeout = IDE_TIMEOUT;
    while (--timeout > 0) {
        status = inb(ch->base + 7);
        if (!(status & IDE_SR_BSY))
            break;
    }
    if (timeout <= 0)
        return -2;  /* Timeout */

    /* Check for ATAPI */
    uint8_t lba_mid = inb(ch->base + 4);
    uint8_t lba_high = inb(ch->base + 5);
    if (lba_mid == 0x14 && lba_high == 0xEB) {
        /* ATAPI device - send IDENTIFY PACKET command instead */
        outb(ch->base + 7, ATA_CMD_IDENTIFY_PACKET);
        ide_delay(ch);

        timeout = IDE_TIMEOUT;
        while (--timeout > 0) {
            status = inb(ch->base + 7);
            if (!(status & IDE_SR_BSY))
                break;
        }
        if (timeout <= 0)
            return -3;
    } else if (lba_mid == 0x3C && lba_high == 0xC3) {
        /* SATA device - treat as ATA */
    } else if (lba_mid != 0 || lba_high != 0) {
        return -4;  /* Unknown device type */
    }

    /* Wait for DRQ */
    if (ide_wait(ch, 1) < 0)
        return -5;

    /* Read identification space */
    insw(ch->base, buf, 256);

    return 0;
}

/* Read sectors using PIO */
int ide_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, void *buf) {
    if (drive >= MAX_IDE_DEVICES || !ide_devices[drive].present)
        return -1;

    ide_device_t *dev = &ide_devices[drive];
    ide_channel_t *ch = &channels[dev->channel];

    /* Select drive first */
    uint8_t drive_val = 0xE0 | (dev->drive << 4) | ((lba >> 24) & 0x0F);
    outb(ch->base + 6, drive_val);
    ide_delay(ch);

    /* Wait for drive to be ready */
    if (ide_wait_ready(ch) < 0)
        return -2;

    /* Disable interrupts */
    outb(ch->ctrl, ch->nIEN);

    /* Set sector count and LBA */
    outb(ch->base + 2, count);
    outb(ch->base + 3, lba & 0xFF);
    outb(ch->base + 4, (lba >> 8) & 0xFF);
    outb(ch->base + 5, (lba >> 16) & 0xFF);

    /* Send READ command */
    outb(ch->base + 7, ATA_CMD_READ_PIO);

    /* Read sectors */
    uint16_t *ptr = (uint16_t *)buf;
    for (int i = 0; i < count; i++) {
        /* Wait for data */
        if (ide_wait(ch, 1) < 0)
            return -3;

        /* Read 256 words (512 bytes) */
        insw(ch->base, ptr, 256);
        ptr += 256;
    }

    return count;
}

/* Write sectors using PIO */
int ide_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, const void *buf) {
    if (drive >= MAX_IDE_DEVICES || !ide_devices[drive].present)
        return -1;

    ide_device_t *dev = &ide_devices[drive];
    ide_channel_t *ch = &channels[dev->channel];

    /* Select drive first */
    uint8_t drive_val = 0xE0 | (dev->drive << 4) | ((lba >> 24) & 0x0F);
    outb(ch->base + 6, drive_val);
    ide_delay(ch);

    /* Wait for drive to be ready */
    if (ide_wait_ready(ch) < 0)
        return -2;

    /* Disable interrupts */
    outb(ch->ctrl, ch->nIEN);

    /* Set sector count and LBA */
    outb(ch->base + 2, count);
    outb(ch->base + 3, lba & 0xFF);
    outb(ch->base + 4, (lba >> 8) & 0xFF);
    outb(ch->base + 5, (lba >> 16) & 0xFF);

    /* Send WRITE command */
    outb(ch->base + 7, ATA_CMD_WRITE_PIO);

    /* Write sectors */
    const uint16_t *ptr = (const uint16_t *)buf;
    for (int i = 0; i < count; i++) {
        /* Wait for drive to be ready for data */
        if (ide_wait(ch, 1) < 0)
            return -3;

        /* Write 256 words (512 bytes) */
        outsw(ch->base, ptr, 256);
        ptr += 256;
    }

    /* Flush cache */
    outb(ch->base + 7, ATA_CMD_CACHE_FLUSH);
    if (ide_wait(ch, 0) < 0)
        return -4;

    return count;
}

/* Block device read callback */
static int blk_read(block_device_t *dev, uint64_t lba,
                    void *buf, size_t count) {
    int drive = (int)(uintptr_t)dev->priv;

    if (count > 255)
        count = 255;

    return ide_read_sectors(drive, (uint32_t)lba, count, buf);
}

/* Block device write callback */
static int blk_write(block_device_t *dev, uint64_t lba,
                     const void *buf, size_t count) {
    int drive = (int)(uintptr_t)dev->priv;

    if (count > 255)
        count = 255;

    return ide_write_sectors(drive, (uint32_t)lba, count, buf);
}

/* Swap bytes in a string (ATA strings are byte-swapped) */
static void ide_string_swap(char *str, int len) {
    for (int i = 0; i < len; i += 2) {
        char tmp = str[i];
        str[i] = str[i + 1];
        str[i + 1] = tmp;
    }
    /* Trim trailing spaces */
    while (len > 0 && str[len - 1] == ' ')
        str[--len] = '\0';
}

/* Initialize IDE driver */
void ide_init(void) {
    uint16_t identify_buf[256];
    char drive_names[] = "hda\0hdb\0hdc\0hdd";

    kprintf("[IDE] Initializing IDE/ATA driver...\n");

    ide_device_num = 0;

    /* Scan both channels */
    for (int ch = 0; ch < 2; ch++) {
        /* Scan both drives on each channel */
        for (int drv = 0; drv < 2; drv++) {
            int idx = ch * 2 + drv;

            memset(&ide_devices[idx], 0, sizeof(ide_device_t));
            ide_devices[idx].channel = ch;
            ide_devices[idx].drive = drv;

            /* Try to identify drive */
            int ret = ide_identify(ch, drv, identify_buf);
            if (ret < 0)
                continue;

            ide_devices[idx].present = 1;

            /* Check device type */
            uint8_t lba_mid = inb(channels[ch].base + 4);
            uint8_t lba_high = inb(channels[ch].base + 5);
            if (lba_mid == 0x14 && lba_high == 0xEB) {
                ide_devices[idx].type = IDE_TYPE_ATAPI;
            } else {
                ide_devices[idx].type = IDE_TYPE_ATA;
            }

            /* Get device info */
            ide_devices[idx].signature = identify_buf[0];
            ide_devices[idx].capabilities = identify_buf[49];
            ide_devices[idx].command_sets = (identify_buf[83] << 16) | identify_buf[82];

            /* Get size */
            if (ide_devices[idx].command_sets & (1 << 26)) {
                /* 48-bit LBA supported */
                ide_devices[idx].size =
                    ((uint64_t)identify_buf[103] << 48) |
                    ((uint64_t)identify_buf[102] << 32) |
                    ((uint64_t)identify_buf[101] << 16) |
                    identify_buf[100];
            } else {
                /* 28-bit LBA */
                ide_devices[idx].size =
                    ((uint32_t)identify_buf[61] << 16) | identify_buf[60];
            }

            /* Get model string */
            memcpy(ide_devices[idx].model, &identify_buf[27], 40);
            ide_devices[idx].model[40] = '\0';
            ide_string_swap(ide_devices[idx].model, 40);

            /* Create block device */
            block_device_t *blk = &blk_devices[ide_device_num];
            memcpy(blk->name, &drive_names[idx * 4], 4);
            blk->size = ide_devices[idx].size * IDE_SECTOR_SIZE;
            blk->block_size = IDE_SECTOR_SIZE;
            blk->read = blk_read;
            blk->write = blk_write;
            blk->priv = (void *)(uintptr_t)idx;

            /* Print device info */
            uint64_t size_mb = (ide_devices[idx].size * IDE_SECTOR_SIZE) / (1024 * 1024);
            kprintf("[IDE] %s: %s %s, %lu MB\n",
                    blk->name,
                    ide_devices[idx].type == IDE_TYPE_ATA ? "ATA" : "ATAPI",
                    ide_devices[idx].model,
                    size_mb);

            ide_device_num++;
        }
    }

    if (ide_device_num == 0) {
        kprintf("[IDE] No drives detected\n");
    } else {
        kprintf("[IDE] Found %d drive(s)\n", ide_device_num);
    }
}

/* Get block device by index */
block_device_t *ide_get_device(int index) {
    if (index < 0 || index >= ide_device_num)
        return NULL;
    return &blk_devices[index];
}

/* Get number of IDE devices */
int ide_device_count(void) {
    return ide_device_num;
}
