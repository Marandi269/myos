/*
 * poll.c - Poll mechanism implementation
 *
 * Implements the core poll/select functionality for I/O multiplexing.
 */

#include "poll.h"
#include "vfs.h"
#include "fd.h"
#include "../proc/process.h"
#include "../proc/scheduler.h"
#include "../proc/syscall.h"
#include "../mm/heap.h"
#include "../lib/kprintf.h"
#include "../drivers/pit.h"

/* Ticks per millisecond (approximate: 100 Hz means 1 tick = 10ms) */
#define TICKS_PER_MS    1   /* Actually 0.1, but we use integer math */
#define MS_PER_TICK     10  /* Each tick is 10ms */

/*
 * Callback function to add wait queue to poll table
 */
static void poll_queue_proc(poll_table_t *pt, wait_queue_head_t *wq) {
    poll_table_entry_t *entry;

    if (!pt || !wq) return;

    /* Allocate new entry */
    entry = kzalloc(sizeof(poll_table_entry_t));
    if (!entry) {
        pt->error = -ENOMEM;
        return;
    }

    /* Initialize wait entry for current process */
    init_waitqueue_entry(&entry->wait_entry);

    /* Add to wait queue */
    entry->wait_queue = wq;
    add_wait_queue(wq, &entry->wait_entry);

    /* Add to poll table's list */
    entry->next = pt->entries;
    pt->entries = entry;
}

/*
 * Initialize a poll table
 */
void poll_table_init(poll_table_t *pt) {
    if (!pt) return;
    pt->queue_proc = poll_queue_proc;
    pt->entries = NULL;
    pt->error = 0;
}

/*
 * Free all entries in poll table
 * Removes from wait queues and frees memory
 */
void poll_table_free(poll_table_t *pt) {
    poll_table_entry_t *entry, *next;

    if (!pt) return;

    entry = pt->entries;
    while (entry) {
        next = entry->next;

        /* Remove from wait queue */
        if (entry->wait_queue) {
            remove_wait_queue(entry->wait_queue, &entry->wait_entry);
        }

        /* Free entry */
        kfree(entry);
        entry = next;
    }

    pt->entries = NULL;
}

/*
 * Register with a wait queue
 * Called by device poll functions to register interest
 */
void poll_wait(struct file *file, wait_queue_head_t *wq, poll_table_t *pt) {
    (void)file;  /* Not used, but passed for consistency with Linux API */

    if (pt && pt->queue_proc && wq) {
        pt->queue_proc(pt, wq);
    }
}

/*
 * Helper: get file from fd for current process
 */
struct file *poll_get_file(int fd) {
    struct fd_table *table;

    if (!current_proc || !current_proc->fd_table) {
        return NULL;
    }

    table = current_proc->fd_table;
    return fd_get(table, fd);
}

/*
 * Poll a single file descriptor
 * Returns the events that are ready
 */
static unsigned int do_poll_fd(struct file *file, short events, poll_table_t *pt) {
    unsigned int mask = 0;

    if (!file) {
        return POLLNVAL;
    }

    /* Check if file has poll operation */
    if (file->f_op && file->f_op->poll) {
        mask = file->f_op->poll(file, pt);
    } else {
        /* No poll support - assume always ready for read/write */
        mask = POLLIN | POLLOUT | POLLRDNORM | POLLWRNORM;
    }

    /* Filter by requested events, but always report errors */
    mask &= (events | POLLERR | POLLHUP | POLLNVAL);

    return mask;
}

/*
 * The core poll implementation
 */
int do_poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    poll_table_t pt;
    uint64_t deadline;
    int count;
    nfds_t i;
    int first_pass = 1;

    if (!fds && nfds > 0) {
        return -EFAULT;
    }

    /* Calculate deadline */
    if (timeout > 0) {
        deadline = pit_get_ticks() + (timeout / MS_PER_TICK) + 1;
    } else if (timeout == 0) {
        deadline = 0;  /* Immediate return */
    } else {
        deadline = (uint64_t)-1;  /* Infinite wait */
    }

    while (1) {
        /* Initialize poll table for first pass */
        if (first_pass) {
            poll_table_init(&pt);
        } else {
            /* On subsequent passes, don't register again */
            pt.queue_proc = NULL;
        }

        count = 0;

        /* Check all file descriptors */
        for (i = 0; i < nfds; i++) {
            struct file *file;
            unsigned int mask;

            fds[i].revents = 0;

            if (fds[i].fd < 0) {
                continue;
            }

            file = poll_get_file(fds[i].fd);
            if (!file) {
                fds[i].revents = POLLNVAL;
                count++;
                continue;
            }

            /* Poll this fd */
            mask = do_poll_fd(file, fds[i].events, first_pass ? &pt : NULL);
            fds[i].revents = mask;

            if (mask) {
                count++;
            }
        }

        first_pass = 0;

        /* Check if we have results */
        if (count > 0) {
            break;
        }

        /* Check for poll table errors */
        if (pt.error) {
            count = pt.error;
            break;
        }

        /* Check for timeout */
        if (timeout == 0) {
            break;
        }

        if (timeout > 0 && pit_get_ticks() >= deadline) {
            break;  /* Timed out */
        }

        /* Sleep waiting for events */
        if (current_proc) {
            current_proc->state = PROC_BLOCKED;
            schedule();
        } else {
            break;  /* No process context */
        }

        /* After waking up, check timeout again */
        if (timeout > 0 && pit_get_ticks() >= deadline) {
            /* Re-check fds one more time */
            count = 0;
            for (i = 0; i < nfds; i++) {
                struct file *file;
                unsigned int mask;

                fds[i].revents = 0;

                if (fds[i].fd < 0) continue;

                file = poll_get_file(fds[i].fd);
                if (!file) {
                    fds[i].revents = POLLNVAL;
                    count++;
                    continue;
                }

                mask = do_poll_fd(file, fds[i].events, NULL);
                fds[i].revents = mask;
                if (mask) count++;
            }
            break;
        }
    }

    /* Cleanup: remove from all wait queues */
    poll_table_free(&pt);

    return count;
}
