/*
 * usermode.c - Kernel/User mode transition
 *
 * P-21: Implementation of ring 0 <-> ring 3 switching
 */

#include "usermode.h"
#include "user_space.h"
#include "process.h"
#include "gdt.h"
#include "tss.h"
#include "mm/vmm.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Jump to user mode using iretq
 *
 * Stack frame for iretq:
 *   [RSP+32] SS     (user data segment with RPL=3)
 *   [RSP+24] RSP    (user stack pointer)
 *   [RSP+16] RFLAGS (with IF set to enable interrupts)
 *   [RSP+8]  CS     (user code segment with RPL=3)
 *   [RSP+0]  RIP    (user code entry point)
 */
void jump_to_usermode(uint64_t entry, uint64_t user_stack) {
    kprintf("[UserMode] Jumping to ring 3: RIP=0x%lx RSP=0x%lx\n",
            entry, user_stack);

    __asm__ volatile (
        /* Disable interrupts during transition */
        "cli\n"

        /* Load user data segment into data segment registers */
        "mov %[user_ds], %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"

        /* Build iretq stack frame */
        "push %[user_ss]\n"     /* SS (user data segment with RPL=3) */
        "push %[user_rsp]\n"    /* RSP (user stack) */

        /* RFLAGS: Enable interrupts (IF=1), clear other flags */
        "pushfq\n"
        "pop %%rax\n"
        "or $0x200, %%rax\n"    /* Set IF (bit 9) */
        "and $0xFFFFFFFFFFFEBFFF, %%rax\n"  /* Clear IOPL, NT */
        "push %%rax\n"          /* RFLAGS */

        "push %[user_cs]\n"     /* CS (user code segment with RPL=3) */
        "push %[user_rip]\n"    /* RIP (entry point) */

        /* Clear general purpose registers for security */
        "xor %%rax, %%rax\n"
        "xor %%rbx, %%rbx\n"
        "xor %%rcx, %%rcx\n"
        "xor %%rdx, %%rdx\n"
        "xor %%rsi, %%rsi\n"
        "xor %%rdi, %%rdi\n"
        "xor %%rbp, %%rbp\n"
        "xor %%r8, %%r8\n"
        "xor %%r9, %%r9\n"
        "xor %%r10, %%r10\n"
        "xor %%r11, %%r11\n"
        "xor %%r12, %%r12\n"
        "xor %%r13, %%r13\n"
        "xor %%r14, %%r14\n"
        "xor %%r15, %%r15\n"

        /* Return to user mode! */
        "iretq\n"
        :
        : [user_ds]  "i" (USER_DATA_SEL),
          [user_ss]  "r" ((uint64_t)USER_DATA_SEL),
          [user_cs]  "r" ((uint64_t)USER_CODE_SEL),
          [user_rsp] "r" (user_stack),
          [user_rip] "r" (entry)
        : "rax", "memory"
    );

    /* This should never be reached */
    __builtin_unreachable();
}

/* Switch to a process's address space */
void switch_address_space(process_t *proc) {
    if (!proc || !proc->page_table) {
        return;
    }

    /* Load new page table into CR3 */
    __asm__ volatile (
        "mov %0, %%cr3"
        :
        : "r" ((uint64_t)proc->page_table)
        : "memory"
    );
}

