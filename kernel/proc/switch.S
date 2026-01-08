/*
 * switch.S - Context switch implementation
 *
 * void switch_context(context_t **old, context_t *new);
 *   - old: pointer to where to save the old context pointer
 *   - new: pointer to the new context to switch to
 */

.global switch_context
.type switch_context, @function

switch_context:
    /* Save callee-saved registers on current stack */
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    /* Save current stack pointer to *old (rdi points to old context ptr) */
    movq %rsp, (%rdi)

    /* Switch to new stack (rsi contains new context ptr) */
    movq %rsi, %rsp

    /* Restore callee-saved registers from new stack */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    /* Return to new context (return address is on stack) */
    ret

/*
 * void switch_to_new(context_t *new);
 *   - Used for first switch to a new process (no old context to save)
 */
.global switch_to_new
.type switch_to_new, @function

switch_to_new:
    /* Just switch to new stack and restore context */
    movq %rdi, %rsp

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    ret
