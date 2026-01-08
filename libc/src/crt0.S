/*
 * crt0.S - C runtime startup code
 *
 * This is the entry point for all user programs.
 * It sets up the environment and calls main().
 */

.section .text
.global _start
.type _start, @function

_start:
    /* Clear base pointer for debuggers */
    xor %rbp, %rbp

    /* argc is at (%rsp), argv at 8(%rsp) */
    /* For now, we pass argc=0, argv=NULL to main */

    /* Align stack to 16 bytes */
    and $-16, %rsp

    /* Call main(argc, argv, envp) */
    xor %edi, %edi          /* argc = 0 */
    xor %rsi, %rsi          /* argv = NULL */
    xor %rdx, %rdx          /* envp = NULL */
    call main

    /* Call exit with return value from main */
    mov %eax, %edi
    call exit

    /* Should never reach here */
    ud2

.size _start, . - _start
