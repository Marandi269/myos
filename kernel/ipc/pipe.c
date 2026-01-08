/*
 * pipe.c - Pipe IPC implementation
 *
 * Implements anonymous pipes for parent-child process communication.
 * Uses a circular buffer with blocking semantics.
 */

#include "pipe.h"
#include "../mm/heap.h"
#include "../lib/kprintf.h"
#include "../lib/string.h"
#include "../fs/fd.h"
#include "../proc/process.h"
#include "../proc/scheduler.h"
#include "../proc/syscall.h"

/* Forward declarations */
static ssize_t pipe_file_read(struct file *file, char *buf, size_t count);
static ssize_t pipe_file_write(struct file *file, const char *buf, size_t count);
static int pipe_read_release(struct file *file);
static int pipe_write_release(struct file *file);

/* File operations for pipe read end */
struct file_operations pipe_read_fops = {
    .open = NULL,
    .close = pipe_read_release,
    .read = pipe_file_read,
    .write = NULL,
    .lseek = NULL,
    .readdir = NULL,
    .ioctl = NULL,
};

/* File operations for pipe write end */
struct file_operations pipe_write_fops = {
    .open = NULL,
    .close = pipe_write_release,
    .read = NULL,
    .write = pipe_file_write,
    .lseek = NULL,
    .readdir = NULL,
    .ioctl = NULL,
};

/*
 * Create a new pipe
 */
pipe_t *pipe_create(void) {
    pipe_t *pipe = kzalloc(sizeof(pipe_t));
    if (!pipe) {
        return NULL;
    }

    pipe->read_pos = 0;
    pipe->write_pos = 0;
    pipe->count = 0;
    pipe->readers = 1;
    pipe->writers = 1;
    pipe->read_closed = 0;
    pipe->write_closed = 0;

    return pipe;
}

/*
 * Destroy a pipe
 */
void pipe_destroy(pipe_t *pipe) {
    if (pipe) {
        kfree(pipe);
    }
}

/*
 * Read from pipe - blocking until data is available or write end closes
 */
static ssize_t pipe_file_read(struct file *file, char *buf, size_t count) {
    pipe_t *pipe;
    size_t bytes_read = 0;

    if (!file || !buf || !file->f_private) {
        return -EINVAL;
    }

    pipe = (pipe_t *)file->f_private;

    /* Read data from buffer */
    while (bytes_read < count) {
        /* Check if there's data to read */
        if (pipe->count > 0) {
            /* Read one byte at a time from circular buffer */
            buf[bytes_read++] = pipe->buffer[pipe->read_pos];
            pipe->read_pos = (pipe->read_pos + 1) % PIPE_BUF_SIZE;
            pipe->count--;
        } else {
            /* No data in buffer */
            if (pipe->write_closed || pipe->writers == 0) {
                /* Write end closed, return what we have (or 0 for EOF) */
                break;
            }

            if (bytes_read > 0) {
                /* We have some data, return it */
                break;
            }

            /* Block waiting for data (simplified - just yield) */
            /* In a full implementation, we would add to a wait queue */
            if (current_proc) {
                yield();
            } else {
                break;  /* Can't block in kernel context */
            }
        }
    }

    return bytes_read;
}

/*
 * Write to pipe - blocking if buffer is full
 */
static ssize_t pipe_file_write(struct file *file, const char *buf, size_t count) {
    pipe_t *pipe;
    size_t bytes_written = 0;

    if (!file || !buf || !file->f_private) {
        return -EINVAL;
    }

    pipe = (pipe_t *)file->f_private;

    /* Check if read end is closed */
    if (pipe->read_closed || pipe->readers == 0) {
        /* SIGPIPE should be sent here, for now just return error */
        kprintf("[PIPE] Write to pipe with no readers (SIGPIPE)\n");
        return -EPIPE;
    }

    /* Write data to buffer */
    while (bytes_written < count) {
        /* Check if there's space in buffer */
        if (pipe->count < PIPE_BUF_SIZE) {
            /* Write one byte at a time to circular buffer */
            pipe->buffer[pipe->write_pos] = buf[bytes_written++];
            pipe->write_pos = (pipe->write_pos + 1) % PIPE_BUF_SIZE;
            pipe->count++;
        } else {
            /* Buffer full */
            if (bytes_written > 0) {
                /* We wrote some data, return count */
                break;
            }

            /* Block waiting for space (simplified - just yield) */
            if (current_proc) {
                yield();
            } else {
                break;
            }
        }

        /* Re-check read end after each byte in case it closed */
        if (pipe->read_closed || pipe->readers == 0) {
            if (bytes_written > 0) {
                break;
            }
            return -EPIPE;
        }
    }

    return bytes_written;
}