/* Create a user process */
process_t *create_user_process(void *code, uint64_t size, const char *name) {
    process_t *proc;

    kprintf("[UserMode] Creating user process: %s\n", name);

    /* Allocate process structure */
    proc = process_alloc();
    if (!proc) {
        kprintf("[UserMode] ERROR: Failed to allocate process\n");
        return NULL;
    }

    /* Set process name */
    strncpy(proc->name, name, sizeof(proc->name) - 1);
    proc->name[sizeof(proc->name) - 1] = '\0';
    proc->is_user = 1;

    /* Create user address space (P-19) */
    proc->page_table = create_user_address_space();
    if (!proc->page_table) {
        kprintf("[UserMode] ERROR: Failed to create address space\n");
        process_free(proc);
        return NULL;
    }

    /* Setup user stack (P-20) */
    if (setup_user_stack(proc) != 0) {
        kprintf("[UserMode] ERROR: Failed to setup user stack\n");
        free_user_address_space(proc->page_table);
        process_free(proc);
        return NULL;
    }

    /* Load user code */
    if (load_user_code(proc, code, size) != 0) {
        kprintf("[UserMode] ERROR: Failed to load user code\n");
        free_user_address_space(proc->page_table);
        process_free(proc);
        return NULL;
    }

    /* Set TSS RSP0 to this process's kernel stack
     * This is where the CPU will switch to on interrupts from ring 3
     */
    tss_set_rsp0(proc->kernel_stack);

    kprintf("[UserMode] Process %d (%s) created successfully\n",
            proc->pid, proc->name);

    return proc;
}

/*
 * Simple test user program
 *
 * This is raw machine code that:
 * 1. Writes "Hello from user mode!\n" using syscall
 * 2. Exits with code 0
 *
 * The code uses Linux-like syscall conventions:
 * - RAX = syscall number (1 = write, 0 = exit)
 * - RDI = arg1 (fd for write, exit code for exit)
 * - RSI = arg2 (buffer for write)
 * - RDX = arg3 (length for write)
 */
static const uint8_t test_user_code[] = {
    /*
     * Simple user program that writes a message and exits
     *
     * Layout:
     *   +0x00: lea rsi, [rip+0x29]   ; 7 bytes -> RSI = 0x07 + 0x29 = 0x30 (message)
     *   +0x07: mov rax, 1            ; 7 bytes
     *   +0x0E: mov rdi, 1            ; 7 bytes
     *   +0x15: mov rdx, 22           ; 7 bytes
     *   +0x1C: syscall               ; 2 bytes
     *   +0x1E: mov rax, 0            ; 7 bytes
     *   +0x25: mov rdi, 0            ; 7 bytes
     *   +0x2C: syscall               ; 2 bytes
     *   +0x2E: jmp $                 ; 2 bytes
     *   +0x30: "Hello from user mode!\n" (22 bytes)
     */

    /* lea rsi, [rip + 0x29] ; RSI = message address */
    0x48, 0x8D, 0x35, 0x29, 0x00, 0x00, 0x00,  /* +0x00 */

    /* mov rax, 1 ; SYS_WRITE */
    0x48, 0xC7, 0xC0, 0x01, 0x00, 0x00, 0x00,  /* +0x07 */

    /* mov rdi, 1 ; stdout */
    0x48, 0xC7, 0xC7, 0x01, 0x00, 0x00, 0x00,  /* +0x0E */

    /* mov rdx, 22 ; length */
    0x48, 0xC7, 0xC2, 0x16, 0x00, 0x00, 0x00,  /* +0x15 */

    /* syscall */
    0x0F, 0x05,                                 /* +0x1C */

    /* mov rax, 0 ; SYS_EXIT */
    0x48, 0xC7, 0xC0, 0x00, 0x00, 0x00, 0x00,  /* +0x1E */

    /* mov rdi, 0 ; exit code */
    0x48, 0xC7, 0xC7, 0x00, 0x00, 0x00, 0x00,  /* +0x25 */

    /* syscall */
    0x0F, 0x05,                                 /* +0x2C */

    /* jmp $ (infinite loop) */
    0xEB, 0xFE,                                 /* +0x2E */

    /* Message: "Hello from user mode!\n" at +0x30 */
    'H', 'e', 'l', 'l', 'o', ' ', 'f', 'r',
    'o', 'm', ' ', 'u', 's', 'e', 'r', ' ',
    'm', 'o', 'd', 'e', '!', '\n'
};

/* Jump to user mode with address space switch
 *
 * This is a combined function that:
 * 1. Switches to the user's page table (CR3)
 * 2. Immediately performs iretq to user mode
 *
 * We combine these because after switching CR3, the kernel
 * still needs to be accessible (via identity mapping or
 * high-half mapping) to execute the iretq instruction.
 */
