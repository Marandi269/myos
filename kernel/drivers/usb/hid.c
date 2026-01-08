/*
 * hid.c - USB HID (Human Interface Device) driver
 */

#include "hid.h"
#include "usb.h"
#include "xhci.h"
#include "../../lib/kprintf.h"
#include "../../lib/string.h"
#include "../../mm/heap.h"

/* Active HID keyboard */
static usb_hid_keyboard_t *active_keyboard = NULL;

/* HID USB driver */
static usb_driver_t hid_driver;

/* HID keycode to ASCII mapping (US layout) */
static const char hid_keycode_to_ascii[128] = {
    0,    0,    0,    0,   'a',  'b',  'c',  'd',    /* 0x00-0x07 */
    'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',    /* 0x08-0x0F */
    'm',  'n',  'o',  'p',  'q',  'r',  's',  't',    /* 0x10-0x17 */
    'u',  'v',  'w',  'x',  'y',  'z',  '1',  '2',    /* 0x18-0x1F */
    '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',    /* 0x20-0x27 */
    '\n', 0x1B, '\b', '\t', ' ',  '-',  '=',  '[',    /* 0x28-0x2F (Enter, Esc, BS, Tab, Space) */
    ']',  '\\', '#',  ';', '\'',  '`',  ',',  '.',    /* 0x30-0x37 */
    '/',   0,    0,    0,    0,    0,    0,    0,     /* 0x38-0x3F (CapsLock, F1-F6) */
    0,    0,    0,    0,    0,    0,    0,    0,      /* 0x40-0x47 (F7-F12, PrtScn, ScrollLock) */
    0,    0,    0,    0,   0x7F,  0,    0,    0,      /* 0x48-0x4F (Pause, Insert, Home, PgUp, Del) */
    0,    0,    0,    0,    0,    0,    0,    0,      /* 0x50-0x57 (End, PgDn, Right, Left, Down, Up) */
    0,    0,    0,    0,    '/',  '*',  '-',  '+',    /* 0x58-0x5F (NumLock, KP/) */
    '\n', '1',  '2',  '3',  '4',  '5',  '6',  '7',    /* 0x60-0x67 (KP Enter, KP 1-7) */
    '8',  '9',  '0',  '.',  0,    0,    0,    0,      /* 0x68-0x6F */
    0,    0,    0,    0,    0,    0,    0,    0,      /* 0x70-0x77 */
    0,    0,    0,    0,    0,    0,    0,    0       /* 0x78-0x7F */
};

/* Shifted keycode mapping */
static const char hid_keycode_to_ascii_shift[128] = {
    0,    0,    0,    0,   'A',  'B',  'C',  'D',    /* 0x00-0x07 */
    'E',  'F',  'G',  'H',  'I',  'J',  'K',  'L',    /* 0x08-0x0F */
    'M',  'N',  'O',  'P',  'Q',  'R',  'S',  'T',    /* 0x10-0x17 */
    'U',  'V',  'W',  'X',  'Y',  'Z',  '!',  '@',    /* 0x18-0x1F */
    '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')',    /* 0x20-0x27 */
    '\n', 0x1B, '\b', '\t', ' ',  '_',  '+',  '{',    /* 0x28-0x2F */
    '}',  '|',  '~',  ':',  '"',  '~',  '<',  '>',    /* 0x30-0x37 */
    '?',   0,    0,    0,    0,    0,    0,    0,     /* 0x38-0x3F */
    0,    0,    0,    0,    0,    0,    0,    0,      /* 0x40-0x47 */
    0,    0,    0,    0,   0x7F,  0,    0,    0,      /* 0x48-0x4F */
    0,    0,    0,    0,    0,    0,    0,    0,      /* 0x50-0x57 */
    0,    0,    0,    0,    '/',  '*',  '-',  '+',    /* 0x58-0x5F */
    '\n', '1',  '2',  '3',  '4',  '5',  '6',  '7',    /* 0x60-0x67 */
    '8',  '9',  '0',  '.',  0,    0,    0,    0,      /* 0x68-0x6F */
    0,    0,    0,    0,    0,    0,    0,    0,      /* 0x70-0x77 */
    0,    0,    0,    0,    0,    0,    0,    0       /* 0x78-0x7F */
};

