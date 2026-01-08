/*
 * usb.c - USB core implementation
 */

#include "usb.h"
#include "xhci.h"
#include "hid.h"
#include "../pci.h"
#include "../../lib/kprintf.h"
#include "../../lib/string.h"
#include "../../mm/heap.h"

/* PCI class codes for USB controllers */
#define PCI_CLASS_SERIAL_BUS    0x0C
#define PCI_SUBCLASS_USB        0x03
#define PCI_PROGIF_UHCI         0x00
#define PCI_PROGIF_OHCI         0x10
#define PCI_PROGIF_EHCI         0x20
#define PCI_PROGIF_XHCI         0x30

/* Registered HCDs */
static usb_hcd_t *hcd_list = NULL;

/* Registered device drivers */
static usb_driver_t *driver_list = NULL;

/* Next available device address */
static uint8_t next_address = 1;

/* Device list */
static usb_device_t *devices[USB_MAX_DEVICES];

/* Probe PCI bus for USB controllers */
static void usb_probe_pci(void) {
    int count = pci_get_device_count();

    for (int i = 0; i < count; i++) {
        pci_device_t *dev = pci_get_device(i);
        if (!dev || !dev->present) continue;

        /* Check for USB controller class */
        if (dev->class_code != PCI_CLASS_SERIAL_BUS ||
            dev->subclass != PCI_SUBCLASS_USB) {
            continue;
        }

        kprintf("[USB] Found USB controller: %02x:%02x.%x prog_if=0x%02x\n",
                dev->bus, dev->slot, dev->func, dev->prog_if);

        switch (dev->prog_if) {
        case PCI_PROGIF_XHCI: {
            /* xHCI (USB 3.x) controller */
            kprintf("[USB] Detected xHCI controller\n");

            /* Get BAR0 (MMIO base address) */
            uint64_t bar0 = dev->bar[0] & ~0xF;

            /* Check for 64-bit BAR */
            if ((dev->bar[0] & 0x6) == 0x4) {
                bar0 |= ((uint64_t)dev->bar[1]) << 32;
            }

            if (bar0 == 0) {
                kprintf("[USB] xHCI BAR0 is 0, skipping\n");
                break;
            }

            /* Enable bus mastering and memory space */
            pci_enable_bus_master(dev);
            uint16_t cmd = pci_read16(dev->bus, dev->slot, dev->func, PCI_COMMAND);
            cmd |= PCI_COMMAND_MEMORY;
            pci_write16(dev->bus, dev->slot, dev->func, PCI_COMMAND, cmd);

            kprintf("[USB] xHCI MMIO base: 0x%lx, IRQ: %d\n", bar0, dev->irq);

            /* Initialize xHCI controller */
            xhci_init(bar0);
            break;
        }
        case PCI_PROGIF_EHCI:
            kprintf("[USB] EHCI controller (not supported)\n");
            break;
        case PCI_PROGIF_OHCI:
            kprintf("[USB] OHCI controller (not supported)\n");
            break;
        case PCI_PROGIF_UHCI:
            kprintf("[USB] UHCI controller (not supported)\n");
            break;
        default:
            kprintf("[USB] Unknown USB controller type: 0x%02x\n", dev->prog_if);
            break;
        }
    }
}

/* Initialize USB subsystem */
void usb_init(void) {
    memset(devices, 0, sizeof(devices));
    next_address = 1;
    kprintf("[USB] Core initialized\n");

    /* Register HID driver */
    usb_hid_init();

    /* Probe PCI for USB controllers */
    usb_probe_pci();
}

/* Register HCD */
int usb_register_hcd(usb_hcd_t *hcd) {
    if (!hcd) return -1;

    kprintf("[USB] Registering HCD: %s\n", hcd->name);

    /* Start the controller */
    if (hcd->start) {
        int ret = hcd->start(hcd);
        if (ret < 0) {
            kprintf("[USB] Failed to start HCD: %s\n", hcd->name);
            return ret;
        }
    }

    /* Add to list (simple linked list would be better) */
    hcd_list = hcd;

    return 0;
}

