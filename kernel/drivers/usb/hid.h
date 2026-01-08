/*
 * hid.h - USB HID (Human Interface Device) driver
 */

#ifndef _USB_HID_H
#define _USB_HID_H

#include "usb.h"

/* HID class-specific requests */
#define HID_REQ_GET_REPORT      0x01
#define HID_REQ_GET_IDLE        0x02
#define HID_REQ_GET_PROTOCOL    0x03
#define HID_REQ_SET_REPORT      0x09
#define HID_REQ_SET_IDLE        0x0A
#define HID_REQ_SET_PROTOCOL    0x0B

/* HID report types */
#define HID_REPORT_INPUT        1
#define HID_REPORT_OUTPUT       2
#define HID_REPORT_FEATURE      3

/* HID protocols */
#define HID_PROTOCOL_BOOT       0
#define HID_PROTOCOL_REPORT     1

/* HID subclass */
#define HID_SUBCLASS_NONE       0
#define HID_SUBCLASS_BOOT       1

/* HID protocol (boot interface) */
#define HID_BOOT_PROTOCOL_NONE      0
#define HID_BOOT_PROTOCOL_KEYBOARD  1
#define HID_BOOT_PROTOCOL_MOUSE     2

/* HID descriptor */
typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdHID;
    uint8_t  bCountryCode;
    uint8_t  bNumDescriptors;
    uint8_t  bReportDescriptorType;
    uint16_t wReportDescriptorLength;
} usb_hid_desc_t;

/* Boot keyboard input report (8 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t modifiers;      /* Modifier keys (ctrl, shift, alt, etc) */
    uint8_t reserved;       /* Reserved byte */
    uint8_t keys[6];        /* Key codes (up to 6 simultaneous keys) */
} hid_keyboard_report_t;

/* Modifier key bits */
#define HID_MOD_LEFT_CTRL   (1 << 0)
#define HID_MOD_LEFT_SHIFT  (1 << 1)
#define HID_MOD_LEFT_ALT    (1 << 2)
#define HID_MOD_LEFT_GUI    (1 << 3)
#define HID_MOD_RIGHT_CTRL  (1 << 4)
#define HID_MOD_RIGHT_SHIFT (1 << 5)
#define HID_MOD_RIGHT_ALT   (1 << 6)
#define HID_MOD_RIGHT_GUI   (1 << 7)

/* HID keyboard device */
typedef struct usb_hid_keyboard {
    usb_device_t *dev;
    usb_interface_t *intf;
    usb_endpoint_t *int_in;     /* Interrupt IN endpoint */
    hid_keyboard_report_t last_report;
    uint8_t leds;               /* LED state (caps, num, scroll lock) */
    uint8_t ep_idx;             /* xHCI endpoint index */
    uint8_t interrupt_enabled;  /* Using interrupt transfers */
} usb_hid_keyboard_t;

/* Initialize USB HID subsystem */
void usb_hid_init(void);

/* HID keyboard functions */
int usb_hid_keyboard_probe(usb_interface_t *intf);
void usb_hid_keyboard_disconnect(usb_interface_t *intf);
int usb_hid_keyboard_poll(usb_hid_keyboard_t *kbd);

/* Get the active HID keyboard */
usb_hid_keyboard_t *usb_hid_get_keyboard(void);

#endif /* _USB_HID_H */