/* Set boot protocol */
static int hid_set_protocol(usb_device_t *dev, uint16_t interface, uint8_t protocol) {
    return usb_control_msg(dev,
                           USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                           HID_REQ_SET_PROTOCOL,
                           protocol,
                           interface,
                           NULL, 0);
}

/* Set idle rate */
static int hid_set_idle(usb_device_t *dev, uint16_t interface, uint8_t duration, uint8_t report_id) {
    return usb_control_msg(dev,
                           USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                           HID_REQ_SET_IDLE,
                           (duration << 8) | report_id,
                           interface,
                           NULL, 0);
}

/* Get report (boot protocol) */
static int hid_get_report(usb_device_t *dev, uint16_t interface,
                          uint8_t type, uint8_t id, void *buf, uint16_t len) {
    return usb_control_msg(dev,
                           USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                           HID_REQ_GET_REPORT,
                           (type << 8) | id,
                           interface,
                           buf, len);
}

/* Keyboard input buffer */
#define KBD_BUFFER_SIZE 64
static char kbd_buffer[KBD_BUFFER_SIZE];
static int kbd_buffer_head = 0;
static int kbd_buffer_tail = 0;

/* Add character to keyboard buffer */
static void kbd_buffer_put(char c) {
    int next = (kbd_buffer_head + 1) % KBD_BUFFER_SIZE;
    if (next != kbd_buffer_tail) {
        kbd_buffer[kbd_buffer_head] = c;
        kbd_buffer_head = next;
    }
}

/* Get character from keyboard buffer */
int usb_kbd_getchar(void) {
    if (kbd_buffer_head == kbd_buffer_tail) {
        return -1;  /* Buffer empty */
    }
    char c = kbd_buffer[kbd_buffer_tail];
    kbd_buffer_tail = (kbd_buffer_tail + 1) % KBD_BUFFER_SIZE;
    return (unsigned char)c;
}

/* Check if keyboard buffer has data */
int usb_kbd_has_data(void) {
    return kbd_buffer_head != kbd_buffer_tail;
}

/* Process keyboard report */
static void hid_keyboard_process_report(usb_hid_keyboard_t *kbd, hid_keyboard_report_t *report) {
    int shift = (report->modifiers & (HID_MOD_LEFT_SHIFT | HID_MOD_RIGHT_SHIFT)) ? 1 : 0;

    /* Process each key in the report */
    for (int i = 0; i < 6; i++) {
        uint8_t keycode = report->keys[i];
        if (keycode == 0) continue;

        /* Check if key was already pressed in last report */
        int was_pressed = 0;
        for (int j = 0; j < 6; j++) {
            if (kbd->last_report.keys[j] == keycode) {
                was_pressed = 1;
                break;
            }
        }

        /* Only process new key presses */
        if (!was_pressed && keycode < 128) {
            char c;
            if (shift) {
                c = hid_keycode_to_ascii_shift[keycode];
            } else {
                c = hid_keycode_to_ascii[keycode];
            }

            if (c != 0) {
                kprintf("%c", c);
                kbd_buffer_put(c);
            }
        }
    }

    /* Save current report for next comparison */
    memcpy(&kbd->last_report, report, sizeof(hid_keyboard_report_t));
}

/* Poll keyboard for new data */
int usb_hid_keyboard_poll(usb_hid_keyboard_t *kbd) {
    hid_keyboard_report_t report;

    if (!kbd || !kbd->dev) {
        return -1;
    }

    /* Use interrupt transfer if available */
    if (kbd->interrupt_enabled) {
        int ret = xhci_poll_interrupt(kbd->dev->slot_id, kbd->ep_idx,
                                       &report, sizeof(report));
        if (ret > 0) {
            /* Check if report changed */
            if (memcmp(&report, &kbd->last_report, sizeof(report)) != 0) {
                hid_keyboard_process_report(kbd, &report);
            }
            return 0;
        }
        return ret;
    }

    /* Fallback to control transfer (boot protocol) */
    int ret = hid_get_report(kbd->dev, kbd->intf->number,
                              HID_REPORT_INPUT, 0,
                              &report, sizeof(report));

    if (ret >= (int)sizeof(report)) {
        /* Check if report changed */
        if (memcmp(&report, &kbd->last_report, sizeof(report)) != 0) {
            hid_keyboard_process_report(kbd, &report);
        }
        return 0;
    }

    return ret;
}

