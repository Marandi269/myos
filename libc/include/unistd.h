/*
 * unistd.h - POSIX API
 */

#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>

/* File descriptors */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* Types */
typedef int pid_t;
typedef long off_t;

/* File I/O */
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
off_t lseek(int fd, off_t offset, int whence);
int dup(int oldfd);
int dup2(int oldfd, int newfd);

/* Seek whence */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* Process control */
pid_t fork(void);
pid_t getpid(void);
pid_t getppid(void);
int execve(const char *pathname, char *const argv[], char *const envp[]);
int execv(const char *pathname, char *const argv[]);
int execvp(const char *file, char *const argv[]);
void _exit(int status);

/* Working directory */
char *getcwd(char *buf, size_t size);
int chdir(const char *path);

/* Misc */
unsigned int sleep(unsigned int seconds);

#endif /* _UNISTD_H */
