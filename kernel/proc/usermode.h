/*
 * usermode.h - Kernel/User mode transition
 *
 * P-21: Ring 0 <-> Ring 3 switching
 */

#ifndef _USERMODE_H
#define _USERMODE_H

#include "types.h"
#include "process.h"

/* Jump to user mode
 *
 * This function:
 * 1. Sets up the stack frame for iretq
 * 2. Switches to user mode (ring 3)
 * 3. Begins execution at the specified entry point
 *
 * Parameters:
 *   entry: User code entry point (RIP)
 *   user_stack: User stack pointer (RSP)
 *
 * Note: This function does not return!
 */
void jump_to_usermode(uint64_t entry, uint64_t user_stack);

/* Switch to a user process's address space
 *
 * Parameters:
 *   proc: Process to switch to
 *
 * This loads the process's page table into CR3
 */
void switch_address_space(process_t *proc);

/* Create and start a user process
 *
 * Parameters:
 *   code: User code to run
 *   size: Size of code in bytes
 *   name: Process name
 *
 * Returns: Process pointer, or NULL on failure
 */
process_t *create_user_process(void *code, uint64_t size, const char *name);

/* Test user mode functionality
 *
 * Creates a simple user process that makes a syscall
 */
void test_usermode(void);

#endif /* _USERMODE_H */
