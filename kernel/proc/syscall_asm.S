/*
 * syscall_asm.S - SYSCALL/SYSRET entry point
 *
 * When SYSCALL is executed:
 *   - RCX = return RIP
 *   - R11 = saved RFLAGS
 *   - RIP = LSTAR (this function)
 *   - CS/SS loaded from STAR MSR
 *   - RSP is NOT changed! (still user stack)
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
 *
 * P-21: This code now properly switches to kernel stack
 */

.global syscall_entry
.type syscall_entry, @function

/* Per-CPU kernel stack pointer (from TSS or GS base)
 * For simplicity, we use a global variable for single-CPU
 */
.extern kernel_syscall_stack

syscall_entry:
    /*
     * CRITICAL: We must switch from user stack to kernel stack
     *
     * On entry, RSP still points to user stack!
     * We need to:
     * 1. Save user RSP somewhere safe
     * 2. Load kernel stack pointer
     * 3. Save context on kernel stack
     */

    /* Save user RSP in R12 temporarily (it's callee-saved, we'll save it properly) */
    /* Actually, we can't use any register that might be clobbered */
    /* Use a memory location instead - but we need a safe place */

    /* Simple approach: swap RSP with a per-cpu kernel stack location */
    /* For now, use a global kernel_syscall_stack variable */

    /* Save RAX (syscall number) temporarily */
    pushq %rax                  /* Push to user stack temporarily */

    /* Get kernel stack pointer */
    movq kernel_syscall_stack(%rip), %rax

    /* Exchange: user RSP -> memory, kernel RSP -> RSP */
    xchgq %rsp, %rax            /* Now RSP = kernel stack, RAX = user stack */

    /* Save user stack pointer on kernel stack */
    pushq %rax                  /* user RSP */

    /* Retrieve the syscall number we saved on user stack */
    /* It's at the address we just saved */
    movq (%rax), %rax           /* Load syscall number from user stack */

    /* Now we're on kernel stack - save full context */
    pushq %rcx                  /* User RIP (return address) */
    pushq %r11                  /* User RFLAGS */

    /* Save syscall number (we'll need it for the handler) */
    pushq %rax

    /* Save all caller-saved registers that syscall_handler might clobber */
    pushq %rdi                  /* arg1 */
    pushq %rsi                  /* arg2 */
    pushq %rdx                  /* arg3 */
    pushq %r10                  /* arg4 */
    pushq %r8                   /* arg5 */
    pushq %r9                   /* arg6 (unused but save for consistency) */

    /* Save callee-saved registers */
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /*
     * Set up arguments for syscall_handler(num, arg1, arg2, arg3, arg4, arg5)
     *
     * C calling convention:
     *   RDI = arg0 (syscall number)
     *   RSI = arg1
     *   RDX = arg2
     *   RCX = arg3
     *   R8  = arg4
     *   R9  = arg5
     *
     * Our saved values on stack:
     *   [RSP+0x00]: r15
     *   [RSP+0x08]: r14
     *   [RSP+0x10]: r13
     *   [RSP+0x18]: r12
     *   [RSP+0x20]: rbp
     *   [RSP+0x28]: rbx
     *   [RSP+0x30]: r9 (original)
     *   [RSP+0x38]: r8 (original arg5)
     *   [RSP+0x40]: r10 (original arg4)
     *   [RSP+0x48]: rdx (original arg3)
     *   [RSP+0x50]: rsi (original arg2)
     *   [RSP+0x58]: rdi (original arg1)
     *   [RSP+0x60]: rax (syscall number)
     */

    /* Load arguments from stack */
    movq 0x60(%rsp), %rdi       /* syscall number -> arg0 */
    movq 0x58(%rsp), %rsi       /* original rdi (arg1) -> arg1 */
    movq 0x50(%rsp), %rdx       /* original rsi (arg2) -> arg2 */
    movq 0x48(%rsp), %rcx       /* original rdx (arg3) -> arg3 */
    movq 0x40(%rsp), %r8        /* original r10 (arg4) -> arg4 */
    movq 0x38(%rsp), %r9        /* original r8 (arg5) -> arg5 */

    /* Call C handler */
    call syscall_handler

    /* Return value is in RAX - save it */
    movq %rax, %rbx             /* Save return value */

    /* Restore callee-saved registers */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    /* Skip rbx - we're using it for return value */
    addq $8, %rsp               /* Skip saved rbx */

    /* Skip caller-saved registers (not needed) */
    addq $0x38, %rsp            /* Skip r9, r8, r10, rdx, rsi, rdi, rax (7 * 8) */

    /* Restore return value */
    movq %rbx, %rax

    /* Restore user context */
    popq %r11                   /* User RFLAGS */
    popq %rcx                   /* User RIP */

    /* Restore user RSP */
    popq %rsp

    /* Clean up the value we pushed to user stack at entry */
    addq $8, %rsp               /* Pop the saved RAX from user stack */

    /* Return to user mode */
    sysretq

/*
 * Kernel syscall stack (per-CPU, but we only support 1 CPU for now)
 * This is initialized to point to the kernel stack
 */
.section .data
.global kernel_syscall_stack
.align 8
kernel_syscall_stack:
    .quad 0                     /* Will be initialized by syscall_init */
