/*
 * signal.h - Signal definitions and interfaces
 */

#ifndef _KERNEL_SIGNAL_H
#define _KERNEL_SIGNAL_H

#include "types.h"

/* Forward declaration */
struct process;

/* Signal numbers (POSIX compatible) */
#define SIGHUP      1   /* Hangup */
#define SIGINT      2   /* Interrupt (Ctrl+C) */
#define SIGQUIT     3   /* Quit */
#define SIGILL      4   /* Illegal instruction */
#define SIGTRAP     5   /* Trace trap */
#define SIGABRT     6   /* Abort */
#define SIGBUS      7   /* Bus error */
#define SIGFPE      8   /* Floating point exception */
#define SIGKILL     9   /* Kill (cannot be caught or ignored) */
#define SIGUSR1     10  /* User defined signal 1 */
#define SIGSEGV     11  /* Segmentation fault */
#define SIGUSR2     12  /* User defined signal 2 */
#define SIGPIPE     13  /* Broken pipe */
#define SIGALRM     14  /* Alarm clock */
#define SIGTERM     15  /* Termination */
#define SIGSTKFLT   16  /* Stack fault */
#define SIGCHLD     17  /* Child status changed */
#define SIGCONT     18  /* Continue */
#define SIGSTOP     19  /* Stop (cannot be caught or ignored) */
#define SIGTSTP     20  /* Terminal stop (Ctrl+Z) */
#define SIGTTIN     21  /* Background read from tty */
#define SIGTTOU     22  /* Background write to tty */
#define SIGURG      23  /* Urgent condition on socket */
#define SIGXCPU     24  /* CPU time limit exceeded */
#define SIGXFSZ     25  /* File size limit exceeded */
#define SIGVTALRM   26  /* Virtual timer expired */
#define SIGPROF     27  /* Profiling timer expired */
#define SIGWINCH    28  /* Window size change */
#define SIGIO       29  /* I/O now possible */
#define SIGPWR      30  /* Power failure */
#define SIGSYS      31  /* Bad system call */

#define NSIG        32  /* Number of signals */

/* Signal mask type */
typedef uint64_t sigset_t;

/* Signal handler type */
typedef void (*sighandler_t)(int);

/* Special signal handlers */
#define SIG_DFL ((sighandler_t)0)   /* Default action */
#define SIG_IGN ((sighandler_t)1)   /* Ignore signal */
#define SIG_ERR ((sighandler_t)-1)  /* Error return */

/* Signal action flags */
#define SA_NOCLDSTOP    0x00000001
#define SA_NOCLDWAIT    0x00000002
#define SA_SIGINFO      0x00000004
#define SA_ONSTACK      0x08000000
#define SA_RESTART      0x10000000
#define SA_NODEFER      0x40000000
#define SA_RESETHAND    0x80000000

/* sigaction structure */
struct sigaction {
    sighandler_t sa_handler;
    uint32_t sa_flags;
    sigset_t sa_mask;
};

/* Signal information (for PCB) */
typedef struct {
    sigset_t pending;           /* Pending signals bitmap */
    sigset_t blocked;           /* Blocked signals bitmap */
    sighandler_t handlers[NSIG]; /* Signal handlers */
} signal_info_t;

/* Signal functions */
void signal_init(signal_info_t *si);
int signal_send(struct process *proc, int sig);
int signal_pending(struct process *proc);
void signal_handle(struct process *proc);

/* System calls */
int64_t sys_kill(int pid, int sig);
int64_t sys_signal(int signum, sighandler_t handler);
int64_t sys_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int64_t sys_sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int64_t sys_sigreturn(void);

/* Signal mask operations */
#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

/* Signal set macros */
#define sigemptyset(set)    (*(set) = 0)
#define sigfillset(set)     (*(set) = ~0ULL)
#define sigaddset(set, sig) (*(set) |= (1ULL << ((sig) - 1)))
#define sigdelset(set, sig) (*(set) &= ~(1ULL << ((sig) - 1)))
#define sigismember(set, sig) ((*(set) & (1ULL << ((sig) - 1))) != 0)

#endif /* _KERNEL_SIGNAL_H */
