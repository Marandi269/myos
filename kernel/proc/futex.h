/*
 * futex.h - Fast Userspace Mutex
 */

#ifndef _FUTEX_H
#define _FUTEX_H

#include "types.h"

/* Futex operations */
#define FUTEX_WAIT          0
#define FUTEX_WAKE          1
#define FUTEX_FD            2   /* Not implemented */
#define FUTEX_REQUEUE       3
#define FUTEX_CMP_REQUEUE   4
#define FUTEX_WAKE_OP       5
#define FUTEX_LOCK_PI       6   /* Not implemented */
#define FUTEX_UNLOCK_PI     7   /* Not implemented */
#define FUTEX_TRYLOCK_PI    8   /* Not implemented */
#define FUTEX_WAIT_BITSET   9
#define FUTEX_WAKE_BITSET   10

/* Futex flags (combined with operation via OR) */
#define FUTEX_PRIVATE_FLAG  128
#define FUTEX_CLOCK_REALTIME 256

/* Extract operation from flags */
#define FUTEX_CMD_MASK      (~(FUTEX_PRIVATE_FLAG | FUTEX_CLOCK_REALTIME))

/* Maximum waiters per futex */
#define FUTEX_MAX_WAITERS   64

/* Futex wait queue entry */
typedef struct futex_waiter {
    uint32_t *uaddr;            /* User address being waited on */
    struct process *proc;       /* Waiting process */
    struct futex_waiter *next;  /* Next in wait queue */
    uint32_t bitset;            /* Wait bitset */
    bool active;                /* Still waiting */
} futex_waiter_t;

/* Initialize futex subsystem */
void futex_init(void);

/* Futex system call */
int64_t sys_futex(uint32_t *uaddr, int futex_op, uint32_t val,
                   const void *timeout, uint32_t *uaddr2, uint32_t val3);

/* Wake waiters when process exits */
void futex_cleanup(struct process *proc);

#endif /* _FUTEX_H */
