/*
 * kprintf.h - Kernel printf interface
 */

#ifndef _KPRINTF_H
#define _KPRINTF_H

#include "types.h"

/* Formatted output to serial console */
int kprintf(const char *fmt, ...);

/* Kernel panic - print message and halt */
void kpanic(const char *fmt, ...) __attribute__((noreturn));

/* Assert macro */
#define kassert(expr) \
    do { \
        if (!(expr)) { \
            kpanic("Assertion failed: %s\n  File: %s, Line: %d\n", \
                   #expr, __FILE__, __LINE__); \
        } \
    } while (0)

#endif /* _KPRINTF_H */
