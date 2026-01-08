/*
 * signal.c - Signal implementation
 *
 * Implements POSIX-like signal mechanism for process communication.
 */

#include "signal.h"
#include "../proc/process.h"
#include "../proc/scheduler.h"
#include "../proc/syscall.h"
#include "../lib/kprintf.h"
#include "../lib/string.h"

/* Forward declaration */
struct process;

/*
 * Initialize signal info for a process
 */
void signal_init(signal_info_t *si) {
    if (!si) return;

    si->pending = 0;
    si->blocked = 0;

    /* Set default handlers */
    for (int i = 0; i < NSIG; i++) {
        si->handlers[i] = SIG_DFL;
    }
}

/*
 * Check if a signal can be caught or ignored
 */
static int signal_can_catch(int sig) {
    /* SIGKILL and SIGSTOP cannot be caught or ignored */
    return (sig != SIGKILL && sig != SIGSTOP);
}

/*
 * Get default action for a signal
 * Returns: 0 = ignore, 1 = terminate, 2 = stop, 3 = continue
 */
static int signal_default_action(int sig) {
    switch (sig) {
        /* Terminate */
        case SIGHUP:
        case SIGINT:
        case SIGQUIT:
        case SIGILL:
        case SIGABRT:
        case SIGFPE:
        case SIGKILL:
        case SIGSEGV:
        case SIGPIPE:
        case SIGALRM:
        case SIGTERM:
        case SIGUSR1:
        case SIGUSR2:
        case SIGSTKFLT:
        case SIGXCPU:
        case SIGXFSZ:
        case SIGSYS:
            return 1;  /* Terminate */

        /* Stop */
        case SIGSTOP:
        case SIGTSTP:
        case SIGTTIN:
        case SIGTTOU:
            return 2;  /* Stop */

        /* Continue */
        case SIGCONT:
            return 3;  /* Continue */

        /* Ignore by default */
        case SIGCHLD:
        case SIGURG:
        case SIGWINCH:
        default:
            return 0;  /* Ignore */
    }
}

/*
 * Send a signal to a process
 */
int signal_send(struct process *proc, int sig) {
    if (!proc || sig < 1 || sig >= NSIG) {
        return -EINVAL;
    }

    kprintf("[SIGNAL] Sending signal %d to PID %d\n", sig, proc->pid);

    /* Add to pending signals */
    proc->sig_pending |= (1ULL << (sig - 1));

    /* Wake up the process if it's blocked */
    if (proc->state == PROC_BLOCKED) {
        proc->state = PROC_READY;
        sched_ready(proc);
    }

    return 0;
}

/*
 * Check if process has pending unblocked signals
 */
int signal_pending(struct process *proc) {
    if (!proc) return 0;

    uint64_t pending = proc->sig_pending & ~proc->sig_blocked;
    return pending != 0;
}

/*
 * Handle pending signals for a process
 * Called when returning from kernel to user mode
 */
void signal_handle(struct process *proc) {
    if (!proc) return;

    uint64_t pending = proc->sig_pending & ~proc->sig_blocked;
    if (pending == 0) return;

    /* Find first pending signal */
    for (int sig = 1; sig < NSIG; sig++) {
        if (!(pending & (1ULL << (sig - 1)))) {
            continue;
        }

        /* Clear from pending */
        proc->sig_pending &= ~(1ULL << (sig - 1));

        kprintf("[SIGNAL] Handling signal %d for PID %d\n", sig, proc->pid);

        /* Get handler */
        sighandler_t handler = proc->sig_handlers[sig - 1];

        if (handler == SIG_IGN && signal_can_catch(sig)) {
            /* Signal ignored */
            continue;
        }

        if (handler == SIG_DFL || !signal_can_catch(sig)) {
            /* Default action */
            int action = signal_default_action(sig);

            switch (action) {
                case 0:  /* Ignore */
                    break;

                case 1:  /* Terminate */
                    kprintf("[SIGNAL] Terminating PID %d due to signal %d\n",
                            proc->pid, sig);
                    proc->exit_code = 128 + sig;
                    proc->state = PROC_ZOMBIE;
                    proc->exited = 1;
                    if (proc == current_proc) {
                        schedule();
                    }
                    return;

                case 2:  /* Stop */
                    kprintf("[SIGNAL] Stopping PID %d\n", proc->pid);
                    proc->state = PROC_BLOCKED;
                    if (proc == current_proc) {
                        schedule();
                    }
                    return;

                case 3:  /* Continue */
                    if (proc->state == PROC_BLOCKED) {
                        proc->state = PROC_READY;
                        sched_ready(proc);
                    }
                    break;
            }
        } else {
            /* User handler - would need to set up signal frame on user stack
             * This is a simplified implementation that just calls the default */
            kprintf("[SIGNAL] User handler for signal %d (not fully implemented)\n", sig);
            /* Full implementation would:
             * 1. Save current user context
             * 2. Set up signal frame on user stack
             * 3. Set RIP to handler address
             * 4. Return to user mode
             * 5. Handler calls sigreturn() when done
             */
        }
    }
}