/*
 * Close read end of pipe
 */
static int pipe_read_release(struct file *file) {
    pipe_t *pipe;

    if (!file || !file->f_private) {
        return -EINVAL;
    }

    pipe = (pipe_t *)file->f_private;
    pipe->readers--;
    pipe->read_closed = (pipe->readers == 0);

    kprintf("[PIPE] Read end closed (readers=%d)\n", pipe->readers);

    /* If both ends are closed, destroy the pipe */
    if (pipe->readers == 0 && pipe->writers == 0) {
        kprintf("[PIPE] Both ends closed, destroying pipe\n");
        pipe_destroy(pipe);
        file->f_private = NULL;
    }

    return 0;
}

/*
 * Close write end of pipe
 */
static int pipe_write_release(struct file *file) {
    pipe_t *pipe;

    if (!file || !file->f_private) {
        return -EINVAL;
    }

    pipe = (pipe_t *)file->f_private;
    pipe->writers--;
    pipe->write_closed = (pipe->writers == 0);

    kprintf("[PIPE] Write end closed (writers=%d)\n", pipe->writers);

    /* If both ends are closed, destroy the pipe */
    if (pipe->readers == 0 && pipe->writers == 0) {
        kprintf("[PIPE] Both ends closed, destroying pipe\n");
        pipe_destroy(pipe);
        file->f_private = NULL;
    }

    return 0;
}

/*
 * sys_pipe - Create a pipe
 *
 * pipefd[0] = read end
 * pipefd[1] = write end
 */
int sys_pipe(int pipefd[2]) {
    return sys_pipe2(pipefd, 0);
}

/*
 * sys_pipe2 - Create a pipe with flags
 */
int sys_pipe2(int pipefd[2], int flags) {
    pipe_t *pipe;
    struct file *read_file, *write_file;
    struct fd_table *table;
    int read_fd, write_fd;

    (void)flags;  /* Flags not implemented yet */

    if (!pipefd) {
        return -EFAULT;
    }

    /* Get fd table */
    if (current_proc && current_proc->fd_table) {
        table = current_proc->fd_table;
    } else {
        /* Need a global fd table as fallback */
        extern struct fd_table *global_fd_table;
        table = global_fd_table;
    }

    if (!table) {
        return -EMFILE;
    }

    /* Create pipe */
    pipe = pipe_create();
    if (!pipe) {
        return -ENOMEM;
    }

    /* Create read end file */
    read_file = file_alloc();
    if (!read_file) {
        pipe_destroy(pipe);
        return -ENOMEM;
    }
    read_file->f_op = &pipe_read_fops;
    read_file->f_private = pipe;
    read_file->f_flags = O_RDONLY;
    read_file->f_count = 1;

    /* Create write end file */
    write_file = file_alloc();
    if (!write_file) {
        file_free(read_file);
        pipe_destroy(pipe);
        return -ENOMEM;
    }
    write_file->f_op = &pipe_write_fops;
    write_file->f_private = pipe;
    write_file->f_flags = O_WRONLY;
    write_file->f_count = 1;

    /* Allocate file descriptors */
    read_fd = fd_alloc(table, read_file);
    if (read_fd < 0) {
        file_free(write_file);
        file_free(read_file);
        pipe_destroy(pipe);
        return -EMFILE;
    }

    write_fd = fd_alloc(table, write_file);
    if (write_fd < 0) {
        fd_free(table, read_fd);
        file_free(write_file);
        file_free(read_file);
        pipe_destroy(pipe);
        return -EMFILE;
    }

    pipefd[0] = read_fd;
    pipefd[1] = write_fd;

    kprintf("[PIPE] Created pipe: read_fd=%d, write_fd=%d\n", read_fd, write_fd);

    return 0;
}

/* Global fd table reference (defined in syscall.c) */
extern struct fd_table *global_fd_table;
