/*
 * ahci.c - AHCI (SATA) Driver
 *
 * Implements AHCI driver for SATA devices
 */

#include "drivers/ahci.h"
#include "drivers/pci.h"
#include "drivers/ide.h"  /* For block_device_t */
#include "mm/pmm.h"
#include "mm/heap.h"
#include "mm/vmm.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* AHCI HBA base address (MMIO) */
static ahci_hba_mem_t *hba_mem = NULL;

/* Device list */
static ahci_device_t ahci_devices[AHCI_MAX_DEVICES];
static int ahci_device_count = 0;

/* Block devices */
static block_device_t ahci_blk_devices[AHCI_MAX_DEVICES];

/* Forward declarations for block device callbacks */
static int ahci_blk_read(block_device_t *dev, uint64_t lba, void *buf, size_t count);
static int ahci_blk_write(block_device_t *dev, uint64_t lba, const void *buf, size_t count);

/* Per-port structures */
static ahci_cmd_header_t *cmd_lists[AHCI_MAX_PORTS];
static ahci_cmd_table_t *cmd_tables[AHCI_MAX_PORTS];
static uint8_t *fis_bases[AHCI_MAX_PORTS];

/* Helper: get port registers */
static ahci_port_t *ahci_port(int port) {
    return (ahci_port_t *)((uint8_t *)hba_mem + 0x100 + port * 0x80);
}

/* Helper: wait for port to be idle */
static int ahci_port_stop(int port) {
    ahci_port_t *p = ahci_port(port);

    /* Clear ST (stop command engine) */
    p->cmd &= ~AHCI_PORT_CMD_ST;

    /* Wait for CR to clear */
    for (int i = 0; i < 1000000; i++) {
        if (!(p->cmd & AHCI_PORT_CMD_CR)) {
            break;
        }
    }

    /* Clear FRE */
    p->cmd &= ~AHCI_PORT_CMD_FRE;

    /* Wait for FR to clear */
    for (int i = 0; i < 1000000; i++) {
        if (!(p->cmd & AHCI_PORT_CMD_FR)) {
            return 0;
        }
    }

    return -1;
}

/* Helper: start port */
static void ahci_port_start(int port) {
    ahci_port_t *p = ahci_port(port);

    /* Wait for CR to clear */
    while (p->cmd & AHCI_PORT_CMD_CR) {
        /* spin */
    }

    /* Enable FIS receive */
    p->cmd |= AHCI_PORT_CMD_FRE;

    /* Start command engine */
    p->cmd |= AHCI_PORT_CMD_ST;
}

/* Helper: find free command slot */
static int ahci_find_cmdslot(int port) {
    ahci_port_t *p = ahci_port(port);
    uint32_t slots = p->sact | p->ci;
    int num_slots = AHCI_CAP_NCS(hba_mem->cap) + 1;

    for (int i = 0; i < num_slots; i++) {
        if (!(slots & (1 << i))) {
            return i;
        }
    }
    return -1;
}

/* Initialize a port's memory structures */
static int ahci_init_port(int port) {
    ahci_port_t *p = ahci_port(port);

    /* Stop the port */
    ahci_port_stop(port);

    /* Allocate command list (1KB, 1KB aligned) */
    cmd_lists[port] = (ahci_cmd_header_t *)pmm_alloc_page();
    if (!cmd_lists[port]) {
        kprintf("[AHCI] Failed to allocate command list for port %d\n", port);
        return -1;
    }
    memset(cmd_lists[port], 0, PAGE_SIZE);

    /* Allocate FIS receive area (256 bytes, 256 aligned) */
    fis_bases[port] = (uint8_t *)pmm_alloc_page();
    if (!fis_bases[port]) {
        kprintf("[AHCI] Failed to allocate FIS base for port %d\n", port);
        return -1;
    }
    memset(fis_bases[port], 0, PAGE_SIZE);

    /* Allocate command table (8KB for 32 slots) */
    cmd_tables[port] = (ahci_cmd_table_t *)pmm_alloc_page();
    if (!cmd_tables[port]) {
        kprintf("[AHCI] Failed to allocate command table for port %d\n", port);
        return -1;
    }
    memset(cmd_tables[port], 0, PAGE_SIZE);

    /* Set up command list base */
    p->clb = (uint32_t)(uint64_t)cmd_lists[port];
    p->clbu = (uint32_t)((uint64_t)cmd_lists[port] >> 32);

    /* Set up FIS base */
    p->fb = (uint32_t)(uint64_t)fis_bases[port];
    p->fbu = (uint32_t)((uint64_t)fis_bases[port] >> 32);

    /* Set up command headers to point to command tables */
    for (int i = 0; i < 32; i++) {
        cmd_lists[port][i].prdtl = 8;  /* 8 PRDT entries */
        cmd_lists[port][i].ctba = (uint32_t)(uint64_t)&cmd_tables[port][0];
        cmd_lists[port][i].ctbau = (uint32_t)((uint64_t)&cmd_tables[port][0] >> 32);
    }

    /* Clear interrupts */
    p->serr = 0xFFFFFFFF;
    p->is = 0xFFFFFFFF;

    /* Start the port */
    ahci_port_start(port);

    return 0;
}

