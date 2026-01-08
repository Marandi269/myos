/*
 * spinlock.c - Spinlock implementation for SMP
 */

#include "lib/spinlock.h"

/* Get CPU ID (simplified - returns 0 for now) */
static inline uint32_t get_cpu_id(void) {
    /* In real SMP, read from LAPIC ID or CPUID */
    return 0;
}

/* Atomic compare and exchange */
static inline uint32_t atomic_cmpxchg(volatile uint32_t *ptr,
                                       uint32_t old, uint32_t new) {
    uint32_t prev;
    __asm__ volatile (
        "lock cmpxchgl %2, %1"
        : "=a"(prev), "+m"(*ptr)
        : "r"(new), "0"(old)
        : "memory"
    );
    return prev;
}

/* Atomic exchange */
static inline uint32_t atomic_xchg(volatile uint32_t *ptr, uint32_t val) {
    __asm__ volatile (
        "xchgl %0, %1"
        : "=r"(val), "+m"(*ptr)
        : "0"(val)
        : "memory"
    );
    return val;
}

/* Get RFLAGS */
static inline unsigned long get_rflags(void) {
    unsigned long flags;
    __asm__ volatile (
        "pushfq\n"
        "popq %0"
        : "=r"(flags)
        :
        : "memory"
    );
    return flags;
}

/* Set RFLAGS */
static inline void set_rflags(unsigned long flags) {
    __asm__ volatile (
        "pushq %0\n"
        "popfq"
        :
        : "r"(flags)
        : "memory", "cc"
    );
}

/* Disable interrupts */
static inline void cli(void) {
    __asm__ volatile ("cli" ::: "memory");
}

/* Enable interrupts */
static inline void sti(void) {
    __asm__ volatile ("sti" ::: "memory");
}

/* Initialize a spinlock */
void spinlock_init(spinlock_t *lock, const char *name) {
    lock->lock = 0;
    lock->owner = 0;
    lock->name = name;
}

/* Acquire spinlock (busy-wait) */
void spin_lock(spinlock_t *lock) {
    uint32_t cpu = get_cpu_id();

    /* Spin until we get the lock */
    while (atomic_xchg(&lock->lock, 1) != 0) {
        /* Busy wait with pause to reduce bus traffic */
        while (lock->lock) {
            cpu_pause();
        }
    }

    /* We have the lock */
    lock->owner = cpu;
    memory_barrier();
}

/* Try to acquire spinlock (non-blocking) */
int spin_trylock(spinlock_t *lock) {
    if (atomic_xchg(&lock->lock, 1) == 0) {
        lock->owner = get_cpu_id();
        memory_barrier();
        return 1;  /* Success */
    }
    return 0;  /* Lock already held */
}

/* Release spinlock */
void spin_unlock(spinlock_t *lock) {
    memory_barrier();
    lock->owner = 0;
    lock->lock = 0;
}

/* Acquire spinlock and disable interrupts */
unsigned long spin_lock_irqsave(spinlock_t *lock) {
    unsigned long flags = get_rflags();
    cli();
    spin_lock(lock);
    return flags;
}

/* Release spinlock and restore interrupts */
void spin_unlock_irqrestore(spinlock_t *lock, unsigned long flags) {
    spin_unlock(lock);
    if (flags & 0x200) {  /* Check IF bit */
        sti();
    }
}

/* Check if spinlock is held */
int spin_is_locked(spinlock_t *lock) {
    return lock->lock != 0;
}
