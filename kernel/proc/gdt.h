/*
 * gdt.h - Global Descriptor Table definitions
 */

#ifndef _GDT_H
#define _GDT_H

#include "types.h"

/* GDT segment selectors
 *
 * Layout optimized for SYSRET:
 *   SYSRET loads: SS = STAR[63:48] + 8 | 3
 *                 CS = STAR[63:48] + 16 | 3
 *
 *   With STAR[63:48] = 0x10 (GDT_KERNEL_DATA):
 *   - SS = 0x10 + 8 = 0x18 | 3 = 0x1B (User Data)
 *   - CS = 0x10 + 16 = 0x20 | 3 = 0x23 (User Code)
 */
#define GDT_NULL        0x00
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_DATA   0x18    /* User Data before User Code for SYSRET */
#define GDT_USER_CODE   0x20
#define GDT_TSS         0x28

/* Ring levels */
#define RING_KERNEL     0
#define RING_USER       3

/* User segment selectors (with RPL=3) */
#define USER_DATA_SEL   (GDT_USER_DATA | RING_USER)  /* 0x1B */
#define USER_CODE_SEL   (GDT_USER_CODE | RING_USER)  /* 0x23 */

/* GDT entry structure */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

/* TSS descriptor (16 bytes in 64-bit mode) */
struct tss_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

/* GDT pointer */
struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* Initialize GDT with user segments and TSS */
void gdt_init(void);

/* Reload GDT (after modification) */
void gdt_reload(void);

#endif /* _GDT_H */
