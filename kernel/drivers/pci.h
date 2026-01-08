/*
 * pci.h - PCI Bus Driver
 */

#ifndef _PCI_H
#define _PCI_H

#include "types.h"

/* PCI Configuration Space Registers */
#define PCI_VENDOR_ID           0x00
#define PCI_DEVICE_ID           0x02
#define PCI_COMMAND             0x04
#define PCI_STATUS              0x06
#define PCI_REVISION_ID         0x08
#define PCI_PROG_IF             0x09
#define PCI_SUBCLASS            0x0A
#define PCI_CLASS               0x0B
#define PCI_CACHE_LINE_SIZE     0x0C
#define PCI_LATENCY_TIMER       0x0D
#define PCI_HEADER_TYPE         0x0E
#define PCI_BIST                0x0F
#define PCI_BAR0                0x10
#define PCI_BAR1                0x14
#define PCI_BAR2                0x18
#define PCI_BAR3                0x1C
#define PCI_BAR4                0x20
#define PCI_BAR5                0x24
#define PCI_INTERRUPT_LINE      0x3C
#define PCI_INTERRUPT_PIN       0x3D

/* PCI Command Register Bits */
#define PCI_COMMAND_IO          (1 << 0)
#define PCI_COMMAND_MEMORY      (1 << 1)
#define PCI_COMMAND_MASTER      (1 << 2)
#define PCI_COMMAND_INTX_DISABLE (1 << 10)

/* PCI BAR Types */
#define PCI_BAR_IO              0x01
#define PCI_BAR_MEM             0x00
#define PCI_BAR_MEM_PREFETCH    0x08

/* PCI I/O Ports */
#define PCI_CONFIG_ADDRESS      0xCF8
#define PCI_CONFIG_DATA         0xCFC

/* Known vendor/device IDs */
#define PCI_VENDOR_REDHAT       0x1AF4
#define PCI_DEVICE_VIRTIO_NET   0x1000
#define PCI_DEVICE_VIRTIO_BLK   0x1001
#define PCI_DEVICE_VIRTIO_NET2  0x1041  /* Modern virtio-net */

/* Maximum PCI devices to track */
#define PCI_MAX_DEVICES         32

/* PCI Device structure */
typedef struct pci_device {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
    uint8_t irq;
    uint32_t bar[6];
    bool present;
} pci_device_t;

/* Initialize PCI subsystem */
void pci_init(void);

/* Read/Write PCI configuration space */
uint8_t pci_read8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_write8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint8_t value);
void pci_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value);
void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value);

/* Find device by vendor/device ID */
pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id);

/* Get device BAR address */
uint32_t pci_get_bar(pci_device_t *dev, int bar_num);

/* Enable bus mastering for DMA */
void pci_enable_bus_master(pci_device_t *dev);

/* Get device count */
int pci_get_device_count(void);

/* Get device by index */
pci_device_t *pci_get_device(int index);

#endif /* _PCI_H */
