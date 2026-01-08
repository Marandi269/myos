/*
 * apic.c - Advanced Programmable Interrupt Controller driver
 */

#include "drivers/apic.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "mm/vmm.h"

/* Global CPU information */
cpu_info_t cpus[MAX_CPUS];
uint32_t cpu_count = 0;
uint32_t bsp_id = 0;
uint64_t lapic_base = 0xFEE00000;  /* Default LAPIC base */
uint64_t ioapic_base = 0xFEC00000; /* Default I/O APIC base */

/* MSR addresses */
#define MSR_APIC_BASE   0x1B

/* Read MSR */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

/* Write MSR */
static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

/* Delay */
static void delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm__ volatile ("pause");
    }
}

/* Read Local APIC register */
uint32_t lapic_read(uint32_t reg) {
    return *(volatile uint32_t *)(lapic_base + reg);
}

/* Write Local APIC register */
void lapic_write(uint32_t reg, uint32_t value) {
    *(volatile uint32_t *)(lapic_base + reg) = value;
    /* Read back to ensure write completion */
    (void)lapic_read(LAPIC_ID);
}

/* Read I/O APIC register */
static uint32_t ioapic_read(uint32_t reg) {
    *(volatile uint32_t *)(ioapic_base + 0x00) = reg;
    return *(volatile uint32_t *)(ioapic_base + 0x10);
}

/* Write I/O APIC register */
static void ioapic_write(uint32_t reg, uint32_t value) {
    *(volatile uint32_t *)(ioapic_base + 0x00) = reg;
    *(volatile uint32_t *)(ioapic_base + 0x10) = value;
}

/* Get Local APIC ID */
uint32_t lapic_get_id(void) {
    return (lapic_read(LAPIC_ID) >> 24) & 0xFF;
}

/* Get current CPU ID */
uint32_t get_cpu_id(void) {
    uint32_t apic_id = lapic_get_id();
    for (uint32_t i = 0; i < cpu_count; i++) {
        if (cpus[i].apic_id == apic_id) {
            return cpus[i].id;
        }
    }
    return 0;  /* Default to BSP */
}

/* Get current CPU info */
cpu_info_t *get_cpu_info(void) {
    return &cpus[get_cpu_id()];
}

/* Initialize Local APIC */
void lapic_init(void) {
    uint64_t base;
    uint32_t ver, max_lvt;
    uint32_t apic_id;

    /* Get APIC base address from MSR */
    base = rdmsr(MSR_APIC_BASE);
    lapic_base = base & 0xFFFFF000;  /* Page-aligned base */

    /* Map LAPIC to virtual memory (identity mapped, cache disabled) */
    vmm_map_page(lapic_base, lapic_base, PTE_WRITABLE | PTE_PCD);

    /* Enable APIC in MSR if not already enabled */
    if (!(base & 0x800)) {
        wrmsr(MSR_APIC_BASE, base | 0x800);
    }

    /* Get APIC ID and version */
    apic_id = lapic_get_id();
    ver = lapic_read(LAPIC_VER);
    max_lvt = ((ver >> 16) & 0xFF) + 1;

    kprintf("[LAPIC] Base: 0x%lx, ID: %d, Version: 0x%x, LVT entries: %d\n",
            lapic_base, apic_id, ver & 0xFF, max_lvt);

    /* Clear task priority to accept all interrupts */
    lapic_write(LAPIC_TPR, 0);

    /* Enable APIC with spurious interrupt vector 0xFF */
    lapic_write(LAPIC_SVR, SVR_ENABLE | 0xFF);

    /* Mask all LVT entries initially */
    lapic_write(LAPIC_LVT_TIMER, LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT0, LVT_MASKED);
    lapic_write(LAPIC_LVT_LINT1, LVT_MASKED);
    lapic_write(LAPIC_LVT_ERROR, LVT_MASKED);

    if (max_lvt >= 4) {
        lapic_write(LAPIC_LVT_PERF, LVT_MASKED);
    }
    if (max_lvt >= 5) {
        lapic_write(LAPIC_LVT_THERMAL, LVT_MASKED);
    }

    /* Clear error status */
    lapic_write(LAPIC_ESR, 0);
    lapic_write(LAPIC_ESR, 0);

    /* Send EOI to clear any pending interrupts */
    lapic_eoi();

    /* Record BSP info */
    bsp_id = apic_id;
    cpus[0].id = 0;
    cpus[0].apic_id = apic_id;
    cpus[0].state = CPU_ONLINE;
    cpus[0].is_bsp = true;
    cpu_count = 1;

    kprintf("[LAPIC] Initialized (BSP APIC ID: %d)\n", apic_id);
}

/* Initialize I/O APIC */
void ioapic_init(void) {
    uint32_t id, ver, max_entries;

    /* Map I/O APIC to virtual memory (cache disabled) */
    vmm_map_page(ioapic_base, ioapic_base, PTE_WRITABLE | PTE_PCD);

    /* Get I/O APIC ID and version */
    id = (ioapic_read(IOAPIC_ID) >> 24) & 0xF;
    ver = ioapic_read(IOAPIC_VER);
    max_entries = ((ver >> 16) & 0xFF) + 1;

    kprintf("[I/O APIC] Base: 0x%lx, ID: %d, Version: 0x%x, Entries: %d\n",
            ioapic_base, id, ver & 0xFF, max_entries);

    /* Mask all entries initially */
    for (uint32_t i = 0; i < max_entries; i++) {
        uint32_t reg_lo = IOAPIC_REDTBL + i * 2;
        uint32_t reg_hi = IOAPIC_REDTBL + i * 2 + 1;

        /* Set masked, edge-triggered, fixed delivery, physical dest */
        ioapic_write(reg_lo, IOREDTBL_MASKED | (0x20 + i));
        ioapic_write(reg_hi, 0);  /* Route to CPU 0 */
    }

    kprintf("[I/O APIC] Initialized\n");
}

