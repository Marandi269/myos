/*
 * stdio.h - Standard I/O interface
 */

#ifndef _STDIO_H
#define _STDIO_H

#include "fd.h"

/*
 * Set up standard file descriptors for a process
 * Opens /dev/console and binds it to fd 0, 1, 2
 * Returns 0 on success, negative error on failure
 */
int setup_stdio(struct fd_table *table);

/*
 * Close standard file descriptors
 */
void teardown_stdio(struct fd_table *table);

#endif /* _STDIO_H */
