/*
 * fd.c - File descriptor table implementation
 */

#include "fd.h"
#include "../mm/heap.h"
#include "../lib/string.h"

/*
 * Create a new file descriptor table
 */
struct fd_table* fd_table_create(void) {
    struct fd_table *table = kzalloc(sizeof(struct fd_table));
    if (table) {
        table->next_fd = 0;
    }
    return table;
}

/*
 * Destroy a file descriptor table
 */
void fd_table_destroy(struct fd_table *table) {
    int i, j;
    struct file *file;
    int ref_count;

    if (!table) {
        return;
    }

    /* Close all open files - count references to same file first,
     * then call file_put the correct number of times */
    for (i = 0; i < MAX_FD; i++) {
        file = table->fds[i];
        if (file) {
            /* Count how many fds point to this file */
            ref_count = 1;
            table->fds[i] = NULL;

            for (j = i + 1; j < MAX_FD; j++) {
                if (table->fds[j] == file) {
                    ref_count++;
                    table->fds[j] = NULL;
                }
            }

            /* Release all references */
            while (ref_count-- > 0) {
                file_put(file);
            }
        }
    }

    kfree(table);
}

/*
 * Clone a file descriptor table (for fork)
 */
struct fd_table* fd_table_clone(struct fd_table *table) {
    struct fd_table *new_table;
    int i;

    if (!table) {
        return NULL;
    }

    new_table = fd_table_create();
    if (!new_table) {
        return NULL;
    }

    /* Duplicate all file descriptors */
    for (i = 0; i < MAX_FD; i++) {
        if (table->fds[i]) {
            new_table->fds[i] = table->fds[i];
            file_get(new_table->fds[i]);
        }
    }

    new_table->next_fd = table->next_fd;
    return new_table;
}

/*
 * Allocate a file descriptor for the given file
 * Returns the fd number on success, -1 on failure
 */
int fd_alloc(struct fd_table *table, struct file *file) {
    int fd;

    if (!table || !file) {
        return -EINVAL;
    }

    /* Start searching from the hint */
    for (fd = table->next_fd; fd < MAX_FD; fd++) {
        if (!table->fds[fd]) {
            table->fds[fd] = file;
            file_get(file);
            table->next_fd = fd + 1;
            return fd;
        }
    }

    /* Wrap around if needed */
    for (fd = 0; fd < table->next_fd; fd++) {
        if (!table->fds[fd]) {
            table->fds[fd] = file;
            file_get(file);
            table->next_fd = fd + 1;
            return fd;
        }
    }

    return -EMFILE;  /* Too many open files */
}

/*
 * Allocate a specific file descriptor
 */
int fd_alloc_at(struct fd_table *table, int fd, struct file *file) {
    if (!table || !file) {
        return -EINVAL;
    }

    if (fd < 0 || fd >= MAX_FD) {
        return -EBADF;
    }

    /* Close existing file if any */
    if (table->fds[fd]) {
        file_put(table->fds[fd]);
    }

    table->fds[fd] = file;
    file_get(file);
    return fd;
}

/*
 * Free a file descriptor
 */
int fd_free(struct fd_table *table, int fd) {
    if (!table) {
        return -EINVAL;
    }

    if (fd < 0 || fd >= MAX_FD) {
        return -EBADF;
    }

    if (!table->fds[fd]) {
        return -EBADF;
    }

    file_put(table->fds[fd]);
    table->fds[fd] = NULL;

    /* Update hint */
    if (fd < table->next_fd) {
        table->next_fd = fd;
    }

    return 0;
}

/*
 * Get the file for a file descriptor
 */
struct file* fd_get(struct fd_table *table, int fd) {
    if (!table) {
        return NULL;
    }

    if (fd < 0 || fd >= MAX_FD) {
        return NULL;
    }

    return table->fds[fd];
}

/*
 * Duplicate a file descriptor
 * Returns new fd on success, -1 on failure
 */
int fd_dup(struct fd_table *table, int oldfd) {
    struct file *file;

    if (!table) {
        return -EINVAL;
    }

    if (oldfd < 0 || oldfd >= MAX_FD) {
        return -EBADF;
    }

    file = table->fds[oldfd];
    if (!file) {
        return -EBADF;
    }

    return fd_alloc(table, file);
}

/*
 * Duplicate a file descriptor to a specific fd
 */
int fd_dup2(struct fd_table *table, int oldfd, int newfd) {
    struct file *file;

    if (!table) {
        return -EINVAL;
    }

    if (oldfd < 0 || oldfd >= MAX_FD || newfd < 0 || newfd >= MAX_FD) {
        return -EBADF;
    }

    file = table->fds[oldfd];
    if (!file) {
        return -EBADF;
    }

    /* If same fd, just return it */
    if (oldfd == newfd) {
        return newfd;
    }

    /* Close newfd if open */
    if (table->fds[newfd]) {
        file_put(table->fds[newfd]);
    }

    table->fds[newfd] = file;
    file_get(file);
    return newfd;
}

/*
 * Check if a file descriptor is valid
 */
int fd_is_valid(struct fd_table *table, int fd) {
    if (!table) {
        return 0;
    }

    if (fd < 0 || fd >= MAX_FD) {
        return 0;
    }

    return table->fds[fd] != NULL;
}