/* Check port type */
static int ahci_check_port_type(int port) {
    ahci_port_t *p = ahci_port(port);

    uint32_t ssts = p->ssts;
    uint8_t det = AHCI_PORT_SSTS_DET(ssts);
    uint8_t ipm = AHCI_PORT_SSTS_IPM(ssts);

    /* Check if device present and Phy ready */
    if (det != AHCI_PORT_DET_PHY) {
        return -1;  /* No device */
    }

    /* Check if device in active state */
    if (ipm != 1) {
        return -1;  /* Not active */
    }

    /* Return device signature */
    switch (p->sig) {
        case AHCI_SIG_ATA:
            return 0;   /* SATA drive */
        case AHCI_SIG_ATAPI:
            return 1;   /* ATAPI drive */
        case AHCI_SIG_SEMB:
            return 2;   /* SEMB */
        case AHCI_SIG_PM:
            return 3;   /* Port multiplier */
        default:
            return 0;   /* Assume SATA */
    }
}

/* Send IDENTIFY command */
static int ahci_identify(int port, uint16_t *buf) {
    ahci_port_t *p = ahci_port(port);

    /* Find free slot */
    int slot = ahci_find_cmdslot(port);
    if (slot < 0) {
        return -1;
    }

    /* Set up command header */
    ahci_cmd_header_t *cmdheader = &cmd_lists[port][slot];
    cmdheader->flags = AHCI_CMD_FIS_LEN(5);  /* FIS length: 5 DWORDs */
    cmdheader->prdtl = 1;
    cmdheader->prdbc = 0;

    /* Set up command table */
    ahci_cmd_table_t *cmdtbl = cmd_tables[port];
    memset(cmdtbl, 0, sizeof(ahci_cmd_table_t));

    /* Set up PRDT */
    cmdtbl->prdt[0].dba = (uint32_t)(uint64_t)buf;
    cmdtbl->prdt[0].dbau = (uint32_t)((uint64_t)buf >> 32);
    cmdtbl->prdt[0].dbc = AHCI_PRDT_DBC(512) | AHCI_PRDT_INTERRUPT;

    /* Set up FIS */
    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)cmdtbl->cfis;
    memset(fis, 0, sizeof(fis_reg_h2d_t));
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = ATA_CMD_IDENTIFY;

    /* Wait for port not busy */
    int timeout = 1000000;
    while ((p->tfd & (AHCI_PORT_TFD_BSY | AHCI_PORT_TFD_DRQ)) && --timeout) {
        /* spin */
    }
    if (!timeout) {
        return -1;
    }

    /* Issue command */
    p->ci = 1 << slot;

    /* Wait for completion */
    timeout = 1000000;
    while (--timeout) {
        if (!(p->ci & (1 << slot))) {
            break;
        }
        if (p->is & AHCI_PORT_IS_TFES) {
            return -1;  /* Task file error */
        }
    }

    if (!timeout) {
        return -1;
    }

    /* Check for errors */
    if (p->is & AHCI_PORT_IS_TFES) {
        return -1;
    }

    return 0;
}

