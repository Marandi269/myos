/*
 * pmm.h - Physical Memory Manager interface
 */

#ifndef _PMM_H
#define _PMM_H

#include "types.h"

/* Page size: 4KB */
#ifndef PAGE_SIZE
#define PAGE_SIZE       4096
#endif
#define PAGE_SHIFT      12

/* Memory region types */
#define MEMORY_AVAILABLE    1
#define MEMORY_RESERVED     2
#define MEMORY_ACPI         3
#define MEMORY_NVS          4
#define MEMORY_BADRAM       5

/* Memory region descriptor */
struct memory_region {
    uint64_t base;
    uint64_t length;
    uint32_t type;
};

/* Initialize physical memory manager */
void pmm_init(uint64_t total_memory);

/* Allocate a physical page (returns physical address) */
void *pmm_alloc_page(void);

/* Free a physical page */
void pmm_free_page(void *addr);

/* Get memory statistics */
uint64_t pmm_get_total_memory(void);
uint64_t pmm_get_free_memory(void);
uint64_t pmm_get_used_memory(void);

/* Print memory map */
void pmm_print_stats(void);

/* Align address up to page boundary */
static inline uint64_t page_align_up(uint64_t addr) {
    return (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

/* Align address down to page boundary */
static inline uint64_t page_align_down(uint64_t addr) {
    return addr & ~(PAGE_SIZE - 1);
}

#endif /* _PMM_H */
