/*
 * signal.h - Signal handling
 */

#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <stdint.h>

/* Signal numbers */
#define SIGHUP      1
#define SIGINT      2
#define SIGQUIT     3
#define SIGILL      4
#define SIGTRAP     5
#define SIGABRT     6
#define SIGBUS      7
#define SIGFPE      8
#define SIGKILL     9
#define SIGUSR1     10
#define SIGSEGV     11
#define SIGUSR2     12
#define SIGPIPE     13
#define SIGALRM     14
#define SIGTERM     15
#define SIGSTKFLT   16
#define SIGCHLD     17
#define SIGCONT     18
#define SIGSTOP     19
#define SIGTSTP     20
#define SIGTTIN     21
#define SIGTTOU     22

/* Signal mask type */
typedef uint64_t sigset_t;

/* Signal handler type */
typedef void (*sighandler_t)(int);

/* Special handlers */
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

/* Signal action flags */
#define SA_NOCLDSTOP    0x00000001
#define SA_NOCLDWAIT    0x00000002
#define SA_SIGINFO      0x00000004
#define SA_RESTART      0x10000000

/* Signal mask operations */
#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

/* sigaction structure */
struct sigaction {
    sighandler_t sa_handler;
    uint32_t sa_flags;
    sigset_t sa_mask;
};

/* Signal functions */
sighandler_t signal(int signum, sighandler_t handler);
int kill(int pid, int sig);
int raise(int sig);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);

/* Signal set operations */
int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int signum);
int sigdelset(sigset_t *set, int signum);
int sigismember(const sigset_t *set, int signum);

#endif /* _SIGNAL_H */
