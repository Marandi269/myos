/*
 * smp.h - Symmetric Multiprocessing support
 */

#ifndef _SMP_H
#define _SMP_H

#include "types.h"
#include "drivers/apic.h"

/* SMP boot trampoline address (must be in first 1MB, page-aligned) */
#define SMP_TRAMPOLINE_ADDR     0x8000

/* AP entry point (set by trampoline) */
extern void ap_entry(void);

/* Initialize SMP subsystem */
void smp_init(void);

/* Start all Application Processors */
void smp_start_aps(void);

/* AP initialization (called on each AP after startup) */
void ap_init(void);

/* Check if SMP is enabled */
int smp_enabled(void);

/* Get number of CPUs online */
uint32_t smp_get_cpu_count(void);

/* Send IPI to specific CPU */
void smp_send_ipi(uint32_t cpu_id, uint32_t vector);

/* Broadcast IPI to all CPUs except self */
void smp_broadcast_ipi(uint32_t vector);

/* Per-CPU initialization hook */
void smp_per_cpu_init(void);

#endif /* _SMP_H */
