/*
 * page_fault.c - Page fault exception handler
 */

#include "page_fault.h"
#include "vmm.h"
#include "pmm.h"
#include "lib/kprintf.h"

/* Get faulting address from CR2 */
static inline uint64_t get_cr2(void) {
    uint64_t cr2;
    __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
    return cr2;
}

/* Page fault handler */
void page_fault_handler(uint64_t error_code) {
    uint64_t fault_addr = get_cr2();

    kprintf("\n[PAGE FAULT] Address: 0x%x, Error: 0x%x\n",
            fault_addr, error_code);

    /* Analyze error type */
    if (error_code & PF_PRESENT) {
        kprintf("  - Protection violation (page present)\n");
    } else {
        kprintf("  - Page not present\n");
    }

    if (error_code & PF_WRITE) {
        kprintf("  - Write access\n");
    } else {
        kprintf("  - Read access\n");
    }

    if (error_code & PF_USER) {
        kprintf("  - User mode\n");
    } else {
        kprintf("  - Kernel mode\n");
    }

    if (error_code & PF_FETCH) {
        kprintf("  - Instruction fetch\n");
    }

    if (error_code & PF_RESERVED) {
        kprintf("  - Reserved bit set in page entry\n");
    }

    /*
     * TODO: Implement demand paging
     * 1. Check if address is in valid range
     * 2. Allocate physical page
     * 3. Map the page
     * 4. Return to continue execution
     *
     * For now: kernel page fault = panic
     */

    if (!(error_code & PF_USER)) {
        kprintf("\n[PANIC] Kernel page fault at 0x%x!\n", fault_addr);
        kprintf("System halted.\n");
        while (1) {
            __asm__ volatile ("cli; hlt");
        }
    }

    /* User mode page fault - would kill the process */
    kprintf("[PAGE FAULT] User process would be terminated\n");
}