/*
 * sys_kill - Send signal to a process
 */
int64_t sys_kill(int pid, int sig) {
    struct process *proc;

    if (sig < 0 || sig >= NSIG) {
        return -EINVAL;
    }

    /* Signal 0 is used for error checking */
    if (sig == 0) {
        proc = process_get(pid);
        return proc ? 0 : -ESRCH;
    }

    if (pid > 0) {
        /* Send to specific process */
        proc = process_get(pid);
        if (!proc) {
            return -ESRCH;
        }
        return signal_send(proc, sig);
    } else if (pid == 0) {
        /* Send to all processes in same process group (not implemented) */
        return -ESRCH;
    } else if (pid == -1) {
        /* Send to all processes (not implemented) */
        return -ESRCH;
    } else {
        /* Send to process group -pid (not implemented) */
        return -ESRCH;
    }
}

/*
 * sys_signal - Simple signal handler registration
 */
int64_t sys_signal(int signum, sighandler_t handler) {
    sighandler_t old_handler;

    if (!current_proc || signum < 1 || signum >= NSIG) {
        return (int64_t)SIG_ERR;
    }

    if (!signal_can_catch(signum)) {
        return (int64_t)SIG_ERR;
    }

    old_handler = current_proc->sig_handlers[signum - 1];
    current_proc->sig_handlers[signum - 1] = handler;

    kprintf("[SIGNAL] Registered handler for signal %d\n", signum);

    return (int64_t)old_handler;
}

/*
 * sys_sigaction - Advanced signal handler registration
 */
int64_t sys_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    if (!current_proc || signum < 1 || signum >= NSIG) {
        return -EINVAL;
    }

    if (!signal_can_catch(signum)) {
        return -EINVAL;
    }

    /* Return old action */
    if (oldact) {
        oldact->sa_handler = current_proc->sig_handlers[signum - 1];
        oldact->sa_mask = 0;
        oldact->sa_flags = 0;
    }

    /* Set new action */
    if (act) {
        current_proc->sig_handlers[signum - 1] = act->sa_handler;
    }

    return 0;
}

/*
 * sys_sigprocmask - Change blocked signal mask
 */
int64_t sys_sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    if (!current_proc) {
        return -EINVAL;
    }

    /* Return old mask */
    if (oldset) {
        *oldset = current_proc->sig_blocked;
    }

    /* Change mask */
    if (set) {
        switch (how) {
            case SIG_BLOCK:
                current_proc->sig_blocked |= *set;
                break;
            case SIG_UNBLOCK:
                current_proc->sig_blocked &= ~(*set);
                break;
            case SIG_SETMASK:
                current_proc->sig_blocked = *set;
                break;
            default:
                return -EINVAL;
        }

        /* Cannot block SIGKILL or SIGSTOP */
        current_proc->sig_blocked &= ~((1ULL << (SIGKILL - 1)) | (1ULL << (SIGSTOP - 1)));
    }

    return 0;
}

/*
 * sys_sigreturn - Return from signal handler
 * Would restore saved user context
 */
int64_t sys_sigreturn(void) {
    /* Full implementation would restore the saved context from signal frame */
    kprintf("[SIGNAL] sigreturn called\n");
    return 0;
}
