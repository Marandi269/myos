/*
 * idt.h - Interrupt Descriptor Table interface
 */

#ifndef _IDT_H
#define _IDT_H

#include "types.h"

/* IDT entry structure (64-bit mode) */
struct idt_entry {
    uint16_t offset_low;     /* Offset bits 0-15 */
    uint16_t selector;       /* Code segment selector */
    uint8_t  ist;            /* Interrupt Stack Table offset */
    uint8_t  type_attr;      /* Type and attributes */
    uint16_t offset_mid;     /* Offset bits 16-31 */
    uint32_t offset_high;    /* Offset bits 32-63 */
    uint32_t zero;           /* Reserved, must be zero */
} __attribute__((packed));

/* IDT pointer structure */
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* IDT type attributes */
#define IDT_PRESENT     0x80
#define IDT_DPL0        0x00
#define IDT_DPL3        0x60
#define IDT_INTERRUPT   0x0E    /* 64-bit Interrupt Gate */
#define IDT_TRAP        0x0F    /* 64-bit Trap Gate */

/* Initialize IDT */
void idt_init(void);

/* Set an IDT gate */
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags);

#endif /* _IDT_H */
