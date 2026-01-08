/*
 * syscall.c - System call implementation
 *
 * Implements the system call framework and basic system calls.
 */

#include "syscall.h"
#include "process.h"
#include "scheduler.h"
#include "gdt.h"
#include "user_space.h"
#include "../lib/kprintf.h"
#include "../lib/string.h"
#include "../serial.h"
#include "../fs/vfs.h"
#include "../fs/fd.h"
#include "../fs/stdio.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../ipc/pipe.h"
#include "../ipc/signal.h"
#include "../ipc/shm.h"

/* Global fd table (for now - should be per-process) */
struct fd_table *global_fd_table = NULL;

/* Get the current process's fd table */
static struct fd_table* get_fd_table(void) {
    if (current_proc && current_proc->fd_table) {
        return current_proc->fd_table;
    }
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

/* Default brk value for processes without one set */
#define DEFAULT_BRK 0x1000000  /* Start heap at 16MB */

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
    syscall_register(SYS_GETPPID,(syscall_fn_t)sys_getppid);
    syscall_register(SYS_EXIT,   (syscall_fn_t)sys_exit);
    syscall_register(SYS_FORK,   (syscall_fn_t)sys_fork);
    syscall_register(SYS_EXECVE, (syscall_fn_t)sys_execve);
    syscall_register(SYS_WAIT4,  (syscall_fn_t)sys_wait4);
    syscall_register(SYS_GETCWD, (syscall_fn_t)sys_getcwd);
    syscall_register(SYS_CHDIR,  (syscall_fn_t)sys_chdir);
    syscall_register(SYS_MKDIR,  (syscall_fn_t)sys_mkdir);
    syscall_register(SYS_GETDENTS64, (syscall_fn_t)sys_getdents64);

    /* IPC syscalls */
    syscall_register(SYS_PIPE,   (syscall_fn_t)sys_pipe);
    syscall_register(SYS_PIPE2,  (syscall_fn_t)sys_pipe2);
    syscall_register(SYS_KILL,   (syscall_fn_t)sys_kill);
    syscall_register(SYS_SIGACTION,   (syscall_fn_t)sys_sigaction);
    syscall_register(SYS_SIGPROCMASK, (syscall_fn_t)sys_sigprocmask);
    syscall_register(SYS_SIGRETURN,   (syscall_fn_t)sys_sigreturn);
    syscall_register(SYS_MMAP,   (syscall_fn_t)sys_mmap);
    syscall_register(SYS_MUNMAP, (syscall_fn_t)sys_munmap);

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
    uint64_t *brk_ptr;
    uint64_t old_brk, new_brk;

    /* Get process brk pointer */
    if (current_proc) {
        brk_ptr = &current_proc->brk;
        if (*brk_ptr == 0) {
            *brk_ptr = DEFAULT_BRK;
        }
    } else {
        /* Fallback for kernel context */
        static uint64_t kernel_brk = DEFAULT_BRK;
        brk_ptr = &kernel_brk;
    }

    old_brk = *brk_ptr;

    /* Query current brk */
    if (addr == 0) {
        return (int64_t)old_brk;
    }

    /* Set new brk (page-aligned) */
    new_brk = (addr + 0xFFF) & ~0xFFFUL;

    /* Validate range */
    if (new_brk < DEFAULT_BRK || new_brk >= 0x100000000UL) {
        return (int64_t)old_brk;
    }

