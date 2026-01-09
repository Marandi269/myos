/*
 * select.c - select() and poll() system call implementation
 *
 * Implements I/O multiplexing system calls.
 */

#include "poll.h"
#include "vfs.h"
#include "fd.h"
#include "../proc/process.h"
#include "../proc/scheduler.h"
#include "../proc/syscall.h"
#include "../lib/kprintf.h"
#include "../drivers/pit.h"

/* Milliseconds per tick */
#define MS_PER_TICK 10

/*
 * sys_poll - Poll file descriptors for events
 *
 * int poll(struct pollfd *fds, nfds_t nfds, int timeout);
 *
 * @param fds      Array of pollfd structures
 * @param nfds     Number of file descriptors to poll
 * @param timeout  Timeout in milliseconds (-1 = infinite, 0 = immediate)
 * @return         Number of ready fds, 0 on timeout, negative on error
 */
int64_t sys_poll(struct pollfd *fds, uint64_t nfds, int timeout) {
    if (!fds && nfds > 0) {
        return -EFAULT;
    }

    if (nfds > FD_SETSIZE) {
        return -EINVAL;
    }

    return do_poll(fds, (nfds_t)nfds, timeout);
}

/*
 * sys_select - Synchronous I/O multiplexing
 *
 * int select(int nfds, fd_set *readfds, fd_set *writefds,
 *            fd_set *exceptfds, struct timeval *timeout);
 *
 * @param nfds       Highest-numbered fd + 1
 * @param readfds    Set of fds to watch for reading
 * @param writefds   Set of fds to watch for writing
 * @param exceptfds  Set of fds to watch for exceptions
 * @param timeout    Maximum time to wait (NULL = infinite)
 * @return           Total number of ready fds, 0 on timeout, negative on error
 */
int64_t sys_select(int nfds, fd_set *readfds, fd_set *writefds,
                   fd_set *exceptfds, struct timeval *timeout) {
    struct pollfd *pfd = NULL;
    int npfd = 0;
    int timeout_ms;
    int ret;
    int fd;

    /* Validate nfds */
    if (nfds < 0 || nfds > FD_SETSIZE) {
        return -EINVAL;
    }

    /* Convert timeout to milliseconds */
    if (timeout) {
        if (timeout->tv_sec < 0 || timeout->tv_usec < 0) {
            return -EINVAL;
        }
        timeout_ms = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
    } else {
        timeout_ms = -1;  /* Infinite */
    }

    /* Count the number of fds we need to poll */
    for (fd = 0; fd < nfds; fd++) {
        int events = 0;
        if (readfds && FD_ISSET(fd, readfds))   events |= POLLIN;
        if (writefds && FD_ISSET(fd, writefds)) events |= POLLOUT;
        if (exceptfds && FD_ISSET(fd, exceptfds)) events |= POLLPRI;
        if (events) {
            npfd++;
        }
    }

    /* No fds to poll - just sleep for timeout */
    if (npfd == 0) {
        if (timeout_ms > 0 && current_proc) {
            uint64_t deadline = pit_get_ticks() + (timeout_ms / MS_PER_TICK) + 1;
            while (pit_get_ticks() < deadline) {
                current_proc->state = PROC_BLOCKED;
                schedule();
            }
        }
        return 0;
    }

    /* Allocate pollfd array on stack if small, otherwise reject */
    if (npfd > 64) {
        return -EINVAL;  /* Too many fds for stack allocation */
    }

    struct pollfd pfd_array[64];
    pfd = pfd_array;

    /* Convert fd_sets to pollfd array */
    npfd = 0;
    for (fd = 0; fd < nfds; fd++) {
        int events = 0;
        if (readfds && FD_ISSET(fd, readfds))   events |= POLLIN;
        if (writefds && FD_ISSET(fd, writefds)) events |= POLLOUT;
        if (exceptfds && FD_ISSET(fd, exceptfds)) events |= POLLPRI;

        if (events) {
            pfd[npfd].fd = fd;
            pfd[npfd].events = events;
            pfd[npfd].revents = 0;
            npfd++;
        }
    }

    /* Call do_poll */
    ret = do_poll(pfd, npfd, timeout_ms);

    if (ret < 0) {
        return ret;
    }

    /* Convert results back to fd_sets */
    if (readfds) FD_ZERO(readfds);
    if (writefds) FD_ZERO(writefds);
    if (exceptfds) FD_ZERO(exceptfds);

    int count = 0;
    for (int i = 0; i < npfd; i++) {
        if (pfd[i].revents == 0) {
            continue;
        }

        if (readfds && (pfd[i].revents & (POLLIN | POLLHUP | POLLERR))) {
            FD_SET(pfd[i].fd, readfds);
        }
        if (writefds && (pfd[i].revents & (POLLOUT | POLLERR))) {
            FD_SET(pfd[i].fd, writefds);
        }
        if (exceptfds && (pfd[i].revents & (POLLPRI | POLLNVAL))) {
            FD_SET(pfd[i].fd, exceptfds);
        }

        if (pfd[i].revents) {
            count++;
        }
    }

    return count;
}
