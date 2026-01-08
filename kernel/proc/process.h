/*
 * process.h - Process Control Block (PCB) definition
 */

#ifndef _PROCESS_H
#define _PROCESS_H

#include "types.h"

/* Process states */
typedef enum {
    PROC_UNUSED = 0,    /* Unused PCB slot */
    PROC_CREATED,       /* Just created */
    PROC_READY,         /* Ready to run */
    PROC_RUNNING,       /* Currently running */
    PROC_BLOCKED,       /* Waiting for something */
    PROC_ZOMBIE,        /* Exited, waiting for parent to reap */
} proc_state_t;

/* CPU context saved during context switch */
typedef struct {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rip;       /* Return address */
} __attribute__((packed)) context_t;

/* Process Control Block */
typedef struct process {
    /* Basic info */
    uint32_t pid;               /* Process ID */
    uint32_t ppid;              /* Parent process ID */
    char name[32];              /* Process name */
    proc_state_t state;         /* Process state */

    /* CPU context and stack */
    context_t *context;         /* Saved context (on kernel stack) */
    uint64_t kernel_stack;      /* Top of kernel stack */
    uint64_t kernel_stack_base; /* Base of kernel stack (for freeing) */

    /* Scheduling info */
    uint32_t priority;          /* Priority level */
    uint32_t time_slice;        /* Remaining time slice */
    uint64_t total_ticks;       /* Total ticks run */

    /* Process relationships */
    struct process *parent;     /* Parent process */
    struct process *next;       /* Next in queue */
    struct process *prev;       /* Previous in queue */

    /* Exit status */
    int exit_code;              /* Exit code */
} process_t;

/* Maximum number of processes */
#define MAX_PROCESSES       64

/* Default time slice in ticks (100Hz timer = 10ms per tick, 100 ticks = 1 second) */
#define DEFAULT_TIME_SLICE  10

/* Kernel stack size (8KB) */
#define KERNEL_STACK_SIZE   (8 * 1024)

/* Process table */
extern process_t proc_table[MAX_PROCESSES];
extern process_t *current_proc;

/* Process management functions */
void process_init(void);
process_t *process_alloc(void);
void process_free(process_t *proc);
process_t *process_get(uint32_t pid);

/* State helpers */
const char *process_state_name(proc_state_t state);

#endif /* _PROCESS_H */
