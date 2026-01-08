/*
 * fd.h - File descriptor table interface
 */

#ifndef _FD_H
#define _FD_H

#include "vfs.h"

/* Maximum number of open file descriptors per process */
#define MAX_FD 256

/* Standard file descriptors */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* File descriptor table */
struct fd_table {
    struct file *fds[MAX_FD];
    int next_fd;  /* Hint for next available fd */
    int refcount; /* Reference count for sharing */
};

/* Create a new file descriptor table */
struct fd_table* fd_table_create(void);

/* Destroy a file descriptor table */
void fd_table_destroy(struct fd_table *table);

/* Clone a file descriptor table (for fork) */
struct fd_table* fd_table_clone(struct fd_table *table);

/* Copy file descriptor table (creates new independent copy) */
struct fd_table* fd_table_copy(struct fd_table *table);

/* Increment reference count (for thread sharing) */
void fd_table_ref(struct fd_table *table);

/* Decrement reference count, destroy if zero */
void fd_table_unref(struct fd_table *table);

/* Allocate a file descriptor for the given file */
int fd_alloc(struct fd_table *table, struct file *file);

/* Allocate a specific file descriptor */
int fd_alloc_at(struct fd_table *table, int fd, struct file *file);

/* Free a file descriptor */
int fd_free(struct fd_table *table, int fd);

/* Get the file for a file descriptor */
struct file* fd_get(struct fd_table *table, int fd);

/* Duplicate a file descriptor */
int fd_dup(struct fd_table *table, int oldfd);

/* Duplicate a file descriptor to a specific fd */
int fd_dup2(struct fd_table *table, int oldfd, int newfd);

/* Check if a file descriptor is valid */
int fd_is_valid(struct fd_table *table, int fd);

#endif /* _FD_H */
