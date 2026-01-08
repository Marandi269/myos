/*
 * wait.c - Process wait functions
 */

#include <sys/wait.h>
#include <syscall.h>

pid_t wait(int *status) {
    return waitpid(-1, status, 0);
}

pid_t waitpid(pid_t pid, int *status, int options) {
    return syscall4(SYS_wait4, pid, (long)status, options, 0);
}
