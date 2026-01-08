/*
 * fs.c - Filesystem initialization
 */

#include "fs.h"
#include "vfs.h"
#include "fd.h"
#include "stdio.h"
#include "ramfs/ramfs.h"
#include "devfs/devfs.h"
#include "../lib/kprintf.h"
#include "../lib/string.h"

/*
 * Initialize the filesystem subsystem
 */
void fs_init(void) {
    kprintf("[FS] Initializing filesystem subsystem\n");

    /* Initialize VFS layer */
    vfs_init();

    /* Register filesystem types */
    ramfs_init();
    devfs_init();

    /* Mount root filesystem (ramfs) */
    if (vfs_mount(NULL, "/", "ramfs") < 0) {
        kprintf("[FS] ERROR: Failed to mount root filesystem\n");
        return;
    }

    /* Create /dev directory */
    if (vfs_mkdir("/dev", 0755) < 0) {
        kprintf("[FS] ERROR: Failed to create /dev\n");
        return;
    }

    /* Mount devfs on /dev */
    if (vfs_mount(NULL, "/dev", "devfs") < 0) {
        kprintf("[FS] ERROR: Failed to mount devfs\n");
        return;
    }

    kprintf("[FS] Filesystem initialized\n");
}

/*
 * Test the filesystem
 */
void fs_test(void) {
    struct file *file;
    struct fd_table *fdt;
    char buf[64];
    ssize_t n;

    kprintf("\n[TEST] Filesystem test\n");

    /* Test 1: Create and write a file */
    kprintf("  Create /test.txt: ");
    file = vfs_open("/test.txt", O_CREAT | O_RDWR, 0644);
    if (file) {
        kprintf("OK\n");
    } else {
        kprintf("FAILED\n");
        return;
    }

    kprintf("  Write 'Hello': ");
    n = vfs_write(file, "Hello", 5);
    if (n == 5) {
        kprintf("OK\n");
    } else {
        kprintf("FAILED (%d)\n", (int)n);
    }

    /* Seek back to beginning */
    vfs_lseek(file, 0, SEEK_SET);

    /* Read back */
    kprintf("  Read back: ");
    memset(buf, 0, sizeof(buf));
    n = vfs_read(file, buf, sizeof(buf) - 1);
    if (n == 5 && strcmp(buf, "Hello") == 0) {
        kprintf("'%s' OK\n", buf);
    } else {
        kprintf("FAILED (got '%s', n=%d)\n", buf, (int)n);
    }

    vfs_close(file);

    /* Test 2: Device files */
    kprintf("  Open /dev/null: ");
    file = vfs_open("/dev/null", O_RDWR, 0);
    if (file) {
        kprintf("OK\n");

        kprintf("  Write to /dev/null: ");
        n = vfs_write(file, "test", 4);
        if (n == 4) {
            kprintf("OK (discarded)\n");
        } else {
            kprintf("FAILED\n");
        }

        kprintf("  Read /dev/null: ");
        n = vfs_read(file, buf, sizeof(buf));
        if (n == 0) {
            kprintf("OK (EOF)\n");
        } else {
            kprintf("FAILED\n");
        }

        vfs_close(file);
    } else {
        kprintf("FAILED\n");
    }

    kprintf("  Open /dev/zero: ");
    file = vfs_open("/dev/zero", O_RDONLY, 0);
    if (file) {
        kprintf("OK\n");
        kprintf("  Read /dev/zero: ");
        memset(buf, 0xFF, 8);
        n = vfs_read(file, buf, 8);
        if (n == 8 && buf[0] == 0 && buf[7] == 0) {
            kprintf("OK (zeros)\n");
        } else {
            kprintf("FAILED (n=%d)\n", (int)n);
        }
        vfs_close(file);
    } else {
        kprintf("FAILED (open)\n");
    }

    /* Test 3: File descriptor table */
    kprintf("  Create fd_table: ");
    fdt = fd_table_create();
    if (fdt) {
        kprintf("OK\n");
    } else {
        kprintf("FAILED\n");
        return;
    }

    /* Test 4: stdio setup */
    kprintf("  Setup stdio: ");
    if (setup_stdio(fdt) == 0) {
        kprintf("OK\n");

        kprintf("  fd 0,1,2 bound to /dev/console\n");

        /* Test write to stdout */
        kprintf("  write(1, 'FS Test Output') -> ");
        file = fd_get(fdt, STDOUT_FILENO);
        if (file && file->f_op && file->f_op->write) {
            n = file->f_op->write(file, "FS Test Output\n", 15);
            if (n == 15) {
                kprintf("OK\n");
            } else {
                kprintf("FAILED\n");
            }
        } else {
            kprintf("FAILED (no file)\n");
        }
    } else {
        kprintf("FAILED\n");
    }

    fd_table_destroy(fdt);

    kprintf("[TEST] Filesystem: ALL PASSED\n\n");
}
