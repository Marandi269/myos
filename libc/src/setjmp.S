/*
 * setjmp.S - Non-local jump implementation for x86_64
 *
 * jmp_buf layout:
 *   [0] = rbx
 *   [8] = rbp
 *  [16] = r12
 *  [24] = r13
 *  [32] = r14
 *  [40] = r15
 *  [48] = rsp (after return from setjmp)
 *  [56] = rip (return address)
 */

.text
.globl setjmp
.globl _setjmp
.globl longjmp
.globl _longjmp
.globl sigsetjmp
.globl siglongjmp

/*
 * int setjmp(jmp_buf env)
 *
 * Save the calling environment in env and return 0.
 * When longjmp is called, setjmp will return val.
 */
setjmp:
_setjmp:
    /* Save callee-saved registers */
    movq    %rbx, 0(%rdi)
    movq    %rbp, 8(%rdi)
    movq    %r12, 16(%rdi)
    movq    %r13, 24(%rdi)
    movq    %r14, 32(%rdi)
    movq    %r15, 40(%rdi)

    /* Save stack pointer (what it will be after we return) */
    leaq    8(%rsp), %rax
    movq    %rax, 48(%rdi)

    /* Save return address */
    movq    (%rsp), %rax
    movq    %rax, 56(%rdi)

    /* Return 0 */
    xorl    %eax, %eax
    ret

/*
 * void longjmp(jmp_buf env, int val)
 *
 * Restore the environment saved by setjmp and return val.
 * If val is 0, return 1 instead.
 */
longjmp:
_longjmp:
    /* val = (val == 0) ? 1 : val */
    movl    %esi, %eax
    testl   %eax, %eax
    jnz     1f
    incl    %eax
1:
    /* Restore callee-saved registers */
    movq    0(%rdi), %rbx
    movq    8(%rdi), %rbp
    movq    16(%rdi), %r12
    movq    24(%rdi), %r13
    movq    32(%rdi), %r14
    movq    40(%rdi), %r15

    /* Restore stack pointer */
    movq    48(%rdi), %rsp

    /* Jump to saved return address */
    jmpq    *56(%rdi)

/*
 * int sigsetjmp(sigjmp_buf env, int savemask)
 *
 * Like setjmp, but optionally saves the signal mask.
 * For now, we just call setjmp (signal mask handling can be added later)
 */
sigsetjmp:
    /* Save savemask flag at offset 64 */
    movl    %esi, 64(%rdi)
    /* Zero out saved_mask for now */
    movq    $0, 72(%rdi)
    /* Fall through to setjmp-like code */

    /* Save callee-saved registers */
    movq    %rbx, 0(%rdi)
    movq    %rbp, 8(%rdi)
    movq    %r12, 16(%rdi)
    movq    %r13, 24(%rdi)
    movq    %r14, 32(%rdi)
    movq    %r15, 40(%rdi)

    /* Save stack pointer (what it will be after we return) */
    leaq    8(%rsp), %rax
    movq    %rax, 48(%rdi)

    /* Save return address */
    movq    (%rsp), %rax
    movq    %rax, 56(%rdi)

    /* Return 0 */
    xorl    %eax, %eax
    ret

/*
 * void siglongjmp(sigjmp_buf env, int val)
 *
 * Like longjmp, but restores signal mask if it was saved.
 */
siglongjmp:
    /* Just call longjmp for now */
    jmp     longjmp