/* Unregister HCD */
void usb_unregister_hcd(usb_hcd_t *hcd) {
    if (!hcd) return;

    if (hcd->stop) {
        hcd->stop(hcd);
    }

    if (hcd_list == hcd) {
        hcd_list = NULL;
    }
}

/* Register device driver */
int usb_register_driver(usb_driver_t *driver) {
    if (!driver) return -1;

    kprintf("[USB] Registering driver: %s\n", driver->name);

    driver->next = driver_list;
    driver_list = driver;

    return 0;
}

/* Unregister device driver */
void usb_unregister_driver(usb_driver_t *driver) {
    usb_driver_t **pp;

    if (!driver) return;

    for (pp = &driver_list; *pp; pp = &(*pp)->next) {
        if (*pp == driver) {
            *pp = driver->next;
            break;
        }
    }
}

/* Allocate USB device */
usb_device_t *usb_alloc_device(usb_hcd_t *hcd) {
    usb_device_t *dev;

    dev = kmalloc(sizeof(usb_device_t));
    if (!dev) return NULL;

    memset(dev, 0, sizeof(usb_device_t));
    dev->hcd = hcd;
    dev->state = USB_STATE_ATTACHED;
    dev->address = 0;

    /* Setup default control endpoint */
    dev->ep0.address = 0;
    dev->ep0.type = USB_ENDPOINT_XFER_CONTROL;
    dev->ep0.max_packet = 8;  /* Default, will be updated */
    dev->ep0.dev = dev;

    return dev;
}

/* Free USB device */
void usb_free_device(usb_device_t *dev) {
    if (!dev) return;

    /* Disconnect drivers */
    for (int i = 0; i < dev->num_interfaces; i++) {
        if (dev->interfaces[i].driver && dev->interfaces[i].driver->disconnect) {
            dev->interfaces[i].driver->disconnect(&dev->interfaces[i]);
        }
    }

    /* Remove from device list */
    if (dev->address > 0 && dev->address < USB_MAX_DEVICES) {
        devices[dev->address] = NULL;
    }

    kfree(dev);
}

/* Control transfer */
int usb_control_msg(usb_device_t *dev, uint8_t request_type, uint8_t request,
                    uint16_t value, uint16_t index, void *data, uint16_t size) {
    usb_setup_packet_t setup;

    if (!dev || !dev->hcd || !dev->hcd->control_transfer) {
        return -1;
    }

    setup.bmRequestType = request_type;
    setup.bRequest = request;
    setup.wValue = value;
    setup.wIndex = index;
    setup.wLength = size;

    return dev->hcd->control_transfer(dev->hcd, dev, &setup, data);
}

/* Get descriptor */
int usb_get_descriptor(usb_device_t *dev, uint8_t type, uint8_t index,
                       void *buf, int size) {
    return usb_control_msg(dev,
                           USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                           USB_REQ_GET_DESCRIPTOR,
                           (type << 8) | index,
                           0,
                           buf, size);
}

/* Set device address */
int usb_set_address(usb_device_t *dev, uint8_t address) {
    int ret;

    ret = usb_control_msg(dev,
                          USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                          USB_REQ_SET_ADDRESS,
                          address,
                          0,
                          NULL, 0);

    if (ret >= 0) {
        dev->address = address;
        dev->state = USB_STATE_ADDRESS;
    }

    return ret;
}

/* Set configuration */
int usb_set_configuration(usb_device_t *dev, uint8_t config) {
    int ret;

    ret = usb_control_msg(dev,
                          USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                          USB_REQ_SET_CONFIGURATION,
                          config,
                          0,
                          NULL, 0);

    if (ret >= 0) {
        dev->state = USB_STATE_CONFIGURED;
    }

    return ret;
}