/* Send End of Interrupt */
void lapic_eoi(void) {
    lapic_write(LAPIC_EOI, 0);
}

/* Configure I/O APIC entry */
void ioapic_set_entry(uint8_t irq, uint8_t vector, uint32_t apic_id) {
    uint32_t reg_lo = IOAPIC_REDTBL + irq * 2;
    uint32_t reg_hi = IOAPIC_REDTBL + irq * 2 + 1;

    /* Low: vector, delivery mode (fixed), physical, active-low, edge */
    ioapic_write(reg_lo, vector);

    /* High: destination APIC ID */
    ioapic_write(reg_hi, apic_id << 24);
}

/* Enable I/O APIC entry */
void ioapic_enable(uint8_t irq) {
    uint32_t reg = IOAPIC_REDTBL + irq * 2;
    uint32_t val = ioapic_read(reg);
    ioapic_write(reg, val & ~IOREDTBL_MASKED);
}

/* Disable I/O APIC entry */
void ioapic_disable(uint8_t irq) {
    uint32_t reg = IOAPIC_REDTBL + irq * 2;
    uint32_t val = ioapic_read(reg);
    ioapic_write(reg, val | IOREDTBL_MASKED);
}

/* Send IPI (Inter-Processor Interrupt) */
void lapic_send_ipi(uint32_t apic_id, uint32_t vector) {
    /* Wait for any previous IPI to complete */
    while (lapic_read(LAPIC_ICR_LO) & ICR_SEND_PENDING) {
        __asm__ volatile ("pause");
    }

    /* Set destination APIC ID */
    lapic_write(LAPIC_ICR_HI, apic_id << 24);

    /* Send IPI */
    lapic_write(LAPIC_ICR_LO, ICR_FIXED | ICR_PHYSICAL | ICR_ASSERT |
                ICR_EDGE | vector);
}

/* Send INIT IPI */
void lapic_send_init(uint32_t apic_id) {
    /* Wait for any previous IPI to complete */
    while (lapic_read(LAPIC_ICR_LO) & ICR_SEND_PENDING) {
        __asm__ volatile ("pause");
    }

    /* Set destination APIC ID */
    lapic_write(LAPIC_ICR_HI, apic_id << 24);

    /* Send INIT IPI */
    lapic_write(LAPIC_ICR_LO, ICR_INIT | ICR_PHYSICAL | ICR_ASSERT |
                ICR_EDGE);

    /* Wait for delivery */
    while (lapic_read(LAPIC_ICR_LO) & ICR_SEND_PENDING) {
        __asm__ volatile ("pause");
    }

    /* Delay ~10ms */
    delay(100000);

    /* De-assert INIT */
    lapic_write(LAPIC_ICR_HI, apic_id << 24);
    lapic_write(LAPIC_ICR_LO, ICR_INIT | ICR_PHYSICAL | ICR_LEVEL);
}

/* Send SIPI (Startup IPI) */
void lapic_send_sipi(uint32_t apic_id, uint8_t vector) {
    /* Wait for any previous IPI to complete */
    while (lapic_read(LAPIC_ICR_LO) & ICR_SEND_PENDING) {
        __asm__ volatile ("pause");
    }

    /* Set destination APIC ID */
    lapic_write(LAPIC_ICR_HI, apic_id << 24);

    /* Send SIPI with startup address (vector * 4KB) */
    lapic_write(LAPIC_ICR_LO, ICR_STARTUP | ICR_PHYSICAL | ICR_ASSERT |
                ICR_EDGE | vector);

    /* Wait for delivery */
    while (lapic_read(LAPIC_ICR_LO) & ICR_SEND_PENDING) {
        __asm__ volatile ("pause");
    }

    /* Delay ~200us */
    delay(2000);
}

/* Initialize LAPIC timer */
void lapic_timer_init(uint32_t frequency) {
    uint32_t cpuBusFreq;
    uint32_t divisor = TIMER_DIV_16;
    uint32_t initial_count;

    /* Set divisor */
    lapic_write(LAPIC_TIMER_DCR, divisor);

    /* Calibrate: Start with a large initial count */
    lapic_write(LAPIC_TIMER_ICR, 0xFFFFFFFF);

    /* Wait ~10ms using PIT (or approximate delay) */
    delay(100000);

    /* Read how many ticks elapsed */
    uint32_t elapsed = 0xFFFFFFFF - lapic_read(LAPIC_TIMER_CCR);

    /* Stop timer */
    lapic_write(LAPIC_LVT_TIMER, LVT_MASKED);

    /* Calculate bus frequency (~100 times elapsed for 10ms) */
    cpuBusFreq = elapsed * 100 * 16;  /* Account for divisor */

    /* Calculate initial count for desired frequency */
    initial_count = cpuBusFreq / 16 / frequency;

    kprintf("[LAPIC] Timer calibration: Bus freq ~%d Hz, initial count: %d\n",
            cpuBusFreq, initial_count);

    /* Set up periodic timer interrupt (vector 0x20) */
    lapic_write(LAPIC_TIMER_DCR, divisor);
    lapic_write(LAPIC_TIMER_ICR, initial_count);
    lapic_write(LAPIC_LVT_TIMER, LVT_TIMER_PERIODIC | 0x20);
}

/* Stop LAPIC timer */
void lapic_timer_stop(void) {
    lapic_write(LAPIC_LVT_TIMER, LVT_MASKED);
}
