/*
 * futex.c - Fast Userspace Mutex implementation
 */

#include "proc/futex.h"
#include "proc/process.h"
#include "proc/scheduler.h"
#include "proc/syscall.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Global futex wait queue - simple implementation */
static futex_waiter_t futex_waiters[FUTEX_MAX_WAITERS];

/* Initialize futex subsystem */
void futex_init(void) {
    memset(futex_waiters, 0, sizeof(futex_waiters));
    kprintf("[futex] Initialized\n");
}

/* Find a free waiter slot */
static futex_waiter_t *futex_alloc_waiter(void) {
    for (int i = 0; i < FUTEX_MAX_WAITERS; i++) {
        if (!futex_waiters[i].active) {
            return &futex_waiters[i];
        }
    }
    return NULL;
}

/* Wait on a futex
 * Block until *uaddr != val or woken up
 */
static int futex_wait(uint32_t *uaddr, uint32_t val, uint32_t bitset) {
    process_t *proc = current_proc;
    futex_waiter_t *waiter;

    if (!proc || !uaddr) {
        return -EINVAL;
    }

    /* Check if value still matches */
    if (*uaddr != val) {
        return -EAGAIN;  /* Value changed, don't sleep */
    }

    /* Allocate waiter */
    waiter = futex_alloc_waiter();
    if (!waiter) {
        return -ENOMEM;
    }

    /* Setup waiter */
    waiter->uaddr = uaddr;
    waiter->proc = proc;
    waiter->bitset = bitset ? bitset : 0xFFFFFFFF;
    waiter->active = true;
    waiter->next = NULL;

    /* Block the process */
    proc->state = PROC_BLOCKED;

    /* Yield to another process */
    yield();

    /* We're back - either woken up or spurious wakeup */
    waiter->active = false;

    return 0;
}

/* Wake processes waiting on a futex */
static int futex_wake(uint32_t *uaddr, int nr_wake, uint32_t bitset) {
    int woken = 0;

    if (!uaddr) {
        return -EINVAL;
    }

    if (!bitset) {
        bitset = 0xFFFFFFFF;
    }

    for (int i = 0; i < FUTEX_MAX_WAITERS && woken < nr_wake; i++) {
        futex_waiter_t *waiter = &futex_waiters[i];

        if (waiter->active && waiter->uaddr == uaddr &&
            (waiter->bitset & bitset)) {
            /* Wake this process */
            waiter->active = false;
            if (waiter->proc && waiter->proc->state == PROC_BLOCKED) {
                waiter->proc->state = PROC_READY;
                sched_ready(waiter->proc);
                woken++;
            }
        }
    }

    return woken;
}

/* Futex system call */
int64_t sys_futex(uint32_t *uaddr, int futex_op, uint32_t val,
                   const void *timeout, uint32_t *uaddr2, uint32_t val3) {
    int op = futex_op & FUTEX_CMD_MASK;

    (void)timeout;  /* TODO: implement timeout */
    (void)uaddr2;
    (void)val3;

    switch (op) {
        case FUTEX_WAIT:
            return futex_wait(uaddr, val, 0xFFFFFFFF);

        case FUTEX_WAIT_BITSET:
            return futex_wait(uaddr, val, val3);

        case FUTEX_WAKE:
            return futex_wake(uaddr, val, 0xFFFFFFFF);

        case FUTEX_WAKE_BITSET:
            return futex_wake(uaddr, val, val3);

        case FUTEX_REQUEUE:
        case FUTEX_CMP_REQUEUE:
            /* Not implemented - return number of waiters requeued (0) */
            return 0;

        default:
            kprintf("[futex] Unsupported operation: %d\n", op);
            return -ENOSYS;
    }
}

/* Clean up futex waiters when process exits */
void futex_cleanup(struct process *proc) {
    for (int i = 0; i < FUTEX_MAX_WAITERS; i++) {
        if (futex_waiters[i].active && futex_waiters[i].proc == proc) {
            futex_waiters[i].active = false;
        }
    }
}
