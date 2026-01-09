/*
 * select.c - poll() and select() wrapper functions
 */

#include <poll.h>
#include <sys/select.h>
#include <syscall.h>

/*
 * poll - wait for events on file descriptors
 */
int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    return (int)syscall3(SYS_poll, (long)fds, (long)nfds, (long)timeout);
}

/*
 * select - synchronous I/O multiplexing
 */
int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout) {
    return (int)syscall5(SYS_select, (long)nfds, (long)readfds,
                         (long)writefds, (long)exceptfds, (long)timeout);
}

/*
 * pselect - synchronous I/O multiplexing with signal mask
 * Note: Simplified implementation that ignores sigmask
 */
int pselect(int nfds, fd_set *readfds, fd_set *writefds,
            fd_set *exceptfds, const struct timespec *timeout,
            const void *sigmask) {
    struct timeval tv;

    (void)sigmask;  /* Not implemented */

    if (timeout) {
        tv.tv_sec = timeout->tv_sec;
        tv.tv_usec = timeout->tv_nsec / 1000;
        return select(nfds, readfds, writefds, exceptfds, &tv);
    }

    return select(nfds, readfds, writefds, exceptfds, NULL);
}