/* Read/Write sectors */
static int ahci_rw(int dev, uint64_t lba, uint32_t count, void *buf, int write) {
    if (dev < 0 || dev >= ahci_device_count) {
        return -1;
    }

    ahci_device_t *device = &ahci_devices[dev];
    if (!device->present) {
        return -1;
    }

    int port = device->port;
    ahci_port_t *p = ahci_port(port);

    /* Find free slot */
    int slot = ahci_find_cmdslot(port);
    if (slot < 0) {
        return -1;
    }

    /* Set up command header */
    ahci_cmd_header_t *cmdheader = &cmd_lists[port][slot];
    cmdheader->flags = AHCI_CMD_FIS_LEN(5);
    if (write) {
        cmdheader->flags |= AHCI_CMD_WRITE;
    }
    cmdheader->prdtl = 1;
    cmdheader->prdbc = 0;

    /* Set up command table */
    ahci_cmd_table_t *cmdtbl = cmd_tables[port];
    memset(cmdtbl, 0, sizeof(ahci_cmd_table_t));

    /* Set up PRDT */
    uint32_t byte_count = count * device->sector_size;
    cmdtbl->prdt[0].dba = (uint32_t)(uint64_t)buf;
    cmdtbl->prdt[0].dbau = (uint32_t)((uint64_t)buf >> 32);
    cmdtbl->prdt[0].dbc = AHCI_PRDT_DBC(byte_count) | AHCI_PRDT_INTERRUPT;

    /* Set up FIS */
    fis_reg_h2d_t *fis = (fis_reg_h2d_t *)cmdtbl->cfis;
    memset(fis, 0, sizeof(fis_reg_h2d_t));
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->c = 1;
    fis->command = write ? ATA_CMD_WRITE_DMA_EX : ATA_CMD_READ_DMA_EX;
    fis->device = 1 << 6;  /* LBA mode */

    fis->lba0 = (uint8_t)(lba & 0xFF);
    fis->lba1 = (uint8_t)((lba >> 8) & 0xFF);
    fis->lba2 = (uint8_t)((lba >> 16) & 0xFF);
    fis->lba3 = (uint8_t)((lba >> 24) & 0xFF);
    fis->lba4 = (uint8_t)((lba >> 32) & 0xFF);
    fis->lba5 = (uint8_t)((lba >> 40) & 0xFF);

    fis->countl = count & 0xFF;
    fis->counth = (count >> 8) & 0xFF;

    /* Wait for port not busy */
    int timeout = 1000000;
    while ((p->tfd & (AHCI_PORT_TFD_BSY | AHCI_PORT_TFD_DRQ)) && --timeout) {
        /* spin */
    }
    if (!timeout) {
        return -1;
    }

    /* Issue command */
    p->ci = 1 << slot;

    /* Wait for completion */
    timeout = 1000000;
    while (--timeout) {
        if (!(p->ci & (1 << slot))) {
            break;
        }
        if (p->is & AHCI_PORT_IS_TFES) {
            return -1;
        }
    }

    if (!timeout) {
        return -1;
    }

    if (p->is & AHCI_PORT_IS_TFES) {
        return -1;
    }

    return 0;
}

/* Copy string from identify data (byte swapped) */
static void ahci_copy_string(char *dest, uint16_t *src, int words) {
    for (int i = 0; i < words; i++) {
        dest[i * 2] = src[i] >> 8;
        dest[i * 2 + 1] = src[i] & 0xFF;
    }
    dest[words * 2] = '\0';

    /* Trim trailing spaces */
    for (int i = words * 2 - 1; i >= 0 && dest[i] == ' '; i--) {
        dest[i] = '\0';
    }
}

/* Probe a port for devices */
static void ahci_probe_port(int port) {
    int type = ahci_check_port_type(port);
    if (type < 0) {
        return;  /* No device */
    }

    if (type != 0) {
        kprintf("[AHCI] Port %d: Non-SATA device (type %d), skipping\n", port, type);
        return;
    }

    /* Initialize port */
    if (ahci_init_port(port) < 0) {
        kprintf("[AHCI] Port %d: Failed to initialize\n", port);
        return;
    }

    /* Allocate buffer for IDENTIFY */
    uint16_t *identify_buf = (uint16_t *)pmm_alloc_page();
    if (!identify_buf) {
        kprintf("[AHCI] Failed to allocate identify buffer\n");
        return;
    }
    memset(identify_buf, 0, PAGE_SIZE);

    /* Send IDENTIFY command */
    if (ahci_identify(port, identify_buf) < 0) {
        kprintf("[AHCI] Port %d: IDENTIFY failed\n", port);
        pmm_free_page((void *)identify_buf);
        return;
    }

    /* Extract device info */
    if (ahci_device_count >= AHCI_MAX_DEVICES) {
        pmm_free_page((void *)identify_buf);
        return;
    }

    ahci_device_t *dev = &ahci_devices[ahci_device_count];
    dev->present = true;
    dev->port = port;
    dev->signature = ahci_port(port)->sig;

    /* Copy model and serial */
    ahci_copy_string(dev->model, &identify_buf[27], 20);
    ahci_copy_string(dev->serial, &identify_buf[10], 10);

    /* Get sector count (LBA48) */
    dev->sectors = *(uint64_t *)&identify_buf[100];
    if (dev->sectors == 0) {
        /* Fall back to LBA28 */
        dev->sectors = *(uint32_t *)&identify_buf[60];
    }

    /* Sector size (usually 512) */
    dev->sector_size = 512;
    if (identify_buf[106] & (1 << 12)) {
        /* Logical sector size is larger than 512 bytes */
        dev->sector_size = *(uint32_t *)&identify_buf[117] * 2;
    }

    uint64_t size_mb = (dev->sectors * dev->sector_size) / (1024 * 1024);
    kprintf("[AHCI] Port %d: %s (%llu MB)\n", port, dev->model, size_mb);

    /* Create block device */
    block_device_t *blk = &ahci_blk_devices[ahci_device_count];
    blk->name[0] = 's';
    blk->name[1] = 'd';
    blk->name[2] = 'a' + ahci_device_count;
    blk->name[3] = '\0';
    blk->size = dev->sectors * dev->sector_size;
    blk->block_size = dev->sector_size;
    blk->read = ahci_blk_read;
    blk->write = ahci_blk_write;
    blk->priv = (void *)(uintptr_t)ahci_device_count;

    pmm_free_page((void *)identify_buf);
    ahci_device_count++;
}