/* Get string descriptor */
int usb_get_string(usb_device_t *dev, uint8_t index, char *buf, int size) {
    usb_string_desc_t desc;
    int ret, i;

    if (!buf || size < 1) return -1;
    buf[0] = '\0';

    if (index == 0) return 0;

    /* Get string descriptor (language 0x0409 = English) */
    ret = usb_control_msg(dev,
                          USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_DEVICE,
                          USB_REQ_GET_DESCRIPTOR,
                          (USB_DT_STRING << 8) | index,
                          0x0409,
                          &desc, sizeof(desc));

    if (ret < 0 || desc.bLength < 2) {
        return ret;
    }

    /* Convert UTF-16LE to ASCII */
    int chars = (desc.bLength - 2) / 2;
    if (chars > size - 1) chars = size - 1;

    for (i = 0; i < chars; i++) {
        buf[i] = (char)(desc.wString[i] & 0xFF);
    }
    buf[i] = '\0';

    return i;
}

/* Bulk transfer */
int usb_bulk_msg(usb_device_t *dev, usb_endpoint_t *ep,
                 void *data, int len, int *actual) {
    int ret;

    if (!dev || !dev->hcd || !dev->hcd->bulk_transfer || !ep) {
        return -1;
    }

    ret = dev->hcd->bulk_transfer(dev->hcd, dev, ep, data, len);

    if (actual) {
        *actual = (ret >= 0) ? ret : 0;
    }

    return ret;
}

/* Interrupt transfer */
int usb_interrupt_msg(usb_device_t *dev, usb_endpoint_t *ep,
                      void *data, int len, int *actual) {
    int ret;

    if (!dev || !dev->hcd || !dev->hcd->int_transfer || !ep) {
        return -1;
    }

    ret = dev->hcd->int_transfer(dev->hcd, dev, ep, data, len);

    if (actual) {
        *actual = (ret >= 0) ? ret : 0;
    }

    return ret;
}

/* Find matching driver for interface */
static usb_driver_t *find_driver(usb_interface_t *intf) {
    usb_driver_t *drv;

    for (drv = driver_list; drv; drv = drv->next) {
        if (drv->probe && drv->probe(intf) == 0) {
            return drv;
        }
    }

    return NULL;
}

/* Parse configuration descriptor */
static int parse_config(usb_device_t *dev, uint8_t *data, int len) {
    usb_config_desc_t *config = (usb_config_desc_t *)data;
    uint8_t *ptr = data + config->bLength;
    uint8_t *end = data + len;
    usb_interface_t *intf = NULL;
    int ep_idx = 0;

    while (ptr < end) {
        uint8_t desc_len = ptr[0];
        uint8_t desc_type = ptr[1];

        if (desc_len < 2) break;

        switch (desc_type) {
        case USB_DT_INTERFACE: {
            usb_interface_desc_t *idesc = (usb_interface_desc_t *)ptr;
            if (dev->num_interfaces < USB_MAX_INTERFACES) {
                intf = &dev->interfaces[dev->num_interfaces++];
                memset(intf, 0, sizeof(usb_interface_t));
                intf->number = idesc->bInterfaceNumber;
                intf->class = idesc->bInterfaceClass;
                intf->subclass = idesc->bInterfaceSubClass;
                intf->protocol = idesc->bInterfaceProtocol;
                intf->dev = dev;
                ep_idx = 0;
            }
            break;
        }
        case USB_DT_ENDPOINT: {
            usb_endpoint_desc_t *edesc = (usb_endpoint_desc_t *)ptr;
            if (intf && ep_idx < USB_MAX_ENDPOINTS) {
                usb_endpoint_t *ep = &intf->endpoints[ep_idx++];
                ep->address = edesc->bEndpointAddress;
                ep->type = edesc->bmAttributes & 0x03;
                ep->max_packet = edesc->wMaxPacketSize;
                ep->interval = edesc->bInterval;
                ep->dev = dev;
                intf->num_endpoints = ep_idx;
            }
            break;
        }
        }

        ptr += desc_len;
    }

    return 0;
}