/* Find interrupt IN endpoint */
static usb_endpoint_t *find_int_in_endpoint(usb_interface_t *intf) {
    for (int i = 0; i < intf->num_endpoints; i++) {
        usb_endpoint_t *ep = &intf->endpoints[i];
        if ((ep->address & 0x80) && ep->type == USB_ENDPOINT_XFER_INT) {
            return ep;
        }
    }
    return NULL;
}

/* Probe HID keyboard */
int usb_hid_keyboard_probe(usb_interface_t *intf) {
    usb_device_t *dev = intf->dev;

    /* Check for HID boot keyboard */
    if (intf->class != USB_CLASS_HID ||
        intf->subclass != HID_SUBCLASS_BOOT ||
        intf->protocol != HID_BOOT_PROTOCOL_KEYBOARD) {
        return -1;
    }

    kprintf("[HID] Found boot keyboard on interface %d\n", intf->number);

    /* Allocate keyboard structure */
    usb_hid_keyboard_t *kbd = kmalloc(sizeof(usb_hid_keyboard_t));
    if (!kbd) {
        return -1;
    }

    memset(kbd, 0, sizeof(usb_hid_keyboard_t));
    kbd->dev = dev;
    kbd->intf = intf;

    /* Find interrupt IN endpoint */
    kbd->int_in = find_int_in_endpoint(intf);
    if (!kbd->int_in) {
        kprintf("[HID] No interrupt IN endpoint found\n");
        /* Still usable via control transfers */
    }

    /* Set boot protocol */
    hid_set_protocol(dev, intf->number, HID_PROTOCOL_BOOT);

    /* Set idle rate to 0 (infinite) */
    hid_set_idle(dev, intf->number, 0, 0);

    /* Setup interrupt endpoint if available */
    kbd->interrupt_enabled = 0;
    if (kbd->int_in) {
        int ep_idx = xhci_setup_interrupt_ep(dev->slot_id,
                                              kbd->int_in->address,
                                              kbd->int_in->max_packet,
                                              kbd->int_in->interval);
        if (ep_idx > 0) {
            kbd->ep_idx = ep_idx;
            /* Start first interrupt transfer */
            if (xhci_start_interrupt_transfer(dev->slot_id, ep_idx, 8) == 0) {
                kbd->interrupt_enabled = 1;
                kprintf("[HID] Interrupt transfers enabled\n");
            }
        }
    }

    /* Store as active keyboard */
    active_keyboard = kbd;

    kprintf("[HID] USB keyboard ready\n");
    return 0;
}

/* Disconnect HID keyboard */
void usb_hid_keyboard_disconnect(usb_interface_t *intf) {
    (void)intf;
    if (active_keyboard) {
        kfree(active_keyboard);
        active_keyboard = NULL;
    }
}

/* HID driver probe */
static int hid_probe(usb_interface_t *intf) {
    /* Check for HID class */
    if (intf->class != USB_CLASS_HID) {
        return -1;
    }

    /* Try keyboard */
    if (intf->subclass == HID_SUBCLASS_BOOT &&
        intf->protocol == HID_BOOT_PROTOCOL_KEYBOARD) {
        return usb_hid_keyboard_probe(intf);
    }

    /* TODO: Add mouse support */

    return -1;
}

/* HID driver disconnect */
static void hid_disconnect(usb_interface_t *intf) {
    if (intf->protocol == HID_BOOT_PROTOCOL_KEYBOARD) {
        usb_hid_keyboard_disconnect(intf);
    }
}

/* Get the active HID keyboard */
usb_hid_keyboard_t *usb_hid_get_keyboard(void) {
    return active_keyboard;
}

/* Initialize USB HID subsystem */
void usb_hid_init(void) {
    hid_driver.name = "USB HID";
    hid_driver.probe = hid_probe;
    hid_driver.disconnect = hid_disconnect;
    hid_driver.next = NULL;

    usb_register_driver(&hid_driver);

    kprintf("[HID] USB HID driver registered\n");
}
