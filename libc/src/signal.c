/*
 * signal.c - Signal handling functions
 */

#include <signal.h>
#include <syscall.h>
#include <unistd.h>

sighandler_t signal(int signum, sighandler_t handler) {
    return (sighandler_t)syscall2(SYS_sigaction, signum, (long)handler);
}

int kill(int pid, int sig) {
    return syscall2(SYS_kill, pid, sig);
}

int raise(int sig) {
    return kill(getpid(), sig);
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    return syscall3(SYS_sigaction, signum, (long)act, (long)oldact);
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    return syscall3(SYS_sigprocmask, how, (long)set, (long)oldset);
}

int sigemptyset(sigset_t *set) {
    *set = 0;
    return 0;
}

int sigfillset(sigset_t *set) {
    *set = ~0ULL;
    return 0;
}

int sigaddset(sigset_t *set, int signum) {
    if (signum < 1 || signum > 64) return -1;
    *set |= (1ULL << (signum - 1));
    return 0;
}

int sigdelset(sigset_t *set, int signum) {
    if (signum < 1 || signum > 64) return -1;
    *set &= ~(1ULL << (signum - 1));
    return 0;
}

int sigismember(const sigset_t *set, int signum) {
    if (signum < 1 || signum > 64) return -1;
    return (*set & (1ULL << (signum - 1))) != 0;
}
