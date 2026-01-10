/*
 * setjmp.h - Non-local jumps
 */

#ifndef _SETJMP_H
#define _SETJMP_H

/*
 * jmp_buf structure for x86_64
 * Saves: rbx, rbp, r12, r13, r14, r15, rsp, rip
 */
typedef struct {
    unsigned long rbx;
    unsigned long rbp;
    unsigned long r12;
    unsigned long r13;
    unsigned long r14;
    unsigned long r15;
    unsigned long rsp;
    unsigned long rip;
} jmp_buf[1];

/* sigjmp_buf includes signal mask */
typedef struct {
    jmp_buf __jmpbuf;
    int __mask_was_saved;
    unsigned long __saved_mask;
} sigjmp_buf[1];

/* Save calling environment in env for later use by longjmp */
int setjmp(jmp_buf env);

/* Restore environment saved by setjmp */
void longjmp(jmp_buf env, int val) __attribute__((noreturn));

/* Save calling environment and signal mask */
int sigsetjmp(sigjmp_buf env, int savemask);

/* Restore environment saved by sigsetjmp */
void siglongjmp(sigjmp_buf env, int val) __attribute__((noreturn));

/* _setjmp and _longjmp don't save/restore signal mask */
int _setjmp(jmp_buf env);
void _longjmp(jmp_buf env, int val) __attribute__((noreturn));

#endif /* _SETJMP_H */
