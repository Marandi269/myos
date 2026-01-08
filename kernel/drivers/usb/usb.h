/*
 * usb.h - USB core definitions
 *
 * Universal Serial Bus core framework.
 */

#ifndef _USB_H
#define _USB_H

#include "types.h"

/* USB speeds */
#define USB_SPEED_LOW       0   /* 1.5 Mbps */
#define USB_SPEED_FULL      1   /* 12 Mbps */
#define USB_SPEED_HIGH      2   /* 480 Mbps */
#define USB_SPEED_SUPER     3   /* 5 Gbps */

/* USB device states */
#define USB_STATE_NOTATTACHED   0
#define USB_STATE_ATTACHED      1
#define USB_STATE_POWERED       2
#define USB_STATE_DEFAULT       3
#define USB_STATE_ADDRESS       4
#define USB_STATE_CONFIGURED    5
#define USB_STATE_SUSPENDED     6

/* USB request types */
#define USB_DIR_OUT             0x00
#define USB_DIR_IN              0x80
#define USB_TYPE_STANDARD       0x00
#define USB_TYPE_CLASS          0x20
#define USB_TYPE_VENDOR         0x40
#define USB_RECIP_DEVICE        0x00
#define USB_RECIP_INTERFACE     0x01
#define USB_RECIP_ENDPOINT      0x02
#define USB_RECIP_OTHER         0x03

/* Standard USB requests */
#define USB_REQ_GET_STATUS          0x00
#define USB_REQ_CLEAR_FEATURE       0x01
#define USB_REQ_SET_FEATURE         0x03
#define USB_REQ_SET_ADDRESS         0x05
#define USB_REQ_GET_DESCRIPTOR      0x06
#define USB_REQ_SET_DESCRIPTOR      0x07
#define USB_REQ_GET_CONFIGURATION   0x08
#define USB_REQ_SET_CONFIGURATION   0x09
#define USB_REQ_GET_INTERFACE       0x0A
#define USB_REQ_SET_INTERFACE       0x0B
#define USB_REQ_SYNCH_FRAME         0x0C

/* Descriptor types */
#define USB_DT_DEVICE           1
#define USB_DT_CONFIG           2
#define USB_DT_STRING           3
#define USB_DT_INTERFACE        4
#define USB_DT_ENDPOINT         5
#define USB_DT_DEVICE_QUALIFIER 6
#define USB_DT_OTHER_SPEED      7
#define USB_DT_INTERFACE_POWER  8
#define USB_DT_HID              0x21
#define USB_DT_HID_REPORT       0x22

/* Endpoint types */
#define USB_ENDPOINT_XFER_CONTROL   0
#define USB_ENDPOINT_XFER_ISOC      1
#define USB_ENDPOINT_XFER_BULK      2
#define USB_ENDPOINT_XFER_INT       3

/* USB class codes */
#define USB_CLASS_PER_INTERFACE     0x00
#define USB_CLASS_AUDIO             0x01
#define USB_CLASS_COMM              0x02
#define USB_CLASS_HID               0x03
#define USB_CLASS_PHYSICAL          0x05
#define USB_CLASS_IMAGE             0x06
#define USB_CLASS_PRINTER           0x07
#define USB_CLASS_MASS_STORAGE      0x08
#define USB_CLASS_HUB               0x09
#define USB_CLASS_CDC_DATA          0x0A
#define USB_CLASS_VENDOR_SPEC       0xFF

/* Maximum values */
#define USB_MAX_DEVICES     127
#define USB_MAX_ENDPOINTS   32
#define USB_MAX_INTERFACES  32

/* USB setup packet (8 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_packet_t;

/* USB device descriptor */
typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} usb_device_desc_t;

/* USB configuration descriptor */
typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} usb_config_desc_t;

/* USB interface descriptor */
typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bInterfaceNumber;
    uint8_t  bAlternateSetting;
    uint8_t  bNumEndpoints;
    uint8_t  bInterfaceClass;
    uint8_t  bInterfaceSubClass;
    uint8_t  bInterfaceProtocol;
    uint8_t  iInterface;
} usb_interface_desc_t;

