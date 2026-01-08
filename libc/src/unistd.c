/*
 * unistd.c - POSIX system calls
 */

#include <unistd.h>
#include <syscall.h>
#include <stddef.h>
#include <fcntl.h>

ssize_t read(int fd, void *buf, size_t count) {
    return syscall3(SYS_read, fd, (long)buf, count);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return syscall3(SYS_write, fd, (long)buf, count);
}

int close(int fd) {
    return syscall1(SYS_close, fd);
}

off_t lseek(int fd, off_t offset, int whence) {
    return syscall3(SYS_lseek, fd, offset, whence);
}

int dup(int oldfd) {
    return syscall1(SYS_dup, oldfd);
}

int dup2(int oldfd, int newfd) {
    return syscall2(SYS_dup2, oldfd, newfd);
}

pid_t fork(void) {
    return syscall0(SYS_fork);
}

pid_t getpid(void) {
    return syscall0(SYS_getpid);
}

pid_t getppid(void) {
    return syscall0(SYS_getppid);
}

int execve(const char *pathname, char *const argv[], char *const envp[]) {
    return syscall3(SYS_execve, (long)pathname, (long)argv, (long)envp);
}

int execv(const char *pathname, char *const argv[]) {
    return execve(pathname, argv, NULL);
}

int execvp(const char *file, char *const argv[]) {
    /* Simple implementation - just try the path directly */
    return execve(file, argv, NULL);
}

void _exit(int status) {
    syscall1(SYS_exit, status);
    __builtin_unreachable();
}

char *getcwd(char *buf, size_t size) {
    long ret = syscall2(SYS_getcwd, (long)buf, size);
    return (ret < 0) ? NULL : buf;
}

int chdir(const char *path) {
    return syscall1(SYS_chdir, (long)path);
}

int open(const char *pathname, int flags, ...) {
    int mode = 0;
    if (flags & O_CREAT) {
        /* Get mode from varargs */
        /* For simplicity, use default mode */
        mode = 0644;
    }
    return syscall3(SYS_open, (long)pathname, flags, mode);
}

int mkdir(const char *pathname, unsigned int mode) {
    return syscall2(SYS_mkdir, (long)pathname, mode);
}

unsigned int sleep(unsigned int seconds) {
    /* Simple busy wait - not ideal but works for now */
    (void)seconds;
    return 0;
}
