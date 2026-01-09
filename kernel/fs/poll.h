/*
 * poll.h - Poll mechanism for I/O multiplexing
 *
 * Provides the poll_table structure and poll event definitions
 * used by select() and poll() system calls.
 */

#ifndef _POLL_H
#define _POLL_H

#include "types.h"
#include "../proc/wait_queue.h"

/* Forward declarations */
struct file;

/*
 * Poll event flags (compatible with POSIX)
 */
#define POLLIN      0x0001  /* Data available for reading */
#define POLLPRI     0x0002  /* Urgent data available */
#define POLLOUT     0x0004  /* Writing now will not block */
#define POLLERR     0x0008  /* Error condition */
#define POLLHUP     0x0010  /* Hang up */
#define POLLNVAL    0x0020  /* Invalid request: fd not open */

/* Additional flags */
#define POLLRDNORM  0x0040  /* Normal data may be read */
#define POLLRDBAND  0x0080  /* Priority data may be read */
#define POLLWRNORM  0x0100  /* Writing normal data will not block */
#define POLLWRBAND  0x0200  /* Writing priority data will not block */

/*
 * Poll table entry - tracks one wait queue registration
 */
typedef struct poll_table_entry {
    wait_queue_head_t *wait_queue;      /* The wait queue */
    wait_queue_entry_t wait_entry;      /* Our entry in that queue */
    struct poll_table_entry *next;      /* Next entry in our list */
} poll_table_entry_t;

/*
 * Poll table - collects wait queues from all polled fds
 */
typedef struct poll_table {
    /* Callback to register with wait queue */
    void (*queue_proc)(struct poll_table *pt, wait_queue_head_t *wq);

    /* Linked list of poll table entries */
    poll_table_entry_t *entries;

    /* Error flag */
    int error;
} poll_table_t;

/* Note: struct pollfd and nfds_t are defined in syscall.h */
struct pollfd;
typedef unsigned long nfds_t;

/*
 * Initialize a poll table
 */
void poll_table_init(poll_table_t *pt);

/*
 * Free all entries in poll table
 */
void poll_table_free(poll_table_t *pt);

/*
 * Register with a wait queue (called from device poll functions)
 * This is the "poll_wait" function used in Linux kernel
 */
void poll_wait(struct file *file, wait_queue_head_t *wq, poll_table_t *pt);

/*
 * The core poll implementation
 * Polls a set of file descriptors and optionally sleeps until events occur
 *
 * @param fds       Array of pollfd structures
 * @param nfds      Number of file descriptors
 * @param timeout   Timeout in milliseconds (-1 = infinite, 0 = immediate)
 * @return          Number of ready fds, 0 on timeout, -1 on error
 */
int do_poll(struct pollfd *fds, nfds_t nfds, int timeout);

/*
 * VFS poll function type
 * Returns bitmask of poll events that are ready
 */
typedef unsigned int (*poll_fn_t)(struct file *file, poll_table_t *pt);

/*
 * Helper: get file from fd for current process
 */
struct file *poll_get_file(int fd);

#endif /* _POLL_H */
