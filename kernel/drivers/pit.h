/*
 * pit.h - Programmable Interval Timer (8254) interface
 */

#ifndef _PIT_H
#define _PIT_H

#include "types.h"

/* PIT frequency (1.193182 MHz) */
#define PIT_BASE_FREQUENCY  1193182

/* Default tick frequency (100 Hz = 10ms per tick) */
#define PIT_DEFAULT_FREQ    100

/* Initialize PIT with given frequency */
void pit_init(uint32_t frequency);

/* Get current tick count */
uint64_t pit_get_ticks(void);

/* Sleep for specified milliseconds */
void sleep_ms(uint32_t ms);

/* PIT interrupt handler (called from IRQ0) */
void pit_handler(void);

#endif /* _PIT_H */
