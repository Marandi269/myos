/*
 * user_space.c - User address space management
 *
 * P-19: Create user address space with separate page tables
 * P-20: Setup user stack
 */

#include "user_space.h"
#include "mm/vmm.h"
#include "mm/pmm.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Create a new user address space
 *
 * This creates a new PML4 table that:
 * 1. Has its own entries for user space (lower half: indices 0-255)
 * 2. Shares kernel mappings (upper half: indices 256-511)
 */
uint64_t *create_user_address_space(void) {
    uint64_t *new_pml4;
    uint64_t *kernel_pml4;
    int i;

    /* Allocate a page for the new PML4 */
    new_pml4 = (uint64_t *)pmm_alloc_page();
    if (!new_pml4) {
        kprintf("[UserSpace] ERROR: Failed to allocate PML4\n");
        return NULL;
    }

    /* Clear the entire table */
    memset(new_pml4, 0, PAGE_SIZE);

    /* Get kernel's PML4 */
    kernel_pml4 = (uint64_t *)vmm_get_kernel_pml4();

    /* Copy kernel mappings (upper half of address space)
     * Indices 256-511 correspond to the upper 128TB of virtual address space
     * This is where the kernel lives (0xFFFF800000000000 and above)
     */
    for (i = 256; i < 512; i++) {
        new_pml4[i] = kernel_pml4[i];
    }

    /* For now, identity-map the first 4MB of physical memory with user access
     * This allows kernel code to execute during the transition to user mode.
     * In a real OS, we would use high-half kernel mapping instead.
     *
     * We create new page table structures with PTE_USER flag set at all levels.
     */
    {
        uint64_t addr;
        /* Map first 4MB (where kernel code resides) */
        for (addr = 0; addr < 0x400000; addr += PAGE_SIZE) {
            vmm_map_page_in(new_pml4, addr, addr, PTE_WRITABLE | PTE_USER);
        }
    }

    kprintf("[UserSpace] Created address space at 0x%lx\n", (uint64_t)new_pml4);
    kprintf("[UserSpace] Identity-mapped first 4MB with user access\n");
    return new_pml4;
}

/* Free a user address space
 *
 * Walk the page tables and free:
 * - User-space pages (not kernel pages)
 * - Page table structures for user space
 */
void free_user_address_space(uint64_t *pml4) {
    int pml4_idx, pdpt_idx, pd_idx, pt_idx;
    uint64_t *pdpt, *pd, *pt;

    if (!pml4) return;

    /* Only free user-space entries (indices 0-255) */
    for (pml4_idx = 0; pml4_idx < 256; pml4_idx++) {
        if (!(pml4[pml4_idx] & PTE_PRESENT)) continue;

        /* Skip identity mapping (index 0) - it's shared with kernel */
        if (pml4_idx == 0) continue;

        pdpt = (uint64_t *)(pml4[pml4_idx] & PAGE_MASK);

        for (pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++) {
            if (!(pdpt[pdpt_idx] & PTE_PRESENT)) continue;

            pd = (uint64_t *)(pdpt[pdpt_idx] & PAGE_MASK);

            for (pd_idx = 0; pd_idx < 512; pd_idx++) {
                if (!(pd[pd_idx] & PTE_PRESENT)) continue;

                /* Check if it's a huge page */
                if (pd[pd_idx] & PTE_HUGE) {
                    /* Can't free huge pages currently */
                    continue;
                }

                pt = (uint64_t *)(pd[pd_idx] & PAGE_MASK);

                /* Free all pages in this page table */
                for (pt_idx = 0; pt_idx < 512; pt_idx++) {
                    if (pt[pt_idx] & PTE_PRESENT) {
                        pmm_free_page((void *)(pt[pt_idx] & PAGE_MASK));
                    }
                }

                /* Free the page table itself */
                pmm_free_page(pt);
            }

            /* Free the page directory */
            pmm_free_page(pd);
        }

        /* Free the PDPT */
        pmm_free_page(pdpt);
    }

    /* Free the PML4 */
    pmm_free_page(pml4);
}

/* Map a page in a specific page table (not the current/kernel one)
 *
 * Similar to vmm_map_page but operates on an arbitrary PML4
 */
