/*
 * vmm.h - Virtual Memory Manager interface
 */

#ifndef _VMM_H
#define _VMM_H

#include "types.h"

/* Page table entry flags */
#define PTE_PRESENT     (1UL << 0)   /* Page is present */
#define PTE_WRITABLE    (1UL << 1)   /* Page is writable */
#define PTE_USER        (1UL << 2)   /* User accessible */
#define PTE_PWT         (1UL << 3)   /* Write-through caching */
#define PTE_PCD         (1UL << 4)   /* Cache disabled */
#define PTE_ACCESSED    (1UL << 5)   /* Page was accessed */
#define PTE_DIRTY       (1UL << 6)   /* Page was written */
#define PTE_HUGE        (1UL << 7)   /* Huge page (2MB/1GB) */
#define PTE_GLOBAL      (1UL << 8)   /* Global page (not flushed) */
#define PTE_NX          (1UL << 63)  /* No execute */

/* Page sizes */
#ifndef PAGE_SIZE
#define PAGE_SIZE       4096UL
#endif
#define PAGE_MASK       (~(PAGE_SIZE - 1))
#define HUGE_PAGE_SIZE  (2UL * 1024 * 1024)  /* 2MB */

/* Page table index extraction (9 bits per level) */
#define PML4_INDEX(addr)  (((uint64_t)(addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr)  (((uint64_t)(addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)    (((uint64_t)(addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)    (((uint64_t)(addr) >> 12) & 0x1FF)

/* Page table entry types */
typedef uint64_t pte_t;
typedef uint64_t pde_t;
typedef uint64_t pdpte_t;
typedef uint64_t pml4e_t;

/* Page table structure (4KB aligned, 512 entries) */
typedef struct {
    uint64_t entries[512];
} __attribute__((aligned(4096))) page_table_t;

/* Initialize virtual memory manager */
void vmm_init(void);

/* Map a single 4KB page */
int vmm_map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags);

/* Map a range of pages */
int vmm_map_range(uint64_t vaddr, uint64_t paddr, uint64_t size, uint64_t flags);

/* Unmap a single page */
int vmm_unmap_page(uint64_t vaddr);

/* Get physical address for virtual address */
uint64_t vmm_get_phys(uint64_t vaddr);

/* Flush TLB for a single page */
void vmm_flush_tlb(uint64_t vaddr);

/* Flush entire TLB */
void vmm_flush_tlb_all(void);

/* Get kernel PML4 */
pml4e_t *vmm_get_kernel_pml4(void);

#endif /* _VMM_H */
