/*
 * stdio.c - Standard I/O implementation
 */

#include "stdio.h"
#include "vfs.h"
#include "../lib/kprintf.h"

/*
 * Set up standard file descriptors for a process
 */
int setup_stdio(struct fd_table *table) {
    struct file *console;

    if (!table) {
        return -EINVAL;
    }

    /* Open console for read/write */
    console = vfs_open("/dev/console", O_RDWR, 0);
    if (!console) {
        kprintf("[STDIO] Failed to open /dev/console\n");
        return -ENOENT;
    }

    /* stdin (fd 0) */
    if (fd_alloc_at(table, STDIN_FILENO, console) < 0) {
        vfs_close(console);
        return -ENOMEM;
    }

    /* stdout (fd 1) */
    if (fd_alloc_at(table, STDOUT_FILENO, console) < 0) {
        fd_free(table, STDIN_FILENO);
        vfs_close(console);
        return -ENOMEM;
    }

    /* stderr (fd 2) */
    if (fd_alloc_at(table, STDERR_FILENO, console) < 0) {
        fd_free(table, STDOUT_FILENO);
        fd_free(table, STDIN_FILENO);
        vfs_close(console);
        return -ENOMEM;
    }

    /* Release our reference - the fd table now owns the references */
    vfs_close(console);

    return 0;
}

/*
 * Close standard file descriptors
 */
void teardown_stdio(struct fd_table *table) {
    if (!table) {
        return;
    }

    fd_free(table, STDIN_FILENO);
    fd_free(table, STDOUT_FILENO);
    fd_free(table, STDERR_FILENO);
}
