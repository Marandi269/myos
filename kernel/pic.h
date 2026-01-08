/*
 * pic.h - Programmable Interrupt Controller (8259) interface
 */

#ifndef _PIC_H
#define _PIC_H

#include "types.h"

/* PIC ports */
#define PIC1_CMD    0x20    /* Master PIC command port */
#define PIC1_DATA   0x21    /* Master PIC data port */
#define PIC2_CMD    0xA0    /* Slave PIC command port */
#define PIC2_DATA   0xA1    /* Slave PIC data port */

/* PIC commands */
#define PIC_EOI     0x20    /* End of Interrupt */

/* IRQ numbers */
#define IRQ_TIMER       0
#define IRQ_KEYBOARD    1
#define IRQ_CASCADE     2
#define IRQ_COM2        3
#define IRQ_COM1        4
#define IRQ_LPT2        5
#define IRQ_FLOPPY      6
#define IRQ_LPT1        7
#define IRQ_RTC         8
#define IRQ_FREE1       9
#define IRQ_FREE2       10
#define IRQ_FREE3       11
#define IRQ_MOUSE       12
#define IRQ_FPU         13
#define IRQ_ATA1        14
#define IRQ_ATA2        15

/* Initialize PIC */
void pic_init(void);

/* Send End of Interrupt signal */
void pic_send_eoi(uint8_t irq);

/* Enable an IRQ line (unmask) */
void pic_clear_mask(uint8_t irq);

/* Disable an IRQ line (mask) */
void pic_set_mask(uint8_t irq);

#endif /* _PIC_H */
