/*
 * vmm.c - Virtual Memory Manager implementation
 *
 * Manages page tables and virtual-to-physical address mapping.
 * Currently uses identity mapping (virtual = physical) for simplicity.
 */

#include "vmm.h"
#include "pmm.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Kernel PML4 (global) */
static pml4e_t *kernel_pml4 = NULL;

/* Allocate a zeroed page table */
static page_table_t *alloc_page_table(void) {
    page_table_t *pt = (page_table_t *)pmm_alloc_page();
    if (pt) {
        memset(pt, 0, PAGE_SIZE);
    }
    return pt;
}

/* Flush TLB for a single page */
void vmm_flush_tlb(uint64_t vaddr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(vaddr) : "memory");
}

/* Flush entire TLB by reloading CR3 */
void vmm_flush_tlb_all(void) {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

/* Get kernel PML4 */
pml4e_t *vmm_get_kernel_pml4(void) {
    return kernel_pml4;
}

/* Map a single 4KB page */
int vmm_map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    uint64_t pml4_idx, pdpt_idx, pd_idx, pt_idx;
    pdpte_t *pdpt;
    pde_t *pd;
    pte_t *pt;

    /* Align addresses to page boundary */
    vaddr &= PAGE_MASK;
    paddr &= PAGE_MASK;

    /* Get page table indices */
    pml4_idx = PML4_INDEX(vaddr);
    pdpt_idx = PDPT_INDEX(vaddr);
    pd_idx = PD_INDEX(vaddr);
    pt_idx = PT_INDEX(vaddr);

    /* Get or create PDPT */
    if (!(kernel_pml4[pml4_idx] & PTE_PRESENT)) {
        page_table_t *new_pdpt = alloc_page_table();
        if (!new_pdpt) {
            kprintf("[VMM] ERROR: Failed to allocate PDPT\n");
            return -1;
        }
        kernel_pml4[pml4_idx] = (uint64_t)new_pdpt | PTE_PRESENT | PTE_WRITABLE;
    }
    pdpt = (pdpte_t *)(kernel_pml4[pml4_idx] & PAGE_MASK);

    /* Get or create PD */
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        page_table_t *new_pd = alloc_page_table();
        if (!new_pd) {
            kprintf("[VMM] ERROR: Failed to allocate PD\n");
            return -1;
        }
        pdpt[pdpt_idx] = (uint64_t)new_pd | PTE_PRESENT | PTE_WRITABLE;
    }
    pdpt = (pdpte_t *)(kernel_pml4[pml4_idx] & PAGE_MASK);
    pd = (pde_t *)(pdpt[pdpt_idx] & PAGE_MASK);

    /* Get or create PT */
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        page_table_t *new_pt = alloc_page_table();
        if (!new_pt) {
            kprintf("[VMM] ERROR: Failed to allocate PT\n");
            return -1;
        }
        pd[pd_idx] = (uint64_t)new_pt | PTE_PRESENT | PTE_WRITABLE;
    }
    pt = (pte_t *)(pd[pd_idx] & PAGE_MASK);

    /* Set page table entry */
    pt[pt_idx] = paddr | flags | PTE_PRESENT;

    /* Flush TLB for this page */
    vmm_flush_tlb(vaddr);

    return 0;
}

/* Map a range of pages */
int vmm_map_range(uint64_t vaddr, uint64_t paddr, uint64_t size, uint64_t flags) {
    uint64_t offset;

    for (offset = 0; offset < size; offset += PAGE_SIZE) {
        if (vmm_map_page(vaddr + offset, paddr + offset, flags) != 0) {
            return -1;
        }
    }
    return 0;
}

/* Unmap a single page */
int vmm_unmap_page(uint64_t vaddr) {
    uint64_t pml4_idx, pdpt_idx, pd_idx, pt_idx;
    pdpte_t *pdpt;
    pde_t *pd;
    pte_t *pt;

    vaddr &= PAGE_MASK;

    pml4_idx = PML4_INDEX(vaddr);
    pdpt_idx = PDPT_INDEX(vaddr);
    pd_idx = PD_INDEX(vaddr);
    pt_idx = PT_INDEX(vaddr);

    /* Walk page tables */
    if (!(kernel_pml4[pml4_idx] & PTE_PRESENT)) {
        return -1;
    }
    pdpt = (pdpte_t *)(kernel_pml4[pml4_idx] & PAGE_MASK);

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        return -1;
    }
    pd = (pde_t *)(pdpt[pdpt_idx] & PAGE_MASK);

    if (!(pd[pd_idx] & PTE_PRESENT)) {
        return -1;
    }
    pt = (pte_t *)(pd[pd_idx] & PAGE_MASK);

    /* Clear page table entry */
    pt[pt_idx] = 0;

    /* Flush TLB */
    vmm_flush_tlb(vaddr);

    return 0;
}

/* Get physical address for virtual address */
uint64_t vmm_get_phys(uint64_t vaddr) {
    uint64_t pml4_idx, pdpt_idx, pd_idx, pt_idx;
    pdpte_t *pdpt;
    pde_t *pd;
    pte_t *pt;

    pml4_idx = PML4_INDEX(vaddr);
    pdpt_idx = PDPT_INDEX(vaddr);
    pd_idx = PD_INDEX(vaddr);
    pt_idx = PT_INDEX(vaddr);

    if (!(kernel_pml4[pml4_idx] & PTE_PRESENT)) {
        return 0;
    }
    pdpt = (pdpte_t *)(kernel_pml4[pml4_idx] & PAGE_MASK);

    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        return 0;
    }
    pd = (pde_t *)(pdpt[pdpt_idx] & PAGE_MASK);

    /* Check for huge page (2MB) */
    if (pd[pd_idx] & PTE_HUGE) {
        return (pd[pd_idx] & ~(HUGE_PAGE_SIZE - 1)) | (vaddr & (HUGE_PAGE_SIZE - 1));
    }

    if (!(pd[pd_idx] & PTE_PRESENT)) {
        return 0;
    }
    pt = (pte_t *)(pd[pd_idx] & PAGE_MASK);

    if (!(pt[pt_idx] & PTE_PRESENT)) {
        return 0;
    }

    return (pt[pt_idx] & PAGE_MASK) | (vaddr & ~PAGE_MASK);
}

/* Initialize VMM - use existing page tables from boot.S */
void vmm_init(void) {
    uint64_t cr3;

    kprintf("[VMM] Initializing virtual memory manager\n");

    /* Get current PML4 from CR3 (set up by boot.S) */
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    kernel_pml4 = (pml4e_t *)(cr3 & PAGE_MASK);

    kprintf("[VMM] Using boot PML4 at 0x%x\n", (uint64_t)kernel_pml4);

    /*
     * The boot.S already sets up identity mapping for the first 4MB
     * using 2MB huge pages. We'll keep using that for now.
     *
     * Current mapping (from boot.S):
     *   PML4[0] -> PDPT[0] -> PD[0] = 2MB huge page (0x000000-0x1FFFFF)
     *                      -> PD[1] = 2MB huge page (0x200000-0x3FFFFF)
     *
     * For now we work with identity mapping.
     * Future: set up high-half kernel mapping.
     */

    kprintf("[VMM] Initialized (identity mapping active)\n");
}
