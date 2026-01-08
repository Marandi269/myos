/*
 * user_space.h - User address space management
 *
 * P-19: User address space (separate page tables)
 * P-20: User stack setup
 */

#ifndef _USER_SPACE_H
#define _USER_SPACE_H

#include "types.h"
#include "process.h"

/* User address space layout */
#define USER_STACK_TOP      0x00007FFFFFFFE000UL  /* Just below 128TB mark */
#define USER_STACK_SIZE     (64 * 1024)           /* 64KB user stack */
#define USER_STACK_PAGES    (USER_STACK_SIZE / 4096)

#define USER_CODE_BASE      0x0000000000400000UL  /* Standard ELF base */
#define USER_HEAP_BASE      0x0000000001000000UL  /* 16MB */

/* Create a new user address space (page table)
 * Returns: pointer to PML4, or NULL on failure
 */
uint64_t *create_user_address_space(void);

/* Free a user address space
 * Frees all user pages and page table structures
 */
void free_user_address_space(uint64_t *pml4);

/* Setup user stack for a process
 * Returns: 0 on success, -1 on failure
 */
int setup_user_stack(process_t *proc);

/* Map a page in a specific page table
 * pml4: the page table to modify
 * vaddr: virtual address to map
 * paddr: physical address to map to
 * flags: page flags (PTE_USER, PTE_WRITABLE, etc.)
 */
int vmm_map_page_in(uint64_t *pml4, uint64_t vaddr, uint64_t paddr, uint64_t flags);

/* Load user code into address space
 * For now, just maps code at USER_CODE_BASE
 */
int load_user_code(process_t *proc, void *code, uint64_t size);

#endif /* _USER_SPACE_H */
