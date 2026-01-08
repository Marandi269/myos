/*
 * scheduler.h - Process scheduler interface
 */

#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include "process.h"

/* Scheduler entry function type for new threads */
typedef void (*thread_entry_t)(void *arg);

/* Initialize scheduler */
void scheduler_init(void);

/* Start the scheduler (never returns) */
void scheduler_start(void);

/* Schedule next process */
void schedule(void);

/* Yield CPU to another process */
void yield(void);

/* Timer tick handler (called from PIT interrupt) */
void scheduler_tick(void);

/* Add process to ready queue */
void sched_ready(process_t *proc);

/* Remove process from ready queue */
void sched_remove(process_t *proc);

/* Create a kernel thread */
process_t *kthread_create(thread_entry_t entry, void *arg, const char *name);

/* Exit current process */
void process_exit(int exit_code);

/* Get current process */
static inline process_t *get_current(void) {
    extern process_t *current_proc;
    return current_proc;
}

#endif /* _SCHEDULER_H */