void switch_and_jump_to_usermode(uint64_t pml4, uint64_t entry, uint64_t user_stack) {
    /* Enable FPU and SSE for user mode */
    __asm__ volatile (
        /* Clear CR0.EM (bit 2), set CR0.MP (bit 1) */
        "mov %%cr0, %%rax\n"
        "and $~0x4, %%rax\n"       /* Clear EM (no FPU emulation) */
        "or $0x2, %%rax\n"         /* Set MP (monitor coprocessor) */
        "mov %%rax, %%cr0\n"

        /* Set CR4.OSFXSR (bit 9) and CR4.OSXMMEXCPT (bit 10) */
        "mov %%cr4, %%rax\n"
        "or $0x600, %%rax\n"       /* Enable SSE and SSE exceptions */
        "mov %%rax, %%cr4\n"
        : : : "rax"
    );

    __asm__ volatile (
        /* Disable interrupts during transition */
        "cli\n"

        /* Switch page tables - load user's PML4 into CR3 */
        "mov %[pml4], %%cr3\n"

        /* Load user data segment into data segment registers */
        "mov %[user_ds], %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"

        /* Build iretq stack frame */
        "push %[user_ss]\n"     /* SS (user data segment with RPL=3) */
        "push %[user_rsp]\n"    /* RSP (user stack) */

        /* RFLAGS: Enable interrupts (IF=1), clear other flags */
        "pushfq\n"
        "pop %%rax\n"
        "or $0x200, %%rax\n"    /* Set IF (bit 9) */
        "and $0xFFFFFFFFFFFEBFFF, %%rax\n"  /* Clear IOPL, NT */
        "push %%rax\n"          /* RFLAGS */

        "push %[user_cs]\n"     /* CS (user code segment with RPL=3) */
        "push %[user_rip]\n"    /* RIP (entry point) */

        /* Clear general purpose registers for security */
        "xor %%rax, %%rax\n"
        "xor %%rbx, %%rbx\n"
        "xor %%rcx, %%rcx\n"
        "xor %%rdx, %%rdx\n"
        "xor %%rsi, %%rsi\n"
        "xor %%rdi, %%rdi\n"
        "xor %%rbp, %%rbp\n"
        "xor %%r8, %%r8\n"
        "xor %%r9, %%r9\n"
        "xor %%r10, %%r10\n"
        "xor %%r11, %%r11\n"
        "xor %%r12, %%r12\n"
        "xor %%r13, %%r13\n"
        "xor %%r14, %%r14\n"
        "xor %%r15, %%r15\n"

        /* Return to user mode! */
        "iretq\n"
        :
        : [pml4]     "r" (pml4),
          [user_ds]  "i" (USER_DATA_SEL),
          [user_ss]  "r" ((uint64_t)USER_DATA_SEL),
          [user_cs]  "r" ((uint64_t)USER_CODE_SEL),
          [user_rsp] "r" (user_stack),
          [user_rip] "r" (entry)
        : "rax", "memory"
    );

    /* This should never be reached */
    __builtin_unreachable();
}

/* Test user mode transition */
void test_usermode(void) {
    process_t *proc;

    kprintf("\n[TEST] User mode transition test\n");

    /* Create user process with test code */
    proc = create_user_process((void *)test_user_code,
                               sizeof(test_user_code),
                               "test_user");

    if (!proc) {
        kprintf("[TEST] FAILED: Could not create user process\n");
        return;
    }

    kprintf("[TEST] Switching to user process...\n");

    /* Combined switch and jump to user mode
     * This does CR3 load + iretq atomically to avoid
     * issues with kernel code access after page table switch
     */
    switch_and_jump_to_usermode((uint64_t)proc->page_table,
                                 proc->user_entry,
                                 proc->user_stack);

    /* Never reached */
    kprintf("[TEST] ERROR: jump_to_usermode returned!\n");
}