/* Enumerate USB device */
int usb_enumerate_device(usb_device_t *dev) {
    usb_device_desc_t dev_desc;
    uint8_t config_buf[256];
    usb_config_desc_t *config;
    int ret;

    kprintf("[USB] Enumerating device...\n");

    /* Get device descriptor (first 8 bytes to get max packet size) */
    ret = usb_get_descriptor(dev, USB_DT_DEVICE, 0, &dev_desc, 8);
    if (ret < 0) {
        kprintf("[USB] Failed to get device descriptor\n");
        return ret;
    }

    /* Update EP0 max packet size */
    dev->ep0.max_packet = dev_desc.bMaxPacketSize0;

    /* Set address */
    uint8_t addr = next_address++;
    if (next_address >= USB_MAX_DEVICES) {
        next_address = 1;
    }

    ret = usb_set_address(dev, addr);
    if (ret < 0) {
        kprintf("[USB] Failed to set address\n");
        return ret;
    }

    kprintf("[USB] Device address: %d\n", addr);
    devices[addr] = dev;

    /* Get full device descriptor */
    ret = usb_get_descriptor(dev, USB_DT_DEVICE, 0, &dev_desc, sizeof(dev_desc));
    if (ret < 0) {
        return ret;
    }

    dev->vendor_id = dev_desc.idVendor;
    dev->product_id = dev_desc.idProduct;
    dev->class = dev_desc.bDeviceClass;
    dev->subclass = dev_desc.bDeviceSubClass;
    dev->protocol = dev_desc.bDeviceProtocol;

    /* Get string descriptors */
    if (dev_desc.iManufacturer) {
        usb_get_string(dev, dev_desc.iManufacturer, dev->manufacturer,
                       sizeof(dev->manufacturer));
    }
    if (dev_desc.iProduct) {
        usb_get_string(dev, dev_desc.iProduct, dev->product, sizeof(dev->product));
    }
    if (dev_desc.iSerialNumber) {
        usb_get_string(dev, dev_desc.iSerialNumber, dev->serial, sizeof(dev->serial));
    }

    kprintf("[USB] VID=%04x PID=%04x %s %s\n",
            dev->vendor_id, dev->product_id,
            dev->manufacturer, dev->product);

    /* Get configuration descriptor */
    ret = usb_get_descriptor(dev, USB_DT_CONFIG, 0, config_buf, sizeof(config_buf));
    if (ret < 0) {
        return ret;
    }

    config = (usb_config_desc_t *)config_buf;

    /* Parse configuration */
    parse_config(dev, config_buf, config->wTotalLength);

    /* Set configuration */
    ret = usb_set_configuration(dev, config->bConfigurationValue);
    if (ret < 0) {
        return ret;
    }

    /* Find drivers for interfaces */
    for (int i = 0; i < dev->num_interfaces; i++) {
        usb_interface_t *intf = &dev->interfaces[i];
        usb_driver_t *drv = find_driver(intf);

        if (drv) {
            intf->driver = drv;
            kprintf("[USB] Interface %d: %s\n", i, drv->name);
        } else {
            kprintf("[USB] Interface %d: class=%02x/%02x/%02x (no driver)\n",
                    i, intf->class, intf->subclass, intf->protocol);
        }
    }

    return 0;
}

/* Print device info */
void usb_print_device(usb_device_t *dev) {
    if (!dev) return;

    kprintf("USB Device:\n");
    kprintf("  Address: %d\n", dev->address);
    kprintf("  VID:PID: %04x:%04x\n", dev->vendor_id, dev->product_id);
    kprintf("  Manufacturer: %s\n", dev->manufacturer);
    kprintf("  Product: %s\n", dev->product);
    kprintf("  Serial: %s\n", dev->serial);
    kprintf("  Class: %02x/%02x/%02x\n", dev->class, dev->subclass, dev->protocol);
    kprintf("  Interfaces: %d\n", dev->num_interfaces);

    for (int i = 0; i < dev->num_interfaces; i++) {
        usb_interface_t *intf = &dev->interfaces[i];
        kprintf("    Interface %d: class=%02x/%02x/%02x, %d endpoints\n",
                intf->number, intf->class, intf->subclass, intf->protocol,
                intf->num_endpoints);
    }
}
