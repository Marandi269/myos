/*
 * wait_queue.c - Wait Queue implementation
 *
 * Provides sleeping and waking mechanisms for I/O multiplexing.
 */

#include "wait_queue.h"
#include "process.h"
#include "scheduler.h"
#include "../lib/kprintf.h"
#include "../drivers/pit.h"

/*
 * Initialize a wait queue head
 */
void init_waitqueue_head(wait_queue_head_t *wq) {
    if (!wq) return;
    wq->head = NULL;
    wq->tail = NULL;
}

/*
 * Initialize a wait queue entry for the current process
 */
void init_waitqueue_entry(wait_queue_entry_t *entry) {
    if (!entry) return;
    entry->task = current_proc;
    entry->next = NULL;
    entry->prev = NULL;
}

/*
 * Add entry to wait queue (at tail)
 */
void add_wait_queue(wait_queue_head_t *wq, wait_queue_entry_t *entry) {
    if (!wq || !entry) return;

    entry->next = NULL;
    entry->prev = wq->tail;

    if (wq->tail) {
        wq->tail->next = entry;
    } else {
        wq->head = entry;
    }
    wq->tail = entry;
}

/*
 * Remove entry from wait queue
 */
void remove_wait_queue(wait_queue_head_t *wq, wait_queue_entry_t *entry) {
    if (!wq || !entry) return;

    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        wq->head = entry->next;
    }

    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        wq->tail = entry->prev;
    }

    entry->next = NULL;
    entry->prev = NULL;
}

/*
 * Check if wait queue is empty
 */
int waitqueue_empty(wait_queue_head_t *wq) {
    return (wq == NULL || wq->head == NULL);
}

/*
 * Wake up one process waiting on this queue
 */
void wake_up(wait_queue_head_t *wq) {
    wait_queue_entry_t *entry;
    process_t *task;

    if (!wq || !wq->head) return;

    entry = wq->head;
    task = entry->task;

    if (task && task->state == PROC_BLOCKED) {
        /* Add process back to ready queue */
        sched_ready(task);
    }
}

/*
 * Wake up all processes waiting on this queue
 */
void wake_up_all(wait_queue_head_t *wq) {
    wait_queue_entry_t *entry;
    process_t *task;

    if (!wq) return;

    for (entry = wq->head; entry != NULL; entry = entry->next) {
        task = entry->task;
        if (task && task->state == PROC_BLOCKED) {
            sched_ready(task);
        }
    }
}

/*
 * Internal helper: put current process to sleep
 */
void __wait_queue_sleep(void) {
    if (!current_proc) return;

    /* Set process state to blocked */
    current_proc->state = PROC_BLOCKED;

    /* Call scheduler to switch to another process */
    schedule();
}

/*
 * Internal helper: get current tick count
 */
uint64_t __wait_queue_get_ticks(void) {
    return pit_get_ticks();
}

/*
 * Set current process to blocked state
 */
void set_current_state_blocked(void) {
    if (current_proc) {
        current_proc->state = PROC_BLOCKED;
    }
}

/*
 * Set current process to running state
 */
void set_current_state_running(void) {
    if (current_proc) {
        current_proc->state = PROC_RUNNING;
    }
}
