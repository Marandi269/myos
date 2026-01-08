/*
 * syscall.c - System call implementation
 *
 * Implements the system call framework and basic system calls.
 */

#include "syscall.h"
#include "process.h"
#include "scheduler.h"
#include "gdt.h"
#include "../lib/kprintf.h"
#include "../serial.h"
#include "../fs/vfs.h"
#include "../fs/fd.h"
#include "../fs/stdio.h"

/* Global fd table (for now - should be per-process) */
static struct fd_table *global_fd_table = NULL;

/* Get the current process's fd table */
static struct fd_table* get_fd_table(void) {
    /* TODO: should be current_proc->fd_table */
    return global_fd_table;
}

/* Read MSR */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

/* Write MSR */
static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

/* External syscall entry point (in syscall_asm.S) */
extern void syscall_entry(void);

/* System call table */
static syscall_fn_t syscall_table[MAX_SYSCALL];

/* Per-process brk value (simplified - global for now) */
static uint64_t current_brk = 0x400000;  /* Start heap at 4MB */

/*
 * Default handler for unimplemented syscalls
 */
static int64_t sys_unimplemented(uint64_t a1, uint64_t a2, uint64_t a3,
                                  uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return -ENOSYS;
}

/*
 * Register a system call handler
 */
void syscall_register(int num, syscall_fn_t handler) {
    if (num >= 0 && num < MAX_SYSCALL) {
        syscall_table[num] = handler;
    }
}

/*
 * Initialize SYSCALL/SYSRET
 */
void syscall_init(void) {
    uint64_t star, efer;
    int i;

    /* Initialize syscall table with default handler */
    for (i = 0; i < MAX_SYSCALL; i++) {
        syscall_table[i] = sys_unimplemented;
    }

    /* Register implemented syscalls */
    syscall_register(SYS_READ,   (syscall_fn_t)sys_read);
    syscall_register(SYS_WRITE,  (syscall_fn_t)sys_write);
    syscall_register(SYS_OPEN,   (syscall_fn_t)sys_open);
    syscall_register(SYS_CLOSE,  (syscall_fn_t)sys_close);
    syscall_register(SYS_LSEEK,  (syscall_fn_t)sys_lseek);
    syscall_register(SYS_BRK,    (syscall_fn_t)sys_brk);
    syscall_register(SYS_DUP,    (syscall_fn_t)sys_dup);
    syscall_register(SYS_DUP2,   (syscall_fn_t)sys_dup2);
    syscall_register(SYS_GETPID, (syscall_fn_t)sys_getpid);
    syscall_register(SYS_EXIT,   (syscall_fn_t)sys_exit);

    /*
     * STAR MSR layout:
     * Bits 32-47: SYSCALL CS (kernel) - CS = this value, SS = this value + 8
     * Bits 48-63: SYSRET base - CS = value + 16, SS = value + 8 (with RPL=3)
     *
     * Our GDT: 0x08=kcode, 0x10=kdata, 0x18=udata, 0x20=ucode
     * STAR[32:47] = 0x08 -> SYSCALL: CS=0x08, SS=0x10
     * STAR[48:63] = 0x10 -> SYSRET: CS=0x20|3=0x23, SS=0x18|3=0x1B
     */
    star = ((uint64_t)GDT_KERNEL_CODE << 32) | ((uint64_t)(GDT_KERNEL_DATA) << 48);
    wrmsr(MSR_STAR, star);

    /* LSTAR: syscall entry point */
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* SFMASK: clear IF (0x200) and DF (0x400) on syscall */
    wrmsr(MSR_SFMASK, 0x200 | 0x400);

    /* Enable SYSCALL/SYSRET in EFER */
    efer = rdmsr(MSR_EFER);
    wrmsr(MSR_EFER, efer | EFER_SCE);

    kprintf("[SYSCALL] Initialized with %d handlers\n", MAX_SYSCALL);
}

/*
 * Initialize syscall stdio (call after fs_init)
 */
void syscall_init_stdio(void) {
    /* Create global fd table */
    global_fd_table = fd_table_create();
    if (!global_fd_table) {
        kprintf("[SYSCALL] ERROR: Failed to create fd table\n");
        return;
    }

    /* Setup stdio (fd 0, 1, 2 -> /dev/console) */
    if (setup_stdio(global_fd_table) < 0) {
        kprintf("[SYSCALL] ERROR: Failed to setup stdio\n");
        return;
    }

    kprintf("[SYSCALL] stdio initialized (fd 0,1,2 -> /dev/console)\n");
}

/*
 * System call dispatcher - called from syscall_entry
 */
uint64_t syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2,
                         uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    if (num >= MAX_SYSCALL) {
        kprintf("[SYSCALL] Invalid syscall number: %d\n", (int)num);
        return -ENOSYS;
    }

    return syscall_table[num](arg1, arg2, arg3, arg4, arg5, 0);
}

