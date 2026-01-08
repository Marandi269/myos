/*
 * scheduler.c - Process scheduler implementation (Round-Robin)
 */

#include "scheduler.h"
#include "process.h"
#include "mm/pmm.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* External context switch function (in switch.S) */
extern void switch_context(context_t **old, context_t *new);
extern void switch_to_new(context_t *new);

/* Ready queue (doubly linked list) */
static process_t *ready_queue_head = NULL;
static process_t *ready_queue_tail = NULL;

/* Idle process (runs when no other process is ready) */
static process_t *idle_proc = NULL;

/* Scheduler initialized flag */
static int scheduler_running = 0;

/* Add process to ready queue */
void sched_ready(process_t *proc) {
    if (!proc || proc->state == PROC_RUNNING) {
        return;
    }

    proc->state = PROC_READY;
    proc->next = NULL;
    proc->prev = ready_queue_tail;

    if (ready_queue_tail) {
        ready_queue_tail->next = proc;
    } else {
        ready_queue_head = proc;
    }
    ready_queue_tail = proc;
}

/* Remove process from ready queue */
void sched_remove(process_t *proc) {
    if (!proc) {
        return;
    }

    if (proc->prev) {
        proc->prev->next = proc->next;
    } else {
        ready_queue_head = proc->next;
    }

    if (proc->next) {
        proc->next->prev = proc->prev;
    } else {
        ready_queue_tail = proc->prev;
    }

    proc->next = NULL;
    proc->prev = NULL;
}

/* Get next process from ready queue */
static process_t *sched_next(void) {
    process_t *next = ready_queue_head;

    if (next) {
        sched_remove(next);
    }

    return next;
}

/* Idle process function */
static void idle_func(void *arg) {
    (void)arg;
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/* Thread wrapper - calls the thread function then exits */
static void thread_wrapper(void) {
    process_t *proc = current_proc;
    thread_entry_t entry = (thread_entry_t)proc->context->rbx;
    void *arg = (void *)proc->context->r12;

    /* Call the actual thread function */
    entry(arg);

    /* Thread returned, exit */
    process_exit(0);
}

/* Create a kernel thread */
process_t *kthread_create(thread_entry_t entry, void *arg, const char *name) {
    process_t *proc;
    context_t *ctx;

    /* Allocate process */
    proc = process_alloc();
    if (!proc) {
        return NULL;
    }

    /* Set name */
    strncpy(proc->name, name, sizeof(proc->name) - 1);
    proc->name[sizeof(proc->name) - 1] = '\0';

    /* Set up initial context on kernel stack */
    proc->kernel_stack -= sizeof(context_t);
    ctx = (context_t *)proc->kernel_stack;
    memset(ctx, 0, sizeof(context_t));

    /* Store entry point and arg in callee-saved registers */
    ctx->rbx = (uint64_t)entry;
    ctx->r12 = (uint64_t)arg;
    ctx->rbp = 0;
    ctx->rip = (uint64_t)thread_wrapper;

    proc->context = ctx;

    kprintf("[Scheduler] Created thread '%s' (PID %d)\n", name, proc->pid);

    /* Add to ready queue */
    sched_ready(proc);

    return proc;
}

/* Initialize scheduler */
void scheduler_init(void) {
    /* Initialize process subsystem */
    process_init();

    /* Create idle process (PID 0) */
    idle_proc = kthread_create(idle_func, NULL, "idle");
    if (!idle_proc) {
        kprintf("[Scheduler] ERROR: Failed to create idle process\n");
        return;
    }

    /* Remove idle from ready queue - it's special */
    sched_remove(idle_proc);
    idle_proc->pid = 0;  /* Force PID 0 for idle */

    kprintf("[Scheduler] Initialized\n");
}

/* Schedule next process */
void schedule(void) {
    process_t *prev, *next;

    if (!scheduler_running) {
        return;
    }

    prev = current_proc;
    next = sched_next();

    /* If no ready process, use idle */
    if (!next) {
        next = idle_proc;
    }

    /* Same process, nothing to do */
    if (next == prev) {
        return;
    }

    /* Put previous process back in ready queue if still runnable */
    if (prev && prev->state == PROC_RUNNING && prev != idle_proc) {
        prev->state = PROC_READY;
        sched_ready(prev);
    }

    /* Switch to next process */
    next->state = PROC_RUNNING;
    current_proc = next;

    /* Perform context switch */
    if (prev) {
        switch_context(&prev->context, next->context);
    } else {
        switch_to_new(next->context);
    }
}

/* Yield CPU to another process */
void yield(void) {
    schedule();
}

/* Timer tick - called from PIT interrupt */
void scheduler_tick(void) {
    if (!scheduler_running || !current_proc) {
        return;
    }

    current_proc->total_ticks++;
    current_proc->time_slice--;

    /* Time slice expired, reschedule */
    if (current_proc->time_slice <= 0) {
        current_proc->time_slice = DEFAULT_TIME_SLICE;
        schedule();
    }
}

/* Start the scheduler */
void scheduler_start(void) {
    process_t *first;

    kprintf("[Scheduler] Starting...\n");

    scheduler_running = 1;

    /* Get first process to run */
    first = sched_next();
    if (!first) {
        first = idle_proc;
    }

    first->state = PROC_RUNNING;
    current_proc = first;

    /* Switch to first process (no previous context) */
    switch_to_new(first->context);

    /* Never reached */
}

/* Exit current process */
void process_exit(int exit_code) {
    process_t *proc = current_proc;

    if (!proc || proc == idle_proc) {
        kprintf("[Scheduler] ERROR: Cannot exit idle process\n");
        return;
    }

    kprintf("[Scheduler] Process '%s' (PID %d) exited with code %d\n",
            proc->name, proc->pid, exit_code);

    proc->exit_code = exit_code;
    proc->state = PROC_ZOMBIE;

    /* For now, just free the process */
    process_free(proc);

    /* Schedule another process */
    current_proc = NULL;
    schedule();

    /* Never reached */
    while (1) {
        __asm__ volatile ("hlt");
    }
}
