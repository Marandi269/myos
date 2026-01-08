/*
 * pci.c - PCI Bus Driver
 */

#include "drivers/pci.h"
#include "lib/kprintf.h"

/* I/O port access functions */
static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* PCI device list */
static pci_device_t pci_devices[PCI_MAX_DEVICES];
static int pci_device_count = 0;

/* Build PCI address for configuration access */
static uint32_t pci_address(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    return (uint32_t)((bus << 16) | (slot << 11) | (func << 8) |
                      (offset & 0xFC) | 0x80000000);
}

/* Read 32-bit value from PCI config space */
uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, slot, func, offset));
    return inl(PCI_CONFIG_DATA);
}

/* Read 16-bit value from PCI config space */
uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, slot, func, offset));
    return (uint16_t)(inl(PCI_CONFIG_DATA) >> ((offset & 2) * 8));
}

/* Read 8-bit value from PCI config space */
uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, slot, func, offset));
    return (uint8_t)(inl(PCI_CONFIG_DATA) >> ((offset & 3) * 8));
}

/* Write 32-bit value to PCI config space */
void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, slot, func, offset));
    outl(PCI_CONFIG_DATA, value);
}

/* Write 16-bit value to PCI config space */
void pci_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value) {
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, slot, func, offset));
    uint32_t data = inl(PCI_CONFIG_DATA);
    int shift = (offset & 2) * 8;
    data = (data & ~(0xFFFF << shift)) | ((uint32_t)value << shift);
    outl(PCI_CONFIG_DATA, data);
}

/* Write 8-bit value to PCI config space */
void pci_write8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t value) {
    outl(PCI_CONFIG_ADDRESS, pci_address(bus, slot, func, offset));
    uint32_t data = inl(PCI_CONFIG_DATA);
    int shift = (offset & 3) * 8;
    data = (data & ~(0xFF << shift)) | ((uint32_t)value << shift);
    outl(PCI_CONFIG_DATA, data);
}

/* Check if device exists at given bus/slot/func */
static bool pci_device_exists(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vendor = pci_read16(bus, slot, func, PCI_VENDOR_ID);
    return vendor != 0xFFFF;
}

/* Scan a single device */
static void pci_scan_device(uint8_t bus, uint8_t slot, uint8_t func) {
    if (!pci_device_exists(bus, slot, func)) {
        return;
    }

    if (pci_device_count >= PCI_MAX_DEVICES) {
        return;
    }

    pci_device_t *dev = &pci_devices[pci_device_count];

    dev->bus = bus;
    dev->slot = slot;
    dev->func = func;
    dev->vendor_id = pci_read16(bus, slot, func, PCI_VENDOR_ID);
    dev->device_id = pci_read16(bus, slot, func, PCI_DEVICE_ID);
    dev->class_code = pci_read8(bus, slot, func, PCI_CLASS);
    dev->subclass = pci_read8(bus, slot, func, PCI_SUBCLASS);
    dev->prog_if = pci_read8(bus, slot, func, PCI_PROG_IF);
    dev->revision = pci_read8(bus, slot, func, PCI_REVISION_ID);
    dev->irq = pci_read8(bus, slot, func, PCI_INTERRUPT_LINE);
    dev->present = true;

    /* Read BARs */
    for (int i = 0; i < 6; i++) {
        dev->bar[i] = pci_read32(bus, slot, func, PCI_BAR0 + i * 4);
    }

    kprintf("[PCI] %02x:%02x.%d %04x:%04x class=%02x:%02x irq=%d\n",
            bus, slot, func, dev->vendor_id, dev->device_id,
            dev->class_code, dev->subclass, dev->irq);

    pci_device_count++;
}

/* Scan all PCI buses */
static void pci_scan_bus(uint8_t bus) {
    for (uint8_t slot = 0; slot < 32; slot++) {
        if (!pci_device_exists(bus, slot, 0)) {
            continue;
        }

        pci_scan_device(bus, slot, 0);

        /* Check for multi-function device */
        uint8_t header = pci_read8(bus, slot, 0, PCI_HEADER_TYPE);
        if (header & 0x80) {
            for (uint8_t func = 1; func < 8; func++) {
                pci_scan_device(bus, slot, func);
            }
        }
    }
}

/* Initialize PCI subsystem */
void pci_init(void) {
    kprintf("[PCI] Scanning PCI bus...\n");

    pci_device_count = 0;

    /* Scan all possible buses */
    for (int bus = 0; bus < 256; bus++) {
        pci_scan_bus(bus);
    }

    kprintf("[PCI] Found %d device(s)\n", pci_device_count);
}

/* Find device by vendor/device ID */
pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id &&
            pci_devices[i].device_id == device_id) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

/* Get device BAR address */
uint32_t pci_get_bar(pci_device_t *dev, int bar_num) {
    if (bar_num < 0 || bar_num > 5) {
        return 0;
    }

    uint32_t bar = dev->bar[bar_num];

    if (bar & PCI_BAR_IO) {
        /* I/O BAR */
        return bar & ~0x3;
    } else {
        /* Memory BAR */
        return bar & ~0xF;
    }
}

/* Enable bus mastering for DMA */
void pci_enable_bus_master(pci_device_t *dev) {
    uint16_t cmd = pci_read16(dev->bus, dev->slot, dev->func, PCI_COMMAND);
    cmd |= PCI_COMMAND_MASTER | PCI_COMMAND_IO | PCI_COMMAND_MEMORY;
    pci_write16(dev->bus, dev->slot, dev->func, PCI_COMMAND, cmd);
}

/* Get device count */
int pci_get_device_count(void) {
    return pci_device_count;
}

/* Get device by index */
pci_device_t *pci_get_device(int index) {
    if (index < 0 || index >= pci_device_count) {
        return NULL;
    }
    return &pci_devices[index];
}