/*
 * sys_read - Read from a file descriptor
 *
 * ssize_t read(int fd, void *buf, size_t count)
 */
int64_t sys_read(int fd, char *buf, size_t count) {
    struct fd_table *table;
    struct file *file;
    ssize_t ret;

    if (!buf) {
        return -EFAULT;
    }

    table = get_fd_table();
    if (!table) {
        return -EBADF;
    }

    file = fd_get(table, fd);
    if (!file) {
        return -EBADF;
    }

    ret = vfs_read(file, buf, count);
    return ret;
}

/*
 * sys_write - Write to a file descriptor
 *
 * ssize_t write(int fd, const void *buf, size_t count)
 */
int64_t sys_write(int fd, const char *buf, size_t count) {
    struct fd_table *table;
    struct file *file;
    ssize_t ret;

    if (!buf) {
        return -EFAULT;
    }

    table = get_fd_table();
    if (!table) {
        return -EBADF;
    }

    file = fd_get(table, fd);
    if (!file) {
        return -EBADF;
    }

    ret = vfs_write(file, buf, count);
    return ret;
}

/*
 * sys_brk - Change data segment size (heap management)
 *
 * void *brk(void *addr)
 *
 * If addr is 0, return current brk.
 * Otherwise, try to set brk to addr and return new brk.
 */
int64_t sys_brk(uint64_t addr) {
    uint64_t page_aligned;

    /* Query current brk */
    if (addr == 0) {
        return (int64_t)current_brk;
    }

    /* Set new brk (page-aligned) */
    page_aligned = (addr + 0xFFF) & ~0xFFFUL;

    /* Simple implementation - just update the value */
    /* TODO: Actually allocate/deallocate pages */
    if (page_aligned >= 0x400000 && page_aligned < 0x10000000) {
        current_brk = page_aligned;
        return (int64_t)current_brk;
    }

    /* Invalid address range */
    return (int64_t)current_brk;
}

/*
 * sys_getpid - Get process ID
 *
 * pid_t getpid(void)
 *
 * Returns the PID of the calling process.
 */
int64_t sys_getpid(void) {
    if (current_proc) {
        return (int64_t)current_proc->pid;
    }
    return 0;
}

/*
 * sys_exit - Terminate the calling process
 *
 * void exit(int status)
 *
 * Terminates the process and records exit status.
 * Does not return.
 */
int64_t sys_exit(int status) {
    kprintf("[SYSCALL] Process %d exiting with status %d\n",
            current_proc ? current_proc->pid : 0, status);

    if (current_proc) {
        current_proc->exit_code = status;
        current_proc->state = PROC_ZOMBIE;
    }

    /* Yield to scheduler - we won't return */
    schedule();

    /* Should never reach here */
    return 0;
}

/*
 * sys_open - Open a file
 *
 * int open(const char *pathname, int flags, mode_t mode)
 */
int64_t sys_open(const char *pathname, int flags, int mode) {
    struct fd_table *table;
    struct file *file;
    int fd;

    if (!pathname) {
        return -EFAULT;
    }

    table = get_fd_table();
    if (!table) {
        return -ENOMEM;
    }

    file = vfs_open(pathname, flags, mode);
    if (!file) {
        return -ENOENT;
    }

    fd = fd_alloc(table, file);
    if (fd < 0) {
        vfs_close(file);
        return -EMFILE;
    }

    return fd;
}

/*
 * sys_close - Close a file descriptor
 *
 * int close(int fd)
 */
int64_t sys_close(int fd) {
    struct fd_table *table;
    int ret;

    table = get_fd_table();
    if (!table) {
        return -EBADF;
    }

    ret = fd_free(table, fd);
    return ret;
}

/*
 * sys_lseek - Reposition file offset
 *
 * off_t lseek(int fd, off_t offset, int whence)
 */
int64_t sys_lseek(int fd, int64_t offset, int whence) {
    struct fd_table *table;
    struct file *file;
    int64_t ret;

    table = get_fd_table();
    if (!table) {
        return -EBADF;
    }

    file = fd_get(table, fd);
    if (!file) {
        return -EBADF;
    }

    ret = vfs_lseek(file, offset, whence);
    return ret;
}

/*
 * sys_dup - Duplicate a file descriptor
 *
 * int dup(int oldfd)
 */
int64_t sys_dup(int oldfd) {
    struct fd_table *table;
    int ret;

    table = get_fd_table();
    if (!table) {
        return -EBADF;
    }

    ret = fd_dup(table, oldfd);
    return ret;
}

/*
 * sys_dup2 - Duplicate a file descriptor to a specific fd
 *
 * int dup2(int oldfd, int newfd)
 */
int64_t sys_dup2(int oldfd, int newfd) {
    struct fd_table *table;
    int ret;

    table = get_fd_table();
    if (!table) {
        return -EBADF;
    }

    ret = fd_dup2(table, oldfd, newfd);
    return ret;
}
