/*
 * pipe.h - Pipe IPC mechanism
 */

#ifndef _PIPE_H
#define _PIPE_H

#include "types.h"
#include "fs/vfs.h"

/* Pipe buffer size */
#define PIPE_BUF_SIZE 4096

/* Pipe structure */
typedef struct pipe {
    char buffer[PIPE_BUF_SIZE];
    size_t read_pos;        /* Read position in buffer */
    size_t write_pos;       /* Write position in buffer */
    size_t count;           /* Number of bytes in buffer */
    int readers;            /* Number of read end references */
    int writers;            /* Number of write end references */
    int read_closed;        /* Read end is closed */
    int write_closed;       /* Write end is closed */
} pipe_t;

/* Pipe file operations */
extern struct file_operations pipe_read_fops;
extern struct file_operations pipe_write_fops;

/* Create a new pipe */
pipe_t *pipe_create(void);

/* Destroy a pipe */
void pipe_destroy(pipe_t *pipe);

/* Pipe system call - creates fd pair */
int sys_pipe(int pipefd[2]);
int sys_pipe2(int pipefd[2], int flags);

/* Pipe read/write operations */
ssize_t pipe_read(struct file *file, char *buf, size_t count);
ssize_t pipe_write(struct file *file, const char *buf, size_t count);

/* Pipe close operations */
int pipe_read_close(struct file *file);
int pipe_write_close(struct file *file);

#endif /* _PIPE_H */
