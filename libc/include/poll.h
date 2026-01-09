/*
 * poll.h - Poll system call interface
 */

#ifndef _POLL_H
#define _POLL_H

#ifdef __cplusplus
extern "C" {
#endif

/* Poll event flags */
#define POLLIN      0x0001  /* Data available for reading */
#define POLLPRI     0x0002  /* Urgent data available */
#define POLLOUT     0x0004  /* Writing now will not block */
#define POLLERR     0x0008  /* Error condition */
#define POLLHUP     0x0010  /* Hang up */
#define POLLNVAL    0x0020  /* Invalid request: fd not open */
#define POLLRDNORM  0x0040  /* Normal data may be read */
#define POLLRDBAND  0x0080  /* Priority data may be read */
#define POLLWRNORM  0x0100  /* Writing normal data will not block */
#define POLLWRBAND  0x0200  /* Writing priority data will not block */

/* Number of file descriptors type */
typedef unsigned long nfds_t;

/* Poll file descriptor structure */
struct pollfd {
    int   fd;       /* File descriptor */
    short events;   /* Requested events */
    short revents;  /* Returned events */
};

/*
 * poll - wait for events on file descriptors
 *
 * @param fds      Array of pollfd structures
 * @param nfds     Number of file descriptors
 * @param timeout  Timeout in milliseconds (-1 = infinite, 0 = immediate)
 * @return         Number of ready fds, 0 on timeout, -1 on error
 */
int poll(struct pollfd *fds, nfds_t nfds, int timeout);

#ifdef __cplusplus
}
#endif

#endif /* _POLL_H */
