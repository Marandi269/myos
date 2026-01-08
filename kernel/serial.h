/*
 * serial.h - Serial port driver interface
 */

#ifndef _SERIAL_H
#define _SERIAL_H

#include "types.h"

/* Serial port base addresses */
#define SERIAL_COM1 0x3F8
#define SERIAL_COM2 0x2F8

/* Initialize serial port */
void serial_init(void);

/* Output a single character */
void serial_putchar(char c);

/* Output a null-terminated string */
void serial_print(const char *str);

/* Output a hexadecimal number */
void serial_print_hex(uint64_t value);

/* Output a decimal number */
void serial_print_dec(uint64_t value);

#endif /* _SERIAL_H */
