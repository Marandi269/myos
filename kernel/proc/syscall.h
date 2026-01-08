/*
 * syscall.h - System call interface
 */

#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "types.h"

/* MSR addresses for SYSCALL/SYSRET */
#define MSR_EFER    0xC0000080  /* Extended Feature Enable Register */
#define MSR_STAR    0xC0000081  /* Segment selectors */
#define MSR_LSTAR   0xC0000082  /* SYSCALL entry point (64-bit) */
#define MSR_CSTAR   0xC0000083  /* SYSCALL entry point (compat, unused) */
#define MSR_SFMASK  0xC0000084  /* RFLAGS mask for SYSCALL */

/* EFER bits */
#define EFER_SCE    (1 << 0)    /* SYSCALL/SYSRET Enable */

/*
 * System call numbers (Linux-compatible subset)
 */
#define SYS_READ        0
#define SYS_WRITE       1
#define SYS_OPEN        2
#define SYS_CLOSE       3
#define SYS_STAT        4
#define SYS_FSTAT       5
#define SYS_LSEEK       8
#define SYS_MMAP        9
#define SYS_MPROTECT    10
#define SYS_MUNMAP      11
#define SYS_BRK         12
#define SYS_IOCTL       16
#define SYS_PIPE        22
#define SYS_DUP         32
#define SYS_DUP2        33
#define SYS_GETPID      39
#define SYS_FORK        57
#define SYS_EXECVE      59
#define SYS_EXIT        60
#define SYS_WAIT4       61
#define SYS_KILL        62
#define SYS_GETCWD      79
#define SYS_CHDIR       80
#define SYS_MKDIR       83
#define SYS_GETDENTS64  217
#define SYS_GETPPID     110

#define MAX_SYSCALL     256

/* Error codes */
#define ENOSYS      38  /* Function not implemented */
#define EBADF       9   /* Bad file descriptor */
#define EINVAL      22  /* Invalid argument */
#define ENOMEM      12  /* Out of memory */
#define EFAULT      14  /* Bad address */
#define ENOENT      2   /* No such file or directory */
#define ECHILD      10  /* No child processes */
#define EAGAIN      11  /* Try again */
#define EACCES      13  /* Permission denied */
#define ENOTDIR     20  /* Not a directory */
#define EISDIR      21  /* Is a directory */
#define ERANGE      34  /* Math result not representable */
#define ENOEXEC     8   /* Exec format error */
#define ESRCH       3   /* No such process */
#define EIO         5   /* I/O error */

/* System call handler function type */
typedef int64_t (*syscall_fn_t)(uint64_t, uint64_t, uint64_t,
                                 uint64_t, uint64_t, uint64_t);

/* Initialize SYSCALL/SYSRET */
void syscall_init(void);

/* Initialize syscall stdio (call after fs_init) */
void syscall_init_stdio(void);

/* System call handler (called from assembly) */
uint64_t syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2,
                         uint64_t arg3, uint64_t arg4, uint64_t arg5);

/* Register a system call handler */
void syscall_register(int num, syscall_fn_t handler);

/*
 * Individual system call declarations
 */
int64_t sys_read(int fd, char *buf, size_t count);
int64_t sys_write(int fd, const char *buf, size_t count);
int64_t sys_open(const char *pathname, int flags, int mode);
int64_t sys_close(int fd);
int64_t sys_lseek(int fd, int64_t offset, int whence);
int64_t sys_brk(uint64_t addr);
int64_t sys_getpid(void);
int64_t sys_getppid(void);
int64_t sys_exit(int status);
int64_t sys_dup(int oldfd);
int64_t sys_dup2(int oldfd, int newfd);

/* Process management syscalls */
int64_t sys_fork(void);
int64_t sys_execve(const char *pathname, char *const argv[], char *const envp[]);
int64_t sys_wait4(int pid, int *status, int options, void *rusage);

/* Directory syscalls */
int64_t sys_getcwd(char *buf, size_t size);
int64_t sys_chdir(const char *path);
int64_t sys_mkdir(const char *pathname, uint32_t mode);
int64_t sys_getdents64(int fd, void *dirp, size_t count);

/* Linux dirent64 structure */
struct linux_dirent64 {
    uint64_t d_ino;
    int64_t  d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[];
};

#endif /* _SYSCALL_H */
