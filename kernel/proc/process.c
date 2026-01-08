/*
 * process.c - Process management implementation
 */

#include "process.h"
#include "mm/pmm.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Process table */
process_t proc_table[MAX_PROCESSES];

/* Current running process */
process_t *current_proc = NULL;

/* Next PID to allocate */
static uint32_t next_pid = 0;

/* State names for debugging */
static const char *state_names[] = {
    "UNUSED",
    "CREATED",
    "READY",
    "RUNNING",
    "BLOCKED",
    "ZOMBIE"
};

/* Get state name */
const char *process_state_name(proc_state_t state) {
    if (state < sizeof(state_names) / sizeof(state_names[0])) {
        return state_names[state];
    }
    return "UNKNOWN";
}

/* Initialize process subsystem */
void process_init(void) {
    int i;

    /* Clear process table */
    memset(proc_table, 0, sizeof(proc_table));

    for (i = 0; i < MAX_PROCESSES; i++) {
        proc_table[i].state = PROC_UNUSED;
        proc_table[i].pid = 0;
    }

    next_pid = 0;
    current_proc = NULL;

    kprintf("[Process] Initialized (max %d processes)\n", MAX_PROCESSES);
}

/* Allocate a new process */
process_t *process_alloc(void) {
    int i;
    process_t *proc = NULL;
    void *stack;

    /* Find an unused slot */
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state == PROC_UNUSED) {
            proc = &proc_table[i];
            break;
        }
    }

    if (!proc) {
        kprintf("[Process] ERROR: No free process slots\n");
        return NULL;
    }

    /* Allocate kernel stack (2 pages = 8KB) */
    stack = pmm_alloc_page();
    if (!stack) {
        kprintf("[Process] ERROR: Failed to allocate kernel stack\n");
        return NULL;
    }
    pmm_alloc_page();  /* Allocate second page */

    /* Initialize PCB */
    memset(proc, 0, sizeof(process_t));
    proc->pid = next_pid++;
    proc->state = PROC_CREATED;
    proc->priority = 1;
    proc->time_slice = DEFAULT_TIME_SLICE;

    /* Set up kernel stack */
    proc->kernel_stack_base = (uint64_t)stack;
    proc->kernel_stack = proc->kernel_stack_base + KERNEL_STACK_SIZE;

    /* Initialize filesystem state */
    proc->fd_table = NULL;
    strncpy(proc->cwd, "/", sizeof(proc->cwd));

    /* Initialize process relationships */
    proc->parent = NULL;
    proc->children = NULL;
    proc->sibling = NULL;
    proc->wait_next = NULL;
    proc->exited = 0;

    return proc;
}

/* Forward declaration */
void fd_table_destroy(struct fd_table *table);
void free_user_address_space(uint64_t *pml4);

/* Free a process */
void process_free(process_t *proc) {
    if (!proc || proc->state == PROC_UNUSED) {
        return;
    }

    /* Free kernel stack */
    if (proc->kernel_stack_base) {
        pmm_free_page((void *)proc->kernel_stack_base);
        pmm_free_page((void *)(proc->kernel_stack_base + PAGE_SIZE));
    }

    /* Free file descriptor table */
    if (proc->fd_table) {
        fd_table_destroy(proc->fd_table);
        proc->fd_table = NULL;
    }

    /* Free user address space */
    if (proc->page_table) {
        free_user_address_space(proc->page_table);
        proc->page_table = NULL;
    }

    /* Remove from parent's children list */
    if (proc->parent) {
        process_t **pp = &proc->parent->children;
        while (*pp) {
            if (*pp == proc) {
                *pp = proc->sibling;
                break;
            }
            pp = &(*pp)->sibling;
        }
    }

    /* Clear PCB */
    memset(proc, 0, sizeof(process_t));
    proc->state = PROC_UNUSED;
}

/* Get process by PID */
process_t *process_get(uint32_t pid) {
    int i;

    for (i = 0; i < MAX_PROCESSES; i++) {
        if (proc_table[i].state != PROC_UNUSED && proc_table[i].pid == pid) {
            return &proc_table[i];
        }
    }

    return NULL;
}
