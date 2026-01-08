/*
 * serial.c - Serial port driver implementation
 */

#include "serial.h"
#include "types.h"

/* Port I/O inline functions */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Check if transmit buffer is empty */
static int serial_is_transmit_empty(void) {
    return inb(SERIAL_COM1 + 5) & 0x20;
}

/* Initialize serial port COM1 */
void serial_init(void) {
    /* Disable all interrupts */
    outb(SERIAL_COM1 + 1, 0x00);

    /* Enable DLAB (set baud rate divisor) */
    outb(SERIAL_COM1 + 3, 0x80);

    /* Set divisor to 1 (115200 baud) */
    outb(SERIAL_COM1 + 0, 0x01);    /* Divisor low byte */
    outb(SERIAL_COM1 + 1, 0x00);    /* Divisor high byte */

    /* 8 bits, no parity, one stop bit */
    outb(SERIAL_COM1 + 3, 0x03);

    /* Enable FIFO, clear them, with 14-byte threshold */
    outb(SERIAL_COM1 + 2, 0xC7);

    /* IRQs enabled, RTS/DSR set */
    outb(SERIAL_COM1 + 4, 0x0B);
}

/* Output a single character to serial port */
void serial_putchar(char c) {
    /* Wait for transmit buffer to be empty */
    while (!serial_is_transmit_empty());
    outb(SERIAL_COM1, c);
}

/* Output a null-terminated string */
void serial_print(const char *str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(*str++);
    }
}

/* Output a hexadecimal number */
void serial_print_hex(uint64_t value) {
    static const char hex[] = "0123456789ABCDEF";
    char buf[19];  /* "0x" + 16 hex digits + null */
    int i;

    buf[0] = '0';
    buf[1] = 'x';

    for (i = 0; i < 16; i++) {
        buf[17 - i] = hex[value & 0xF];
        value >>= 4;
    }
    buf[18] = '\0';

    serial_print(buf);
}

/* Output a decimal number */
void serial_print_dec(uint64_t value) {
    char buf[21];  /* Max uint64 is 20 digits + null */
    int i = 20;

    buf[i] = '\0';

    if (value == 0) {
        serial_putchar('0');
        return;
    }

    while (value > 0) {
        buf[--i] = '0' + (value % 10);
        value /= 10;
    }

    serial_print(&buf[i]);
}
