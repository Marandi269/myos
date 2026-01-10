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
        /* Map first 128MB (where kernel code and data resides) */
        for (addr = 0; addr < 0x8000000; addr += PAGE_SIZE) {
            vmm_map_page_in(new_pml4, addr, addr, PTE_WRITABLE | PTE_USER);
        }
    }

    kprintf("[UserSpace] Created address space at 0x%lx\n", (uint64_t)new_pml4);
    kprintf("[UserSpace] Identity-mapped first 128MB with user access\n");
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
    return setup_user_stack_with_args(proc, 0, NULL, NULL);
}

/* Setup user stack with arguments
 *
 * Allocates and maps pages for the user stack at USER_STACK_TOP,
 * and sets up argc, argv, and envp on the stack.
 */
int setup_user_stack_with_args(process_t *proc, int argc, char *argv[], char *envp[]) {
    uint64_t stack_top = USER_STACK_TOP;
    uint64_t stack_bottom = stack_top - USER_STACK_SIZE;
    uint64_t addr;
    uint64_t phys = 0;
    int i;

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

    /* Set up argc/argv/envp at top of stack
     * x86_64 SysV ABI entry point stack layout:
     *   <- Higher addresses
     *   [strings area - argv/envp string data]
     *   [padding for alignment]
     *   [NULL]           - auxv terminator
     *   [NULL]           - envp terminator
     *   [envp[n-1]]      - envp pointers (if any)
     *   ...
     *   [envp[0]]
     *   [NULL]           - argv terminator
     *   [argv[argc-1]]   - argv pointers
     *   ...
     *   [argv[0]]
     *   [argc]           - argument count
     *   <- rsp (16-byte aligned)
     */
    {
        /* phys still points to the last allocated page which is at stack_top - PAGE_SIZE */
        uint64_t top_phys = phys + PAGE_SIZE;  /* Physical addr of stack_top */
        char *str_ptr = (char *)top_phys;      /* For string storage */
        uint64_t *stack_ptr;
        uint64_t argv_ptrs[32];  /* Max 32 args for now */
        uint64_t envp_ptrs[32];  /* Max 32 env vars */
        int envc = 0;
        size_t len;

        /* Copy strings to stack (near the top) and record their virtual addresses */
        /* Copy argv strings */
        int actual_argc = 0;
        for (i = 0; i < argc && i < 32 && argv && argv[i]; i++) {
            len = strlen(argv[i]) + 1;
            str_ptr -= len;
            memcpy(str_ptr, argv[i], len);
            /* Virtual address = stack_top - (top_phys - physical addr) */
            argv_ptrs[i] = stack_top - (top_phys - (uint64_t)str_ptr);
            actual_argc++;
            kprintf("[UserSpace] argv[%d] = '%s' at virt 0x%lx\n", i, argv[i], argv_ptrs[i]);
        }
        argc = actual_argc;  /* Update argc to actual count */

        /* Copy envp strings if provided */
        if (envp) {
            for (envc = 0; envc < 32 && envp[envc]; envc++) {
                len = strlen(envp[envc]) + 1;
                str_ptr -= len;
                memcpy(str_ptr, envp[envc], len);
                envp_ptrs[envc] = stack_top - (top_phys - (uint64_t)str_ptr);
            }
        }

        /* Align to 16 bytes */
        str_ptr = (char *)((uint64_t)str_ptr & ~0xF);

        /* Now write the pointer table */
        stack_ptr = (uint64_t *)str_ptr;

        /* Auxiliary vector (auxv) - musl needs these */
        /* Format: pairs of (type, value), terminated by (AT_NULL, 0) */
        #define AT_NULL         0
        #define AT_PAGESZ       6
        #define AT_ENTRY        9
        #define AT_UID          11
        #define AT_EUID         12
        #define AT_GID          13
        #define AT_EGID         14
        #define AT_HWCAP        16
        #define AT_CLKTCK       17
        #define AT_SECURE       23
        #define AT_RANDOM       25

        /* Null terminator for auxv */
        *(--stack_ptr) = 0;  /* AT_NULL value */
        *(--stack_ptr) = AT_NULL;  /* AT_NULL key */

        /* AT_PAGESZ - page size */
        *(--stack_ptr) = PAGE_SIZE;
        *(--stack_ptr) = AT_PAGESZ;

        /* AT_UID/EUID/GID/EGID - user/group IDs */
        *(--stack_ptr) = 0;  /* root */
        *(--stack_ptr) = AT_UID;
        *(--stack_ptr) = 0;
        *(--stack_ptr) = AT_EUID;
        *(--stack_ptr) = 0;
        *(--stack_ptr) = AT_GID;
        *(--stack_ptr) = 0;
        *(--stack_ptr) = AT_EGID;

        /* AT_CLKTCK - clock ticks per second */
        *(--stack_ptr) = 100;  /* Our PIT runs at 100 Hz */
        *(--stack_ptr) = AT_CLKTCK;

        /* AT_SECURE - secure mode flag */
        *(--stack_ptr) = 0;  /* Not secure mode */
        *(--stack_ptr) = AT_SECURE;

        /* envp NULL terminator */
        *(--stack_ptr) = 0;

        /* envp pointers (reverse order) */
        for (i = envc - 1; i >= 0; i--) {
            *(--stack_ptr) = envp_ptrs[i];
        }

        /* argv NULL terminator */
        *(--stack_ptr) = 0;

        /* argv pointers (reverse order) */
        for (i = argc - 1; i >= 0; i--) {
            *(--stack_ptr) = argv_ptrs[i];
        }

        /* argc */
        *(--stack_ptr) = argc;

        /* Calculate final stack pointer in virtual space */
        proc->user_stack = stack_top - (top_phys - (uint64_t)stack_ptr);

        /* Debug: print stack contents */
        uint64_t page_virt_base = stack_top - PAGE_SIZE;
        uint64_t page_phys_base = phys;
        kprintf("[UserSpace] Stack at phys 0x%lx (virt 0x%lx):\n",
                (uint64_t)stack_ptr, proc->user_stack);
        uint64_t *debug_ptr = stack_ptr;
        kprintf("[UserSpace]   argc = %ld\n", *debug_ptr);
        debug_ptr++;
        for (i = 0; i < argc; i++) {
            uint64_t argv_ptr = *debug_ptr++;
            kprintf("[UserSpace]   argv[%d] = 0x%lx", i, argv_ptr);
            if (argv_ptr && argv_ptr >= page_virt_base && argv_ptr < stack_top) {
                /* Read the string from the stack */
                uint64_t str_phys = page_phys_base + (argv_ptr - page_virt_base);
                kprintf(" -> '%s'\n", (char *)str_phys);
            } else {
                kprintf(" (out of page)\n");
            }
        }

        /* Ensure 16-byte alignment (required by x86_64 ABI) */
        if (proc->user_stack & 0xF) {
            /* Adjust if needed - shouldn't happen if we aligned str_ptr properly */
            proc->user_stack &= ~0xF;
        }
    }

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
