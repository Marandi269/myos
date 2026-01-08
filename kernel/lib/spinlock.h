/*
 * spinlock.h - Spinlock implementation for SMP
 */

#ifndef _SPINLOCK_H
#define _SPINLOCK_H

#include "types.h"

/* Spinlock structure */
typedef struct spinlock {
    volatile uint32_t lock;
    volatile uint32_t owner;   /* CPU that holds the lock */
    const char *name;          /* For debugging */
} spinlock_t;

/* Static initializer */
#define SPINLOCK_INIT(n) { 0, 0, n }

/* Initialize a spinlock */
void spinlock_init(spinlock_t *lock, const char *name);

/* Acquire spinlock (busy-wait) */
void spin_lock(spinlock_t *lock);

/* Try to acquire spinlock (non-blocking) */
int spin_trylock(spinlock_t *lock);

/* Release spinlock */
void spin_unlock(spinlock_t *lock);

/* Acquire spinlock and disable interrupts */
unsigned long spin_lock_irqsave(spinlock_t *lock);

/* Release spinlock and restore interrupts */
void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags);

/* Check if spinlock is held */
int spin_is_locked(spinlock_t *lock);

/* CPU pause instruction for busy-wait loops */
static inline void cpu_pause(void) {
    __asm__ volatile ("pause" ::: "memory");
}

/* Memory barrier */
static inline void memory_barrier(void) {
    __asm__ volatile ("mfence" ::: "memory");
}

/* Store barrier */
static inline void store_barrier(void) {
    __asm__ volatile ("sfence" ::: "memory");
}

/* Load barrier */
static inline void load_barrier(void) {
    __asm__ volatile ("lfence" ::: "memory");
}

#endif /* _SPINLOCK_H */