/* Initialize AHCI controller */
int ahci_init(void) {
    kprintf("[AHCI] Initializing AHCI driver...\n");

    /* Find AHCI controller */
    pci_device_t *pci = pci_find_class_prog(AHCI_CLASS, AHCI_SUBCLASS, AHCI_PROG_IF);
    if (!pci) {
        /* Try without prog_if check */
        pci = pci_find_class(AHCI_CLASS, AHCI_SUBCLASS);
    }

    if (!pci) {
        kprintf("[AHCI] No AHCI controller found\n");
        return -1;
    }

    kprintf("[AHCI] Found controller: %04x:%04x at %02x:%02x.%d\n",
            pci->vendor_id, pci->device_id, pci->bus, pci->slot, pci->func);

    /* Enable bus mastering and memory space */
    pci_enable_bus_master(pci);

    /* Get ABAR (BAR5) */
    uint32_t abar = pci_get_bar(pci, 5);
    if (!abar) {
        kprintf("[AHCI] Invalid ABAR\n");
        return -1;
    }

    kprintf("[AHCI] ABAR: 0x%x\n", abar);

    /* Map HBA memory - AHCI registers are in MMIO space */
    /* Need to map multiple pages for HBA + ports */
    uint64_t abar_base = abar & ~0xFFF;  /* Page-align */
    for (int page = 0; page < 16; page++) {  /* Map 64KB for HBA + 32 ports */
        uint64_t addr = abar_base + page * PAGE_SIZE;
        if (vmm_map_page(addr, addr, PTE_PRESENT | PTE_WRITABLE | PTE_PCD) != 0) {
            kprintf("[AHCI] Warning: Failed to map MMIO page 0x%lx\n", addr);
        }
    }

    hba_mem = (ahci_hba_mem_t *)(uint64_t)abar;

    /* Read capabilities */
    uint32_t cap = hba_mem->cap;
    uint32_t pi = hba_mem->pi;
    uint32_t vs = hba_mem->vs;

    int num_ports = AHCI_CAP_NP(cap) + 1;
    int num_cmd_slots = AHCI_CAP_NCS(cap) + 1;
    bool supports_64bit = cap & AHCI_CAP_S64A;

    kprintf("[AHCI] Version: %d.%d\n", (vs >> 16) & 0xFFFF, vs & 0xFFFF);
    kprintf("[AHCI] Ports: %d, Command slots: %d, 64-bit: %s\n",
            num_ports, num_cmd_slots, supports_64bit ? "yes" : "no");

    /* Enable AHCI mode */
    hba_mem->ghc |= AHCI_GHC_AE;

    /* Probe all implemented ports */
    for (int i = 0; i < 32; i++) {
        if (pi & (1 << i)) {
            ahci_probe_port(i);
        }
    }

    if (ahci_device_count == 0) {
        kprintf("[AHCI] No devices found\n");
        return -1;
    }

    kprintf("[AHCI] Found %d device(s)\n", ahci_device_count);
    return 0;
}

/* Read sectors */
int ahci_read(int dev, uint64_t lba, uint32_t count, void *buf) {
    return ahci_rw(dev, lba, count, buf, 0);
}

/* Write sectors */
int ahci_write(int dev, uint64_t lba, uint32_t count, void *buf) {
    return ahci_rw(dev, lba, count, buf, 1);
}

/* Get device count */
int ahci_get_device_count(void) {
    return ahci_device_count;
}

/* Get device info */
ahci_device_t *ahci_get_device(int index) {
    if (index < 0 || index >= ahci_device_count) {
        return NULL;
    }
    return &ahci_devices[index];
}

/* Block device read callback */
static int ahci_blk_read(block_device_t *dev, uint64_t lba,
                         void *buf, size_t count) {
    int idx = (int)(uintptr_t)dev->priv;
    if (count > 128) count = 128;  /* Limit to 64KB per transfer */
    return ahci_read(idx, lba, count, buf);
}

/* Block device write callback */
static int ahci_blk_write(block_device_t *dev, uint64_t lba,
                          const void *buf, size_t count) {
    int idx = (int)(uintptr_t)dev->priv;
    if (count > 128) count = 128;
    return ahci_write(idx, lba, count, (void *)buf);
}

/* Get block device by index */
block_device_t *ahci_get_block_device(int index) {
    if (index < 0 || index >= ahci_device_count) {
        return NULL;
    }
    return &ahci_blk_devices[index];
}
