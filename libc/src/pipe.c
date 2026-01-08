/*
 * pipe.c - Pipe functions
 */

#include <unistd.h>
#include <syscall.h>

int pipe(int pipefd[2]) {
    return syscall1(SYS_pipe, (long)pipefd);
}

int pipe2(int pipefd[2], int flags) {
    return syscall2(SYS_pipe2, (long)pipefd, flags);
}
