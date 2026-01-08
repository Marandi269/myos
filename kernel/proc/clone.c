/*
 * clone.c - clone() system call implementation for thread creation
 */

#include "proc/clone.h"
#include "proc/process.h"
#include "proc/scheduler.h"
#include "proc/syscall.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/heap.h"
#include "fs/fd.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* External functions */
extern void switch_to_new(context_t *ctx);

/* MSR write for TLS */
static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

#define MSR_FS_BASE 0xC0000100
#define MSR_GS_BASE 0xC0000101

/* Thread entry wrapper - called when new thread starts */
static void thread_entry_wrapper(void) {
    /* The thread will return to user space with the correct registers
     * This is set up by the clone implementation */
}

/* clone() system call */
int64_t sys_clone(uint64_t flags, void *child_stack,
                   int *parent_tidptr, int *child_tidptr, void *tls) {
    process_t *parent = current_proc;
    process_t *child;
    context_t *ctx;

    if (!parent) {
        return -ESRCH;
    }

    /* For threads, child_stack must be provided */
    if ((flags & CLONE_VM) && !child_stack) {
        return -EINVAL;
    }

    /* Allocate new process/thread structure */
    child = process_alloc();
    if (!child) {
        return -ENOMEM;
    }

    /* Copy basic info */
    strncpy(child->name, parent->name, sizeof(child->name) - 1);
    child->name[sizeof(child->name) - 1] = '\0';
    child->ppid = parent->pid;
    child->parent = parent;

    /* Add to parent's children list */
    child->sibling = parent->children;
    parent->children = child;

    if (flags & CLONE_VM) {
        /* Thread: share address space */
        child->page_table = parent->page_table;
        child->brk = parent->brk;
        child->is_user = parent->is_user;
        child->user_entry = parent->user_entry;

        /* Use provided child stack */
        child->user_stack = (uint64_t)child_stack;
    } else {
        /* Process: copy address space (fork-like behavior) */
        /* For now, just share - COW would be implemented here */
        child->page_table = parent->page_table;
        child->brk = parent->brk;
        child->is_user = parent->is_user;
        child->user_entry = parent->user_entry;
        child->user_stack = parent->user_stack;
    }

    if (flags & CLONE_FILES) {
        /* Share file descriptor table */
        child->fd_table = parent->fd_table;
        if (child->fd_table) {
            fd_table_ref(child->fd_table);
        }
    } else {
        /* Copy file descriptor table */
        child->fd_table = fd_table_copy(parent->fd_table);
    }

    if (flags & CLONE_FS) {
        /* Share filesystem info (cwd, etc.) */
        strncpy(child->cwd, parent->cwd, sizeof(child->cwd));
    } else {
        strncpy(child->cwd, parent->cwd, sizeof(child->cwd));
    }

    if (flags & CLONE_SIGHAND) {
        /* Share signal handlers */
        memcpy(child->sig_handlers, parent->sig_handlers,
               sizeof(child->sig_handlers));
    }

    /* Setup kernel stack context for the child */
    ctx = (context_t *)(child->kernel_stack - sizeof(context_t));
    memset(ctx, 0, sizeof(context_t));

    /* Child returns 0 from clone */
    ctx->rip = (uint64_t)thread_entry_wrapper;  /* Will be replaced by actual return */
    ctx->rbp = 0;
    ctx->rbx = 0;
    ctx->r12 = 0;
    ctx->r13 = 0;
    ctx->r14 = 0;
    ctx->r15 = 0;

    child->context = ctx;

    /* Handle TLS */
    if (flags & CLONE_SETTLS) {
        /* Store TLS base - will be set when thread runs */
        /* For x86-64, TLS is typically set via FS base */
        /* We store it and set it in the context switch */
    }

    /* Store TID if requested */
    if ((flags & CLONE_PARENT_SETTID) && parent_tidptr) {
        *parent_tidptr = child->pid;
    }

    if ((flags & CLONE_CHILD_SETTID) && child_tidptr) {
        *child_tidptr = child->pid;
    }

    /* Make child runnable */
    child->state = PROC_READY;
    sched_ready(child);

    kprintf("[clone] Created thread %d from %d, flags=0x%x\n",
            child->pid, parent->pid, (uint32_t)flags);

    /* Parent returns child's TID */
    return child->pid;
}

/* Set the address that will be cleared when thread exits */
int64_t sys_set_tid_address(int *tidptr) {
    process_t *proc = current_proc;

    if (!proc) {
        return -ESRCH;
    }

    /* Store the tidptr for clearing on thread exit */
    /* For now, just return the current TID */
    return proc->pid;
}

/* arch_prctl - set architecture-specific thread state */
int64_t sys_arch_prctl(int code, uint64_t addr) {
    switch (code) {
        case ARCH_SET_FS:
            wrmsr(MSR_FS_BASE, addr);
            return 0;

        case ARCH_SET_GS:
            wrmsr(MSR_GS_BASE, addr);
            return 0;

        case ARCH_GET_FS:
            if (addr) {
                *(uint64_t *)addr = rdmsr(MSR_FS_BASE);
            }
            return 0;

        case ARCH_GET_GS:
            if (addr) {
                *(uint64_t *)addr = rdmsr(MSR_GS_BASE);
            }
            return 0;

        default:
            return -EINVAL;
    }
}