int vmm_map_page_in(uint64_t *pml4, uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    uint64_t pml4_idx, pdpt_idx, pd_idx, pt_idx;
    uint64_t *pdpt, *pd, *pt;

    /* Align addresses */
    vaddr &= PAGE_MASK;
    paddr &= PAGE_MASK;

    /* Get indices */
    pml4_idx = PML4_INDEX(vaddr);
    pdpt_idx = PDPT_INDEX(vaddr);
    pd_idx = PD_INDEX(vaddr);
    pt_idx = PT_INDEX(vaddr);

    /* Get or create PDPT */
    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        pdpt = (uint64_t *)pmm_alloc_page();
        if (!pdpt) return -1;
        memset(pdpt, 0, PAGE_SIZE);
        pml4[pml4_idx] = (uint64_t)pdpt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }
    pdpt = (uint64_t *)(pml4[pml4_idx] & PAGE_MASK);

    /* Get or create PD */
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        pd = (uint64_t *)pmm_alloc_page();
        if (!pd) return -1;
        memset(pd, 0, PAGE_SIZE);
        pdpt[pdpt_idx] = (uint64_t)pd | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }
    pd = (uint64_t *)(pdpt[pdpt_idx] & PAGE_MASK);

    /* Get or create PT */
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        pt = (uint64_t *)pmm_alloc_page();
        if (!pt) return -1;
        memset(pt, 0, PAGE_SIZE);
        pd[pd_idx] = (uint64_t)pt | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }
    pt = (uint64_t *)(pd[pd_idx] & PAGE_MASK);

    /* Set the page table entry */
    pt[pt_idx] = paddr | flags | PTE_PRESENT;

    return 0;
}

/* Setup user stack (P-20)
 *
 * Allocates and maps pages for the user stack at USER_STACK_TOP
 */
int setup_user_stack(process_t *proc) {
    uint64_t stack_top = USER_STACK_TOP;
    uint64_t stack_bottom = stack_top - USER_STACK_SIZE;
    uint64_t addr;
    uint64_t phys;

    if (!proc->page_table) {
        kprintf("[UserSpace] ERROR: Process has no page table\n");
        return -1;
    }

    kprintf("[UserSpace] Setting up user stack: 0x%lx - 0x%lx\n",
            stack_bottom, stack_top);

    /* Map stack pages */
    for (addr = stack_bottom; addr < stack_top; addr += PAGE_SIZE) {
        phys = (uint64_t)pmm_alloc_page();
        if (!phys) {
            kprintf("[UserSpace] ERROR: Failed to allocate stack page\n");
            return -1;
        }

        /* Clear the page */
        memset((void *)phys, 0, PAGE_SIZE);

        /* Map with user, writable flags (and NX for security) */
        if (vmm_map_page_in(proc->page_table, addr, phys,
                            PTE_WRITABLE | PTE_USER) != 0) {
            pmm_free_page((void *)phys);
            kprintf("[UserSpace] ERROR: Failed to map stack page\n");
            return -1;
        }
    }

    /* Set user stack pointer (pointing to top of stack, grows down) */
    proc->user_stack = stack_top;

    kprintf("[UserSpace] User stack ready at 0x%lx\n", proc->user_stack);
    return 0;
}

/* Load user code into address space
 *
 * For now this is a simple implementation that:
 * 1. Allocates pages for the code
 * 2. Copies the code to those pages
 * 3. Maps them at USER_CODE_BASE with execute permissions
 */
int load_user_code(process_t *proc, void *code, uint64_t size) {
    uint64_t addr;
    uint64_t phys;
    uint64_t pages_needed;
    uint64_t copied = 0;

    if (!proc->page_table || !code || size == 0) {
        return -1;
    }

    pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    kprintf("[UserSpace] Loading %d bytes of code at 0x%lx (%d pages)\n",
            (int)size, USER_CODE_BASE, (int)pages_needed);

    for (addr = USER_CODE_BASE;
         addr < USER_CODE_BASE + pages_needed * PAGE_SIZE;
         addr += PAGE_SIZE) {

        phys = (uint64_t)pmm_alloc_page();
        if (!phys) {
            kprintf("[UserSpace] ERROR: Failed to allocate code page\n");
            return -1;
        }

        /* Copy code to the page */
        uint64_t to_copy = size - copied;
        if (to_copy > PAGE_SIZE) to_copy = PAGE_SIZE;

        memcpy((void *)phys, (char *)code + copied, to_copy);
        if (to_copy < PAGE_SIZE) {
            /* Zero the rest of the page */
            memset((char *)phys + to_copy, 0, PAGE_SIZE - to_copy);
        }
        copied += to_copy;

        /* Map with user and execute permissions (read-only code) */
        if (vmm_map_page_in(proc->page_table, addr, phys, PTE_USER) != 0) {
            pmm_free_page((void *)phys);
            return -1;
        }
    }

    /* Set entry point */
    proc->user_entry = USER_CODE_BASE;

    kprintf("[UserSpace] Code loaded, entry point: 0x%lx\n", proc->user_entry);
    return 0;
}
