/*
 * sys/select.h - Select system call interface
 */

#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* Maximum number of file descriptors */
#define FD_SETSIZE 256

/* fd_set type */
typedef struct {
    unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(unsigned long))];
} fd_set;

/* fd_set manipulation macros */
#define __FDELT(fd)     ((fd) / (8 * sizeof(unsigned long)))
#define __FDMASK(fd)    (1UL << ((fd) % (8 * sizeof(unsigned long))))

#define FD_ZERO(set)    do { \
    unsigned int __i; \
    for (__i = 0; __i < sizeof((set)->fds_bits)/sizeof((set)->fds_bits[0]); __i++) \
        (set)->fds_bits[__i] = 0; \
} while (0)

#define FD_SET(fd, set)   ((set)->fds_bits[__FDELT(fd)] |= __FDMASK(fd))
#define FD_CLR(fd, set)   ((set)->fds_bits[__FDELT(fd)] &= ~__FDMASK(fd))
#define FD_ISSET(fd, set) (((set)->fds_bits[__FDELT(fd)] & __FDMASK(fd)) != 0)

/* Time structures */
struct timeval {
    long tv_sec;    /* Seconds */
    long tv_usec;   /* Microseconds */
};

struct timespec {
    long tv_sec;    /* Seconds */
    long tv_nsec;   /* Nanoseconds */
};

/*
 * select - synchronous I/O multiplexing
 *
 * @param nfds       Highest-numbered fd + 1
 * @param readfds    Set of fds to watch for reading
 * @param writefds   Set of fds to watch for writing
 * @param exceptfds  Set of fds to watch for exceptions
 * @param timeout    Maximum time to wait (NULL = infinite)
 * @return           Total number of ready fds, 0 on timeout, -1 on error
 */
int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout);

/*
 * pselect - synchronous I/O multiplexing with signal mask
 *
 * @param nfds       Highest-numbered fd + 1
 * @param readfds    Set of fds to watch for reading
 * @param writefds   Set of fds to watch for writing
 * @param exceptfds  Set of fds to watch for exceptions
 * @param timeout    Maximum time to wait (NULL = infinite)
 * @param sigmask    Signal mask to apply during wait
 * @return           Total number of ready fds, 0 on timeout, -1 on error
 */
int pselect(int nfds, fd_set *readfds, fd_set *writefds,
            fd_set *exceptfds, const struct timespec *timeout,
            const void *sigmask);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SELECT_H */