/* USB endpoint descriptor */
typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} usb_endpoint_desc_t;

/* USB string descriptor */
typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wString[127];
} usb_string_desc_t;

/* Forward declarations */
struct usb_device;
struct usb_driver;
struct usb_hcd;

/* USB endpoint */
typedef struct usb_endpoint {
    uint8_t address;
    uint8_t type;
    uint16_t max_packet;
    uint8_t interval;
    struct usb_device *dev;
} usb_endpoint_t;

/* USB interface */
typedef struct usb_interface {
    uint8_t number;
    uint8_t class;
    uint8_t subclass;
    uint8_t protocol;
    uint8_t num_endpoints;
    usb_endpoint_t endpoints[USB_MAX_ENDPOINTS];
    struct usb_driver *driver;
    struct usb_device *dev;
} usb_interface_t;

/* USB device */
typedef struct usb_device {
    uint8_t address;
    uint8_t speed;
    uint8_t state;
    uint8_t slot_id;        /* xHCI slot ID */
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t class;
    uint8_t subclass;
    uint8_t protocol;
    char manufacturer[64];
    char product[64];
    char serial[64];
    uint8_t num_interfaces;
    usb_interface_t interfaces[USB_MAX_INTERFACES];
    usb_endpoint_t ep0;     /* Control endpoint */
    struct usb_hcd *hcd;
    void *hcd_priv;         /* HCD-specific data */
    struct usb_device *parent;  /* Hub this device is connected to */
    uint8_t port;           /* Port number on parent hub */
} usb_device_t;

/* USB Host Controller Driver */
typedef struct usb_hcd {
    const char *name;
    void *priv;             /* Controller-specific data */

    /* Operations */
    int (*start)(struct usb_hcd *hcd);
    void (*stop)(struct usb_hcd *hcd);
    int (*reset_port)(struct usb_hcd *hcd, int port);
    int (*control_transfer)(struct usb_hcd *hcd, usb_device_t *dev,
                            usb_setup_packet_t *setup, void *data);
    int (*bulk_transfer)(struct usb_hcd *hcd, usb_device_t *dev,
                         usb_endpoint_t *ep, void *data, int len);
    int (*int_transfer)(struct usb_hcd *hcd, usb_device_t *dev,
                        usb_endpoint_t *ep, void *data, int len);
} usb_hcd_t;

/* USB device driver */
typedef struct usb_driver {
    const char *name;
    int (*probe)(usb_interface_t *intf);
    void (*disconnect)(usb_interface_t *intf);
    struct usb_driver *next;
} usb_driver_t;

/* Initialize USB subsystem */
void usb_init(void);

/* Register/unregister HCD */
int usb_register_hcd(usb_hcd_t *hcd);
void usb_unregister_hcd(usb_hcd_t *hcd);

/* Register/unregister device driver */
int usb_register_driver(usb_driver_t *driver);
void usb_unregister_driver(usb_driver_t *driver);

/* Device enumeration */
usb_device_t *usb_alloc_device(usb_hcd_t *hcd);
void usb_free_device(usb_device_t *dev);
int usb_enumerate_device(usb_device_t *dev);

/* Control transfers */
int usb_control_msg(usb_device_t *dev, uint8_t request_type, uint8_t request,
                    uint16_t value, uint16_t index, void *data, uint16_t size);
int usb_get_descriptor(usb_device_t *dev, uint8_t type, uint8_t index,
                       void *buf, int size);
int usb_set_address(usb_device_t *dev, uint8_t address);
int usb_set_configuration(usb_device_t *dev, uint8_t config);

/* Bulk/interrupt transfers */
int usb_bulk_msg(usb_device_t *dev, usb_endpoint_t *ep,
                 void *data, int len, int *actual);
int usb_interrupt_msg(usb_device_t *dev, usb_endpoint_t *ep,
                      void *data, int len, int *actual);

/* Get string descriptor */
int usb_get_string(usb_device_t *dev, uint8_t index, char *buf, int size);

/* Debug */
void usb_print_device(usb_device_t *dev);

#endif /* _USB_H */
