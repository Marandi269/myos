/*
 * idt.c - Interrupt Descriptor Table implementation
 */

#include "idt.h"
#include "serial.h"

#define IDT_ENTRIES 256

/* IDT and pointer */
static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

/* External ISR stubs defined in boot.S */
extern void isr_stub_0(void);
extern void isr_stub_1(void);
extern void isr_stub_2(void);
extern void isr_stub_3(void);
extern void isr_stub_4(void);
extern void isr_stub_5(void);
extern void isr_stub_6(void);
extern void isr_stub_7(void);
extern void isr_stub_8(void);
extern void isr_stub_9(void);
extern void isr_stub_10(void);
extern void isr_stub_11(void);
extern void isr_stub_12(void);
extern void isr_stub_13(void);
extern void isr_stub_14(void);
extern void isr_stub_15(void);
extern void isr_stub_16(void);
extern void isr_stub_17(void);
extern void isr_stub_18(void);
extern void isr_stub_19(void);

/* IRQ stubs (IRQ0-15 = INT 32-47) */
extern void isr_stub_32(void);
extern void isr_stub_33(void);
extern void isr_stub_34(void);
extern void isr_stub_35(void);
extern void isr_stub_36(void);
extern void isr_stub_37(void);
extern void isr_stub_38(void);
extern void isr_stub_39(void);
extern void isr_stub_40(void);
extern void isr_stub_41(void);
extern void isr_stub_42(void);
extern void isr_stub_43(void);
extern void isr_stub_44(void);
extern void isr_stub_45(void);
extern void isr_stub_46(void);
extern void isr_stub_47(void);

/* Set an IDT gate */
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low  = handler & 0xFFFF;
    idt[num].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].selector    = selector;
    idt[num].ist         = 0;
    idt[num].type_attr   = flags;
    idt[num].zero        = 0;
}

/* Initialize IDT */
void idt_init(void) {
    int i;

    /* Set up IDT pointer */
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt;

    /* Clear all IDT entries */
    for (i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    /* Set up exception handlers (0-19) */
    idt_set_gate(0,  (uint64_t)isr_stub_0,  0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(1,  (uint64_t)isr_stub_1,  0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(2,  (uint64_t)isr_stub_2,  0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(3,  (uint64_t)isr_stub_3,  0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(4,  (uint64_t)isr_stub_4,  0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(5,  (uint64_t)isr_stub_5,  0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(6,  (uint64_t)isr_stub_6,  0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(7,  (uint64_t)isr_stub_7,  0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(8,  (uint64_t)isr_stub_8,  0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(9,  (uint64_t)isr_stub_9,  0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(10, (uint64_t)isr_stub_10, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(11, (uint64_t)isr_stub_11, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(12, (uint64_t)isr_stub_12, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(13, (uint64_t)isr_stub_13, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(14, (uint64_t)isr_stub_14, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(15, (uint64_t)isr_stub_15, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(16, (uint64_t)isr_stub_16, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(17, (uint64_t)isr_stub_17, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(18, (uint64_t)isr_stub_18, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(19, (uint64_t)isr_stub_19, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);

    /* Set up IRQ handlers (32-47) */
    idt_set_gate(32, (uint64_t)isr_stub_32, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(33, (uint64_t)isr_stub_33, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(34, (uint64_t)isr_stub_34, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(35, (uint64_t)isr_stub_35, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(36, (uint64_t)isr_stub_36, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(37, (uint64_t)isr_stub_37, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(38, (uint64_t)isr_stub_38, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(39, (uint64_t)isr_stub_39, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(40, (uint64_t)isr_stub_40, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(41, (uint64_t)isr_stub_41, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(42, (uint64_t)isr_stub_42, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(43, (uint64_t)isr_stub_43, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(44, (uint64_t)isr_stub_44, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(45, (uint64_t)isr_stub_45, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(46, (uint64_t)isr_stub_46, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);
    idt_set_gate(47, (uint64_t)isr_stub_47, 0x08, IDT_PRESENT | IDT_DPL0 | IDT_INTERRUPT);

    /* Load IDT */
    __asm__ volatile ("lidt %0" : : "m"(idtp));

    serial_print("[IDT] Initialized\n");
}
