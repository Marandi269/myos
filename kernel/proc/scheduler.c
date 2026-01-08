/*
 * scheduler.c - Round-Robin scheduler implementation
 */

#include "scheduler.h"
#include "process.h"
#include "mm/pmm.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* External assembly functions */
extern void switch_context(context_t **old, context_t *new_ctx);
extern void switch_to_new(context_t *new_ctx);

/* Ready queue (doubly linked circular list) */
static process_t *ready_queue_head = NULL;
static process_t *ready_queue_tail = NULL;

/* Idle process */
static process_t *idle_proc = NULL;

/* Scheduler state */
static int scheduler_running = 0;

/* Add process to ready queue */
void sched_ready(process_t *proc) {
    if (!proc || proc->state == PROC_RUNNING) {
        return;
    }

    proc->state = PROC_READY;
    proc->next = NULL;
    proc->prev = NULL;

    if (!ready_queue_head) {
        ready_queue_head = proc;
        ready_queue_tail = proc;
    } else {
        ready_queue_tail->next = proc;
        proc->prev = ready_queue_tail;
        ready_queue_tail = proc;
    }
}

/* Remove process from ready queue */
void sched_remove(process_t *proc) {
    if (!proc) return;

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

/* Pick next process to run */
static process_t *pick_next(void) {
    process_t *next = ready_queue_head;

    if (next) {
        sched_remove(next);
    } else {
        next = idle_proc;
    }

    return next;
}

/* Idle thread function */
static void idle_thread(void *arg) {
    (void)arg;
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/* Thread wrapper to handle return */
static void thread_wrapper(void) {
    /* Get the entry point and arg from stack setup */
    process_t *proc = current_proc;
    thread_entry_t entry;
    void *arg;

    /* Entry and arg were pushed on stack during creation */
    /* They are at known offsets from context */
    uint64_t *stack = (uint64_t *)proc->context;
    entry = (thread_entry_t)stack[7];  /* After context struct */
    arg = (void *)stack[8];

    /* Call the actual thread function */
    entry(arg);

    /* If thread returns, exit */
    process_exit(0);
}

/* Create a kernel thread */
process_t *kthread_create(thread_entry_t entry, void *arg, const char *name) {
    process_t *proc;
    uint64_t *stack;
    context_t *ctx;

    /* Allocate process */
    proc = process_alloc();
    if (!proc) {
        return NULL;
    }

    /* Copy name */
    strncpy(proc->name, name, sizeof(proc->name) - 1);
    proc->name[sizeof(proc->name) - 1] = '\0';

    /* Set up initial stack */
    stack = (uint64_t *)proc->kernel_stack;

    /* Push entry and arg for thread_wrapper to use */
    *(--stack) = (uint64_t)arg;
    *(--stack) = (uint64_t)entry;

    /* Push return address (thread_wrapper, not directly entry) */
    *(--stack) = (uint64_t)thread_wrapper;

    /* Push initial context (will be popped by switch) */
    *(--stack) = 0;                     /* r15 */
    *(--stack) = 0;                     /* r14 */
    *(--stack) = 0;                     /* r13 */
    *(--stack) = 0;                     /* r12 */
    *(--stack) = 0;                     /* rbx */
    *(--stack) = 0;                     /* rbp */

    /* Context points to saved registers */
    proc->context = (context_t *)stack;

    kprintf("[Scheduler] Created thread '%s' (PID %d)\n", proc->name, proc->pid);

    return proc;
}

/* Initialize scheduler */
void scheduler_init(void) {
    kprintf("[Scheduler] Initializing\n");

    /* Initialize process subsystem */
    process_init();

    /* Create idle process */
    idle_proc = kthread_create(idle_thread, NULL, "idle");
    if (!idle_proc) {
        kprintf("[Scheduler] ERROR: Failed to create idle process\n");
        return;
    }
    idle_proc->priority = 0;  /* Lowest priority */

    kprintf("[Scheduler] Initialized\n");
}

/* Schedule next process */
void schedule(void) {
    process_t *prev, *next;

    if (!scheduler_running) {
        return;
    }

    prev = current_proc;
    next = pick_next();

    if (next == prev) {
        return;
    }

    /* Put previous process back in ready queue if it was running */
    if (prev && prev->state == PROC_RUNNING && prev != idle_proc) {
        prev->state = PROC_READY;
        sched_ready(prev);
    }

    /* Switch to next process */
    next->state = PROC_RUNNING;
    current_proc = next;

    if (prev) {
        switch_context(&prev->context, next->context);
    } else {
        switch_to_new(next->context);
    }
}

/* Start the scheduler */
void scheduler_start(void) {
    if (scheduler_running) {
        return;
    }

    kprintf("[Scheduler] Starting\n");

    scheduler_running = 1;

    /* Set idle as current initially */
    current_proc = idle_proc;
    current_proc->state = PROC_RUNNING;

    /* If there are ready processes, switch to one */
    if (ready_queue_head) {
        schedule();
    }

    /* If we get here, just run idle */
    idle_thread(NULL);
}

/* Yield CPU */
void yield(void) {
    if (current_proc && current_proc->state == PROC_RUNNING) {
        schedule();
    }
}

/* Timer tick handler */
void scheduler_tick(void) {
    if (!scheduler_running || !current_proc) {
        return;
    }

    current_proc->total_ticks++;

    /* Decrease time slice */
    if (current_proc != idle_proc && current_proc->time_slice > 0) {
        current_proc->time_slice--;

        if (current_proc->time_slice == 0) {
            /* Reset time slice and reschedule */
            current_proc->time_slice = DEFAULT_TIME_SLICE;
            schedule();
        }
    }
}

/* Exit current process */
void process_exit(int exit_code) {
    process_t *proc = current_proc;

    if (!proc || proc == idle_proc) {
        return;
    }

    kprintf("[Scheduler] Process '%s' (PID %d) exited with code %d\n",
            proc->name, proc->pid, exit_code);

    proc->state = PROC_ZOMBIE;
    proc->exit_code = exit_code;

    /* Remove from ready queue if present */
    sched_remove(proc);

    /* Schedule next process */
    schedule();

    /* Should never reach here */
    while (1) {
        __asm__ volatile ("hlt");
    }
}
