/*
 * gdt.c - Global Descriptor Table implementation
 */

#include "gdt.h"
#include "tss.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* GDT entries:
 * 0x00: Null
 * 0x08: Kernel Code (64-bit, ring 0)
 * 0x10: Kernel Data (64-bit, ring 0)
 * 0x18: User Code (64-bit, ring 3)
 * 0x20: User Data (64-bit, ring 3)
 * 0x28: TSS (16 bytes)
 */
#define GDT_ENTRIES 7  /* 5 normal entries + TSS (2 slots) */

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr gdtp;

/* External assembly function to reload GDT */
extern void gdt_flush(uint64_t gdtp);

/* Set a GDT entry */
static void gdt_set_entry(int index, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t granularity) {
    gdt[index].base_low = base & 0xFFFF;
    gdt[index].base_mid = (base >> 16) & 0xFF;
    gdt[index].base_high = (base >> 24) & 0xFF;
    gdt[index].limit_low = limit & 0xFFFF;
    gdt[index].granularity = ((limit >> 16) & 0x0F) | (granularity & 0xF0);
    gdt[index].access = access;
}

/* Set TSS descriptor (16 bytes in 64-bit mode) */
static void gdt_set_tss(int index, uint64_t base, uint32_t limit) {
    struct tss_descriptor *tss_desc = (struct tss_descriptor *)&gdt[index];

    tss_desc->limit_low = limit & 0xFFFF;
    tss_desc->base_low = base & 0xFFFF;
    tss_desc->base_mid = (base >> 16) & 0xFF;
    tss_desc->access = 0x89;  /* Present, 64-bit TSS (available) */
    tss_desc->granularity = ((limit >> 16) & 0x0F);
    tss_desc->base_high = (base >> 24) & 0xFF;
    tss_desc->base_upper = (base >> 32) & 0xFFFFFFFF;
    tss_desc->reserved = 0;
}

/* Initialize GDT */
void gdt_init(void) {
    tss_t *tss;

    /* Initialize TSS first */
    tss_init();
    tss = tss_get();

    /* Clear GDT */
    memset(gdt, 0, sizeof(gdt));

    /* Null descriptor */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* Kernel Code: 64-bit, ring 0, executable, readable */
    /* Access: Present(1) | DPL(00) | Type(1) | Exec(1) | DC(0) | RW(1) | Acc(0) = 0x9A */
    /* Granularity: Long mode(1) | Size(0) | Gran(0) = 0x20 */
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0x20);

    /* Kernel Data: 64-bit, ring 0, writable */
    /* Access: Present(1) | DPL(00) | Type(1) | Exec(0) | DC(0) | RW(1) | Acc(0) = 0x92 */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0x20);

    /* User Code: 64-bit, ring 3, executable, readable */
    /* Access: Present(1) | DPL(11) | Type(1) | Exec(1) | DC(0) | RW(1) | Acc(0) = 0xFA */
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0x20);

    /* User Data: 64-bit, ring 3, writable */
    /* Access: Present(1) | DPL(11) | Type(1) | Exec(0) | DC(0) | RW(1) | Acc(0) = 0xF2 */
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0x20);

    /* TSS descriptor (uses 2 slots) */
    gdt_set_tss(5, (uint64_t)tss, sizeof(tss_t) - 1);

    /* Set up GDT pointer */
    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base = (uint64_t)&gdt;

    /* Load GDT */
    gdt_flush((uint64_t)&gdtp);

    /* Load TSS */
    __asm__ volatile ("ltr %0" : : "r"((uint16_t)GDT_TSS));

    kprintf("[GDT] Initialized with user segments and TSS\n");
}

/* Reload GDT */
void gdt_reload(void) {
    gdt_flush((uint64_t)&gdtp);
}