    /* Allocate pages if growing */
    if (new_brk > old_brk && current_proc && current_proc->page_table) {
        uint64_t page;
        for (page = (old_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
             page < new_brk;
             page += PAGE_SIZE) {
            uint64_t phys = (uint64_t)pmm_alloc_page();
            if (!phys) {
                return (int64_t)old_brk;
            }
            memset((void *)phys, 0, PAGE_SIZE);
            vmm_map_page_in(current_proc->page_table, page, phys,
                           PTE_WRITABLE | PTE_USER);
        }
    }

    *brk_ptr = new_brk;
    return (int64_t)new_brk;
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

/*
 * sys_getppid - Get parent process ID
 */
int64_t sys_getppid(void) {
    if (current_proc) {
        return (int64_t)current_proc->ppid;
    }
    return 0;
}

/*
 * sys_fork - Create a child process
 *
 * Returns: 0 to child, child PID to parent, -1 on error
 */
int64_t sys_fork(void) {
    process_t *child;
    process_t *parent = current_proc;

    if (!parent) {
        return -EAGAIN;
    }

    /* Allocate child process */
    child = process_alloc();
    if (!child) {
        kprintf("[FORK] Failed to allocate child process\n");
        return -EAGAIN;
    }

    /* Copy process name */
    strncpy(child->name, parent->name, sizeof(child->name) - 1);

    /* Set parent-child relationship */
    child->ppid = parent->pid;
    child->parent = parent;

    /* Add to parent's children list */
    child->sibling = parent->children;
    parent->children = child;

    /* Copy file descriptor table */
    if (parent->fd_table) {
        child->fd_table = fd_table_clone(parent->fd_table);
    } else {
        child->fd_table = fd_table_clone(global_fd_table);
    }

    /* Copy current working directory */
    strncpy(child->cwd, parent->cwd, sizeof(child->cwd));

    /* Copy user space attributes */
    child->is_user = parent->is_user;
    child->brk = parent->brk;
    child->user_entry = parent->user_entry;
    child->user_stack = parent->user_stack;

    /* Create new address space and copy pages (simplified - share for now) */
    if (parent->page_table) {
        child->page_table = create_user_address_space();
        if (!child->page_table) {
            process_free(child);
            return -ENOMEM;
        }
        /* Copy user stack */
        setup_user_stack(child);
    }

    /* Copy kernel context - child returns 0 */
    if (parent->context) {
        child->kernel_stack -= sizeof(context_t);
        child->context = (context_t *)child->kernel_stack;
        memcpy(child->context, parent->context, sizeof(context_t));
    }

    /* Make child ready to run */
    sched_ready(child);

    kprintf("[FORK] Created child PID %d from parent PID %d\n",
            child->pid, parent->pid);

    /* Return child PID to parent */
    return child->pid;
}

/* Forward declaration for ELF loader */
int elf_exec(const char *path, char *const argv[], char *const envp[]);

/*
 * sys_execve - Execute a program
 *
 * Replaces current process with new executable
 */
int64_t sys_execve(const char *pathname, char *const argv[], char *const envp[]) {
    int ret;

    if (!pathname) {
        return -EFAULT;
    }

    /* Check if file exists first */
    struct inode *inode = vfs_lookup(pathname);
    if (!inode) {
        return -ENOENT;
    }
    inode_put(inode);

    kprintf("[EXECVE] Executing: %s\n", pathname);

    /* Use ELF loader */
    ret = elf_exec(pathname, argv, envp);
    if (ret < 0) {
        return ret;
    }

    /* exec doesn't return on success - jump to user mode */
    /* This will be handled by returning to the syscall return path
     * with the new context set up */

    return 0;
}

/*
 * sys_wait4 - Wait for a child process
 *
 * pid: -1 = any child, >0 = specific child
 * options: WNOHANG (1) = don't block
 */
int64_t sys_wait4(int pid, int *status, int options, void *rusage) {
    process_t *child;
    int found = 0;
    int i;

    (void)rusage;

    if (!current_proc) {
        return -ECHILD;
    }

    while (1) {
        /* Search for zombie children */
        for (i = 0; i < MAX_PROCESSES; i++) {
            child = &proc_table[i];

            /* Skip if not our child */
            if (child->state == PROC_UNUSED || child->ppid != current_proc->pid) {
                continue;
            }

            found = 1;

            /* Check if this is the child we want */
            if (pid > 0 && child->pid != (uint32_t)pid) {
                continue;
            }

            /* Found a zombie? */
            if (child->state == PROC_ZOMBIE || child->exited) {
                int child_pid = child->pid;
                int exit_code = child->exit_code;

                /* Return status if requested */
                if (status) {
                    /* Linux-style status: exit_code << 8 */
                    *status = (exit_code & 0xFF) << 8;
                }

                /* Clean up child */
                process_free(child);

                kprintf("[WAIT] Reaped child PID %d, exit code %d\n",
                        child_pid, exit_code);
                return child_pid;
            }
        }

        /* No children at all? */
        if (!found) {
            return -ECHILD;
        }

        /* WNOHANG - don't block */
        if (options & 1) {
            return 0;
        }

        /* Block and wait for a child to exit */
        current_proc->state = PROC_BLOCKED;
        schedule();
    }
}

/*
 * sys_getcwd - Get current working directory
 */
int64_t sys_getcwd(char *buf, size_t size) {
    const char *cwd;

    if (!buf || size == 0) {
        return -EINVAL;
    }

    /* Get cwd from process or use "/" as default */
    if (current_proc && current_proc->cwd[0]) {
        cwd = current_proc->cwd;
    } else {
        cwd = "/";
    }

    size_t len = strlen(cwd);
    if (len + 1 > size) {
        return -ERANGE;
    }

    strncpy(buf, cwd, size);
    return (int64_t)buf;
}

/*
 * sys_chdir - Change current working directory
 */
int64_t sys_chdir(const char *path) {
    struct inode *inode;

    if (!path) {
        return -EFAULT;
    }

    /* Verify path exists and is a directory */
    inode = vfs_lookup(path);
    if (!inode) {
        return -ENOENT;
    }

    if (!S_ISDIR(inode->i_mode)) {
        inode_put(inode);
        return -ENOTDIR;
    }
    inode_put(inode);

    /* Update process cwd */
    if (current_proc) {
        strncpy(current_proc->cwd, path, sizeof(current_proc->cwd) - 1);
        current_proc->cwd[sizeof(current_proc->cwd) - 1] = '\0';
    }

    return 0;
}

/*
 * sys_mkdir - Create a directory
 */
int64_t sys_mkdir(const char *pathname, uint32_t mode) {
    if (!pathname) {
        return -EFAULT;
    }

    return vfs_mkdir(pathname, mode);
}

/*
 * sys_getdents64 - Get directory entries
 */
int64_t sys_getdents64(int fd, void *dirp, size_t count) {
    struct fd_table *table;
    struct file *file;
    struct dirent dirent;
    struct linux_dirent64 *d;
    size_t pos = 0;
    int ret;

    if (!dirp || count == 0) {
        return -EINVAL;
    }

    table = get_fd_table();
    if (!table) {
        return -EBADF;
    }

    file = fd_get(table, fd);
    if (!file) {
        return -EBADF;
    }

    if (!file->f_inode || !S_ISDIR(file->f_inode->i_mode)) {
        return -ENOTDIR;
    }

    /* Read directory entries */
    while (pos + sizeof(struct linux_dirent64) + 256 < count) {
        ret = vfs_readdir(file, &dirent);
        if (ret <= 0) {
            break;
        }

        /* Calculate record length (aligned to 8 bytes) */
        size_t name_len = strlen(dirent.d_name);
        size_t reclen = (sizeof(struct linux_dirent64) + name_len + 1 + 7) & ~7;

        if (pos + reclen > count) {
            break;
        }

        /* Fill in linux_dirent64 structure */
        d = (struct linux_dirent64 *)((char *)dirp + pos);
        d->d_ino = dirent.d_ino;
        d->d_off = file->f_pos;
        d->d_reclen = reclen;
        d->d_type = dirent.d_type;
        memcpy(d->d_name, dirent.d_name, name_len + 1);

        pos += reclen;
    }

    return pos;
}
