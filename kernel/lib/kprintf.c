/*
 * kprintf.c - Kernel printf implementation
 */

#include "kprintf.h"
#include "serial.h"
#include "string.h"

/* Variable argument handling */
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap)         __builtin_va_end(ap)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)

/* Print unsigned integer in given base */
static void print_num(uint64_t num, int base, int width, char pad, int uppercase) {
    char buf[65];
    char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;
    int len;

    /* Handle zero */
    if (num == 0) {
        buf[i++] = '0';
    } else {
        while (num > 0) {
            buf[i++] = digits[num % base];
            num /= base;
        }
    }

    /* Pad if necessary */
    len = i;
    while (len < width) {
        serial_putchar(pad);
        len++;
    }

    /* Print in reverse order */
    while (i > 0) {
        serial_putchar(buf[--i]);
    }
}

/* Print signed integer */
static void print_signed(int64_t num, int width, char pad) {
    if (num < 0) {
        serial_putchar('-');
        if (width > 0) width--;
        num = -num;
    }
    print_num((uint64_t)num, 10, width, pad, 0);
}

/* Kernel printf */
int kprintf(const char *fmt, ...) {
    va_list ap;
    int count = 0;
    char c;
    const char *s;
    int64_t d;
    uint64_t u;
    void *p;
    int width;
    char pad;

    va_start(ap, fmt);

    while ((c = *fmt++) != '\0') {
        if (c != '%') {
            if (c == '\n') {
                serial_putchar('\r');
            }
            serial_putchar(c);
            count++;
            continue;
        }

        /* Parse format specifier */
        width = 0;
        pad = ' ';

        c = *fmt++;
        if (c == '\0') break;

        /* Check for zero padding */
        if (c == '0') {
            pad = '0';
            c = *fmt++;
            if (c == '\0') break;
        }

        /* Parse width */
        while (c >= '0' && c <= '9') {
            width = width * 10 + (c - '0');
            c = *fmt++;
            if (c == '\0') break;
        }

        /* Handle 'l' and 'll' modifiers */
        if (c == 'l') {
            c = *fmt++;
            if (c == 'l') {
                c = *fmt++;
            }
        }

        switch (c) {
            case 'd':
            case 'i':
                d = va_arg(ap, int64_t);
                print_signed(d, width, pad);
                break;

            case 'u':
                u = va_arg(ap, uint64_t);
                print_num(u, 10, width, pad, 0);
                break;

            case 'x':
                u = va_arg(ap, uint64_t);
                print_num(u, 16, width, pad, 0);
                break;

            case 'X':
                u = va_arg(ap, uint64_t);
                print_num(u, 16, width, pad, 1);
                break;

            case 'p':
                p = va_arg(ap, void *);
                serial_print("0x");
                print_num((uint64_t)p, 16, 16, '0', 0);
                break;

            case 's':
                s = va_arg(ap, const char *);
                if (s == NULL) {
                    s = "(null)";
                }
                while (*s) {
                    serial_putchar(*s++);
                    count++;
                }
                break;

            case 'c':
                c = (char)va_arg(ap, int);
                serial_putchar(c);
                count++;
                break;

            case '%':
                serial_putchar('%');
                count++;
                break;

            default:
                serial_putchar('%');
                serial_putchar(c);
                count += 2;
                break;
        }
    }

    va_end(ap);
    return count;
}

/* Kernel panic - print message and halt */
void kpanic(const char *fmt, ...) {
    va_list ap;
    char c;
    const char *s;
    int64_t d;
    uint64_t u;

    /* Disable interrupts */
    __asm__ volatile ("cli");

    serial_print("\n\n");
    serial_print("==================== KERNEL PANIC ====================\n");

    va_start(ap, fmt);

    while ((c = *fmt++) != '\0') {
        if (c != '%') {
            if (c == '\n') {
                serial_putchar('\r');
            }
            serial_putchar(c);
            continue;
        }

        c = *fmt++;
        if (c == '\0') break;

        /* Skip modifiers */
        while (c == 'l' || (c >= '0' && c <= '9')) {
            c = *fmt++;
            if (c == '\0') break;
        }

        switch (c) {
            case 'd':
            case 'i':
                d = va_arg(ap, int64_t);
                print_signed(d, 0, ' ');
                break;
            case 'u':
                u = va_arg(ap, uint64_t);
                print_num(u, 10, 0, ' ', 0);
                break;
            case 'x':
                u = va_arg(ap, uint64_t);
                print_num(u, 16, 0, ' ', 0);
                break;
            case 'X':
                u = va_arg(ap, uint64_t);
                print_num(u, 16, 0, ' ', 1);
                break;
            case 'p':
                u = va_arg(ap, uint64_t);
                serial_print("0x");
                print_num(u, 16, 16, '0', 0);
                break;
            case 's':
                s = va_arg(ap, const char *);
                if (s == NULL) s = "(null)";
                serial_print(s);
                break;
            case 'c':
                c = (char)va_arg(ap, int);
                serial_putchar(c);
                break;
            case '%':
                serial_putchar('%');
                break;
        }
    }

    va_end(ap);

    serial_print("======================================================\n");
    serial_print("System halted.\n");

    /* Halt forever */
    while (1) {
        __asm__ volatile ("hlt");
    }
}
