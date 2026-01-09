/*
 * wait_queue.h - Wait Queue mechanism for I/O multiplexing
 *
 * Wait queues allow processes to sleep waiting for events and
 * be woken up when those events occur.
 */

#ifndef _WAIT_QUEUE_H
#define _WAIT_QUEUE_H

#include "types.h"

/* Forward declaration */
struct process;

/*
 * Wait queue entry - represents one waiting process
 */
typedef struct wait_queue_entry {
    struct process *task;           /* The waiting process */
    struct wait_queue_entry *next;  /* Next entry in queue */
    struct wait_queue_entry *prev;  /* Previous entry in queue */
} wait_queue_entry_t;

/*
 * Wait queue head - the queue itself
 */
typedef struct wait_queue_head {
    wait_queue_entry_t *head;       /* First entry */
    wait_queue_entry_t *tail;       /* Last entry */
} wait_queue_head_t;

/*
 * Static initializer for wait queue head
 */
#define WAIT_QUEUE_HEAD_INIT { .head = NULL, .tail = NULL }

/*
 * Declare and initialize a wait queue
 */
#define DECLARE_WAIT_QUEUE_HEAD(name) \
    wait_queue_head_t name = WAIT_QUEUE_HEAD_INIT

/*
 * Initialize a wait queue head
 */
void init_waitqueue_head(wait_queue_head_t *wq);

/*
 * Initialize a wait queue entry for the current process
 */
void init_waitqueue_entry(wait_queue_entry_t *entry);

/*
 * Add current process to wait queue (but don't sleep yet)
 */
void add_wait_queue(wait_queue_head_t *wq, wait_queue_entry_t *entry);

/*
 * Remove process from wait queue
 */
void remove_wait_queue(wait_queue_head_t *wq, wait_queue_entry_t *entry);

/*
 * Check if wait queue is empty
 */
int waitqueue_empty(wait_queue_head_t *wq);

/*
 * Wake up one process waiting on this queue
 */
void wake_up(wait_queue_head_t *wq);

/*
 * Wake up all processes waiting on this queue
 */
void wake_up_all(wait_queue_head_t *wq);

/*
 * Sleep on a wait queue until condition is true
 * Usage:
 *   wait_event(wq, condition);
 *
 * This is a macro that:
 * 1. Checks condition, returns if true
 * 2. Adds current process to wait queue
 * 3. Sets process to BLOCKED state
 * 4. Calls schedule()
 * 5. When woken up, re-checks condition
 * 6. Removes from wait queue when condition is true
 */
#define wait_event(wq, condition)                           \
    do {                                                    \
        wait_queue_entry_t __entry;                         \
        if (condition)                                      \
            break;                                          \
        init_waitqueue_entry(&__entry);                     \
        add_wait_queue(&(wq), &__entry);                    \
        while (!(condition)) {                              \
            __wait_queue_sleep();                           \
        }                                                   \
        remove_wait_queue(&(wq), &__entry);                 \
    } while (0)

/*
 * Sleep on a wait queue with timeout
 * Returns: 0 if timed out, positive if woken up
 */
#define wait_event_timeout(wq, condition, timeout_ticks)    \
    ({                                                      \
        int __ret = 1;                                      \
        wait_queue_entry_t __entry;                         \
        uint64_t __deadline;                                \
        if (!(condition)) {                                 \
            init_waitqueue_entry(&__entry);                 \
            add_wait_queue(&(wq), &__entry);                \
            __deadline = __wait_queue_get_ticks() + (timeout_ticks); \
            while (!(condition)) {                          \
                if (__wait_queue_get_ticks() >= __deadline) { \
                    __ret = 0;                              \
                    break;                                  \
                }                                           \
                __wait_queue_sleep();                       \
            }                                               \
            remove_wait_queue(&(wq), &__entry);             \
        }                                                   \
        __ret;                                              \
    })

/*
 * Internal helper: put current process to sleep
 * Sets state to BLOCKED and calls schedule()
 */
void __wait_queue_sleep(void);

/*
 * Internal helper: get current tick count
 */
uint64_t __wait_queue_get_ticks(void);

/*
 * Set current process to sleeping state (BLOCKED)
 * Process will be woken up when added back to ready queue
 */
void set_current_state_blocked(void);

/*
 * Set current process to running state
 */
void set_current_state_running(void);

#endif /* _WAIT_QUEUE_H */
