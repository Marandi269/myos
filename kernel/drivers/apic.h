/*
 * apic.h - Advanced Programmable Interrupt Controller
 *
 * Supports Local APIC and I/O APIC for SMP systems.
 */

#ifndef _APIC_H
#define _APIC_H

#include "types.h"

/* Local APIC register offsets (memory-mapped) */
#define LAPIC_ID            0x020   /* APIC ID */
#define LAPIC_VER           0x030   /* Version */
#define LAPIC_TPR           0x080   /* Task Priority */
#define LAPIC_APR           0x090   /* Arbitration Priority */
#define LAPIC_PPR           0x0A0   /* Processor Priority */
#define LAPIC_EOI           0x0B0   /* End of Interrupt */
#define LAPIC_RRD           0x0C0   /* Remote Read */
#define LAPIC_LDR           0x0D0   /* Logical Destination */
#define LAPIC_DFR           0x0E0   /* Destination Format */
#define LAPIC_SVR           0x0F0   /* Spurious Interrupt Vector */
#define LAPIC_ISR           0x100   /* In-Service (8 regs) */
#define LAPIC_TMR           0x180   /* Trigger Mode (8 regs) */
#define LAPIC_IRR           0x200   /* Interrupt Request (8 regs) */
#define LAPIC_ESR           0x280   /* Error Status */
#define LAPIC_ICR_LO        0x300   /* Interrupt Command (low 32) */
#define LAPIC_ICR_HI        0x310   /* Interrupt Command (high 32) */
#define LAPIC_LVT_TIMER     0x320   /* LVT Timer */
#define LAPIC_LVT_THERMAL   0x330   /* LVT Thermal Sensor */
#define LAPIC_LVT_PERF      0x340   /* LVT Performance Counter */
#define LAPIC_LVT_LINT0     0x350   /* LVT LINT0 */
#define LAPIC_LVT_LINT1     0x360   /* LVT LINT1 */
#define LAPIC_LVT_ERROR     0x370   /* LVT Error */
#define LAPIC_TIMER_ICR     0x380   /* Timer Initial Count */
#define LAPIC_TIMER_CCR     0x390   /* Timer Current Count */
#define LAPIC_TIMER_DCR     0x3E0   /* Timer Divide Configuration */

/* Spurious Vector Register bits */
#define SVR_ENABLE          0x100   /* APIC Software Enable */
#define SVR_FOCUS_DISABLED  0x200   /* Focus Processor Checking */

/* ICR delivery modes */
#define ICR_FIXED           0x00000 /* Fixed delivery */
#define ICR_LOWEST          0x00100 /* Lowest priority */
#define ICR_SMI             0x00200 /* SMI */
#define ICR_NMI             0x00400 /* NMI */
#define ICR_INIT            0x00500 /* INIT IPI */
#define ICR_STARTUP         0x00600 /* Startup IPI */

/* ICR destination modes */
#define ICR_PHYSICAL        0x00000 /* Physical destination */
#define ICR_LOGICAL         0x00800 /* Logical destination */

/* ICR delivery status */
#define ICR_IDLE            0x00000 /* Idle */
#define ICR_SEND_PENDING    0x01000 /* Send pending */

/* ICR level */
#define ICR_DEASSERT        0x00000 /* De-assert */
#define ICR_ASSERT          0x04000 /* Assert */

/* ICR trigger mode */
#define ICR_EDGE            0x00000 /* Edge triggered */
#define ICR_LEVEL           0x08000 /* Level triggered */

/* ICR destination shorthand */
#define ICR_NO_SHORTHAND    0x00000 /* No shorthand */
#define ICR_SELF            0x40000 /* Self */
#define ICR_ALL_INCLUDING   0x80000 /* All including self */
#define ICR_ALL_EXCLUDING   0xC0000 /* All excluding self */

/* LVT bits */
#define LVT_MASKED          0x10000 /* Masked */
#define LVT_TIMER_PERIODIC  0x20000 /* Periodic timer */
#define LVT_TIMER_TSC_DL    0x40000 /* TSC-Deadline mode */

/* Timer divide values */
#define TIMER_DIV_1         0xB
#define TIMER_DIV_2         0x0
#define TIMER_DIV_4         0x1
#define TIMER_DIV_8         0x2
#define TIMER_DIV_16        0x3
#define TIMER_DIV_32        0x8
#define TIMER_DIV_64        0x9
#define TIMER_DIV_128       0xA

/* I/O APIC registers */
#define IOAPIC_ID           0x00    /* ID */
#define IOAPIC_VER          0x01    /* Version */
#define IOAPIC_ARB          0x02    /* Arbitration */
#define IOAPIC_REDTBL       0x10    /* Redirection table (0x10-0x3F) */

/* I/O APIC redirection entry bits */
#define IOREDTBL_MASKED     0x10000 /* Masked */

/* Maximum CPUs supported */
#define MAX_CPUS            16

/* CPU state */
typedef enum {
    CPU_OFFLINE = 0,
    CPU_STARTING,
    CPU_ONLINE
} cpu_state_t;

/* Per-CPU structure */
typedef struct cpu_info {
    uint32_t id;            /* CPU ID */
    uint32_t apic_id;       /* APIC ID */
    cpu_state_t state;      /* Current state */
    uint64_t kernel_stack;  /* Kernel stack top */
    void *current_task;     /* Current running task */
    uint64_t ticks;         /* Timer ticks */
    bool is_bsp;            /* Is bootstrap processor */
} cpu_info_t;

/* Global CPU information */
extern cpu_info_t cpus[MAX_CPUS];
extern uint32_t cpu_count;
extern uint32_t bsp_id;
extern uint64_t lapic_base;
extern uint64_t ioapic_base;

/* Initialize Local APIC */
void lapic_init(void);

/* Initialize I/O APIC */
void ioapic_init(void);

/* Send End of Interrupt */
void lapic_eoi(void);

/* Get current CPU's APIC ID */
uint32_t lapic_get_id(void);

/* Send IPI (Inter-Processor Interrupt) */
void lapic_send_ipi(uint32_t apic_id, uint32_t vector);

/* Send INIT IPI */
void lapic_send_init(uint32_t apic_id);

/* Send SIPI (Startup IPI) */
void lapic_send_sipi(uint32_t apic_id, uint8_t vector);

/* Configure I/O APIC entry */
void ioapic_set_entry(uint8_t irq, uint8_t vector, uint32_t apic_id);

/* Enable/disable I/O APIC entry */
void ioapic_enable(uint8_t irq);
void ioapic_disable(uint8_t irq);

/* Read/write Local APIC register */
uint32_t lapic_read(uint32_t reg);
void lapic_write(uint32_t reg, uint32_t value);

/* Start APIC timer */
void lapic_timer_init(uint32_t frequency);
void lapic_timer_stop(void);

/* Get current CPU info */
cpu_info_t *get_cpu_info(void);

/* Get current CPU ID */
uint32_t get_cpu_id(void);

#endif /* _APIC_H */
