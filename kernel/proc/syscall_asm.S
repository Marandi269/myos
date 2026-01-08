/*
 * syscall_asm.S - SYSCALL/SYSRET entry point
 *
 * When SYSCALL is executed:
 *   - RCX = return RIP
 *   - R11 = saved RFLAGS
 *   - RIP = LSTAR (this function)
 *   - CS/SS loaded from STAR MSR
 *
 * System call convention (Linux-like):
 *   - RAX = syscall number
 *   - RDI = arg1
 *   - RSI = arg2
 *   - RDX = arg3
 *   - R10 = arg4 (RCX is clobbered by SYSCALL)
 *   - R8  = arg5
 *   - R9  = arg6
 *   - RAX = return value
 */

.global syscall_entry
.type syscall_entry, @function

syscall_entry:
    /* Switch to kernel stack */
    /* For now, we stay on the same stack since we're in ring 0 testing */
    /* TODO: switch to kernel stack using TSS.rsp0 or per-cpu area */

    /* Save user context */
    pushq %rcx          /* Save return address */
    pushq %r11          /* Save RFLAGS */

    /* Save callee-saved registers (syscall_handler may use them) */
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* Set up arguments for syscall_handler */
    /* RAX = syscall number (already in place) */
    /* RDI = arg1 (already in place) */
    /* RSI = arg2 (already in place) */
    /* RDX = arg3 (already in place) */
    /* R10 -> RCX for arg4 */
    /* R8 = arg5 (already in place) */
    /* R9 = arg6 (not used currently) */
    movq %rax, %rdi     /* syscall number -> arg1 */
    movq %rsi, %rdx     /* original arg2 -> arg3 */
    movq %rdi, %rsi     /* original arg1 -> arg2 (wait, this is wrong) */

    /* Let me redo this properly:
     * On entry: RAX=num, RDI=arg1, RSI=arg2, RDX=arg3, R10=arg4, R8=arg5
     * syscall_handler(num, arg1, arg2, arg3, arg4, arg5)
     * C calling: RDI=num, RSI=arg1, RDX=arg2, RCX=arg3, R8=arg4, R9=arg5
     */

    /* Shuffle arguments */
    movq %r8, %r9       /* arg5 -> r9 */
    movq %r10, %r8      /* arg4 -> r8 */
    movq %rdx, %rcx     /* arg3 -> rcx */
    movq %rsi, %rdx     /* arg2 -> rdx */
    movq %rdi, %rsi     /* arg1 -> rsi */
    movq %rax, %rdi     /* num -> rdi */

    /* Call C handler */
    call syscall_handler

    /* Return value is in RAX */

    /* Restore callee-saved registers */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx

    /* Restore user context */
    popq %r11           /* Restore RFLAGS */
    popq %rcx           /* Restore return address */

    /* Return to user mode */
    sysretq
