/*
 * sys/wait.h - Wait for process
 */

#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <stddef.h>

typedef int pid_t;

/* Wait options */
#define WNOHANG   1
#define WUNTRACED 2

/* Status macros */
#define WIFEXITED(status)   (((status) & 0x7F) == 0)
#define WEXITSTATUS(status) (((status) & 0xFF00) >> 8)
#define WIFSIGNALED(status) (((status) & 0x7F) != 0)
#define WTERMSIG(status)    ((status) & 0x7F)
#define WIFSTOPPED(status)  (((status) & 0xFF) == 0x7F)
#define WSTOPSIG(status)    WEXITSTATUS(status)

pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);

#endif /* _SYS_WAIT_H */
