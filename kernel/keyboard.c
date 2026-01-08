/*
 * keyboard.c - PS/2 Keyboard driver implementation
 */

#include "keyboard.h"
#include "pic.h"
#include "serial.h"

#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64

/* Port I/O inline function */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* US keyboard scancode set 1 -> ASCII mapping (lowercase only) */
static const char scancode_to_ascii[128] = {
    0,    27,   '1',  '2',  '3',  '4',  '5',  '6',   /* 0x00-0x07 */
    '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',  /* 0x08-0x0F */
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',   /* 0x10-0x17 */
    'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',   /* 0x18-0x1F */
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',   /* 0x20-0x27 */
    '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',   /* 0x28-0x2F */
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',   /* 0x30-0x37 */
    0,    ' ',  0,    0,    0,    0,    0,    0,     /* 0x38-0x3F */
    0,    0,    0,    0,    0,    0,    0,    '7',   /* 0x40-0x47 */
    '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',   /* 0x48-0x4F */
    '2',  '3',  '0',  '.',  0,    0,    0,    0,     /* 0x50-0x57 */
    0,    0,    0,    0,    0,    0,    0,    0,     /* 0x58-0x5F */
    0,    0,    0,    0,    0,    0,    0,    0,     /* 0x60-0x67 */
    0,    0,    0,    0,    0,    0,    0,    0,     /* 0x68-0x6F */
    0,    0,    0,    0,    0,    0,    0,    0,     /* 0x70-0x77 */
    0,    0,    0,    0,    0,    0,    0,    0      /* 0x78-0x7F */
};

/* Initialize keyboard driver */
void keyboard_init(void) {
    /* Enable keyboard IRQ (IRQ1) */
    pic_clear_mask(IRQ_KEYBOARD);

    serial_print("[Keyboard] Initialized\n");
}

/* Keyboard interrupt handler */
void keyboard_handler(void) {
    uint8_t scancode;
    char c;

    /* Read scancode from keyboard data port */
    scancode = inb(KEYBOARD_DATA_PORT);

    /* Ignore key release events (bit 7 set) */
    if (scancode & 0x80) {
        pic_send_eoi(IRQ_KEYBOARD);
        return;
    }

    /* Convert scancode to ASCII */
    if (scancode < sizeof(scancode_to_ascii)) {
        c = scancode_to_ascii[scancode];
        if (c) {
            serial_putchar(c);
        }
    }

    /* Send EOI to PIC */
    pic_send_eoi(IRQ_KEYBOARD);
}
