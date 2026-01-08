/*
 * main.c - MyOS kernel entry point
 */

#include "types.h"
#include "serial.h"
#include "idt.h"
#include "pic.h"
#include "keyboard.h"

/* Exception names for debugging */
static const char *exception_names[] = {
    "Division by Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating Point"
};

/* Exception handler - called from assembly */
void exception_handler(int num) {
    serial_print("\n!!! EXCEPTION: ");
    if (num < 20) {
        serial_print(exception_names[num]);
    } else {
        serial_print("Unknown (");
        serial_print_dec(num);
        serial_print(")");
    }
    serial_print(" !!!\n");

    /* Halt the system */
    serial_print("System halted.\n");
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

/* IRQ handler - called from assembly */
void irq_handler(int num) {
    /* IRQ1 (INT 33) = Keyboard */
    if (num == 33) {
        keyboard_handler();
    } else {
        /* Unknown IRQ, just send EOI */
        pic_send_eoi(num - 32);
    }
}

/* Kernel main entry point */
void kernel_main(void) {
    /* Initialize serial port first for debug output */
    serial_init();

    /* Print welcome banner */
    serial_print("\n");
    serial_print("=============================\n");
    serial_print("  Hello from MyOS!\n");
    serial_print("  64-bit kernel running\n");
    serial_print("=============================\n");
    serial_print("\n");

    /* Initialize PIC (must be before IDT enables interrupts) */
    pic_init();

    /* Initialize IDT */
    idt_init();

    /* Initialize keyboard driver */
    keyboard_init();

    /* Enable interrupts */
    __asm__ volatile ("sti");

    serial_print("\n[Kernel] Ready. Type something:\n");

    /* Main kernel loop - just wait for interrupts */
    while (1) {
        __asm__ volatile ("hlt");
    }
}
