/*
 * pit.c - Programmable Interval Timer (8254) implementation
 */

#include "pit.h"
#include "pic.h"
#include "lib/kprintf.h"
#include "proc/scheduler.h"

/* PIT I/O ports */
#define PIT_CHANNEL0    0x40
#define PIT_CHANNEL1    0x41
#define PIT_CHANNEL2    0x42
#define PIT_COMMAND     0x43

/* PIT command bits */
#define PIT_CMD_CHANNEL0    0x00
#define PIT_CMD_LATCH       0x00
#define PIT_CMD_ACCESS_LO   0x10
#define PIT_CMD_ACCESS_HI   0x20
#define PIT_CMD_ACCESS_LOHI 0x30
#define PIT_CMD_MODE0       0x00    /* Interrupt on terminal count */
#define PIT_CMD_MODE2       0x04    /* Rate generator */
#define PIT_CMD_MODE3       0x06    /* Square wave generator */
#define PIT_CMD_BINARY      0x00

/* Port I/O inline functions */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Timer state */
static volatile uint64_t tick_count = 0;
static uint32_t tick_frequency = 0;
static uint32_t ms_per_tick = 0;

/* Initialize PIT */
void pit_init(uint32_t frequency) {
    uint32_t divisor;

    /* Calculate divisor */
    divisor = PIT_BASE_FREQUENCY / frequency;
    if (divisor > 65535) {
        divisor = 65535;
    }
    if (divisor < 1) {
        divisor = 1;
    }

    /* Recalculate actual frequency */
    tick_frequency = PIT_BASE_FREQUENCY / divisor;
    ms_per_tick = 1000 / tick_frequency;

    /* Configure PIT channel 0 for rate generator mode */
    outb(PIT_COMMAND, PIT_CMD_CHANNEL0 | PIT_CMD_ACCESS_LOHI | PIT_CMD_MODE3 | PIT_CMD_BINARY);

    /* Send divisor */
    outb(PIT_CHANNEL0, divisor & 0xFF);         /* Low byte */
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);  /* High byte */

    /* Enable timer IRQ (IRQ0) */
    pic_clear_mask(IRQ_TIMER);

    tick_count = 0;

    kprintf("[PIT] Initialized at %d Hz (divisor=%d)\n", tick_frequency, divisor);
}

/* Get current tick count */
uint64_t pit_get_ticks(void) {
    return tick_count;
}

/* Sleep for specified milliseconds */
void sleep_ms(uint32_t ms) {
    uint64_t target_ticks;
    uint64_t ticks_to_wait;

    if (tick_frequency == 0) {
        return;
    }

    /* Calculate ticks to wait */
    ticks_to_wait = (ms * tick_frequency) / 1000;
    if (ticks_to_wait == 0) {
        ticks_to_wait = 1;
    }

    target_ticks = tick_count + ticks_to_wait;

    /* Busy wait (will be improved with scheduler) */
    while (tick_count < target_ticks) {
        __asm__ volatile ("hlt");
    }
}

/* PIT interrupt handler */
void pit_handler(void) {
    tick_count++;
    pic_send_eoi(IRQ_TIMER);

    /* Call scheduler tick handler */
    scheduler_tick();
}
