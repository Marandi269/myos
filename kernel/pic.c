/*
 * pic.c - Programmable Interrupt Controller (8259) implementation
 */

#include "pic.h"
#include "serial.h"

/* Port I/O inline functions */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* I/O wait for slow devices */
static inline void io_wait(void) {
    outb(0x80, 0);
}

/* Initialize the 8259 PIC */
void pic_init(void) {
    /* Save masks */
    uint8_t mask1, mask2;
    mask1 = inb(PIC1_DATA);
    mask2 = inb(PIC2_DATA);

    /* ICW1: Start initialization sequence (cascade mode) */
    outb(PIC1_CMD, 0x11);
    io_wait();
    outb(PIC2_CMD, 0x11);
    io_wait();

    /* ICW2: Set vector offsets */
    outb(PIC1_DATA, 0x20);  /* IRQ 0-7  -> INT 32-39 */
    io_wait();
    outb(PIC2_DATA, 0x28);  /* IRQ 8-15 -> INT 40-47 */
    io_wait();

    /* ICW3: Configure cascading */
    outb(PIC1_DATA, 0x04);  /* Tell Master PIC there is a slave at IRQ2 */
    io_wait();
    outb(PIC2_DATA, 0x02);  /* Tell Slave PIC its cascade identity */
    io_wait();

    /* ICW4: Set 8086 mode */
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    /* Restore saved masks (mask all interrupts initially) */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    (void)mask1;
    (void)mask2;

    serial_print("[PIC] Initialized\n");
}

/* Send End of Interrupt signal */
void pic_send_eoi(uint8_t irq) {
    /* If IRQ came from slave PIC, send EOI to slave too */
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    /* Always send EOI to master */
    outb(PIC1_CMD, PIC_EOI);
}

/* Enable an IRQ line (unmask) */
void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

/* Disable an IRQ line (mask) */
void pic_set_mask(uint8_t irq) {
    uint16_t port;
    uint8_t value;

    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }

    value = inb(port) | (1 << irq);
    outb(port, value);
}
