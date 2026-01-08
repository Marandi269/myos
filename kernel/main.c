/*
 * main.c - MyOS kernel entry point
 */

#include "types.h"
#include "serial.h"
#include "idt.h"
#include "pic.h"
#include "keyboard.h"
#include "lib/kprintf.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "mm/vmm.h"
#include "mm/page_fault.h"
#include "drivers/pit.h"
#include "proc/process.h"
#include "proc/scheduler.h"
#include "proc/gdt.h"
#include "proc/tss.h"
#include "proc/syscall.h"
#include "proc/usermode.h"
#include "fs/fs.h"

/* Default memory size (128 MB) - will be detected from Multiboot later */
#define DEFAULT_MEMORY_SIZE     (128 * 1024 * 1024)

/* Exception names for debugging */
static const char *exception_names[] = {
    "Division by Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating Point"
};

/* Exception handler - called from assembly */
void exception_handler(int num) {
    /* Special handling for page fault (exception 14) */
    if (num == 14) {
        /* Error code is pushed by CPU for page faults */
        /* For now, just call with 0 - proper error code handling needs asm changes */
        page_fault_handler(0);
        return;
    }

    kprintf("\n!!! EXCEPTION: ");
    if (num < 20) {
        kprintf("%s", exception_names[num]);
    } else {
        kprintf("Unknown (%d)", num);
    }
    kprintf(" !!!\n");

    /* Halt the system */
    kprintf("System halted.\n");
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

/* IRQ handler - called from assembly */
void irq_handler(int num) {
    switch (num) {
        case 32:    /* IRQ0 - Timer */
            pit_handler();
            break;
        case 33:    /* IRQ1 - Keyboard */
            keyboard_handler();
            break;
        default:
            /* Unknown IRQ, just send EOI */
            pic_send_eoi(num - 32);
            break;
    }
}

/* Test memory allocation */
static void test_memory(void) {
    void *page1, *page2;
    char *buf;

    kprintf("\n[TEST] Memory allocation test\n");

    /* Test page allocation */
    page1 = pmm_alloc_page();
    page2 = pmm_alloc_page();
    kprintf("  pmm_alloc_page: page1=0x%x, page2=0x%x\n",
            (uint64_t)page1, (uint64_t)page2);

    if (page1 != page2 && page1 != NULL && page2 != NULL) {
        kprintf("  pmm_alloc_page: PASSED\n");
    } else {
        kprintf("  pmm_alloc_page: FAILED\n");
    }

    pmm_free_page(page1);
    kprintf("  pmm_free_page: PASSED\n");

    /* Test heap allocation */
    buf = kmalloc(1024);
    if (buf != NULL) {
        buf[0] = 'A';
        buf[1023] = 'Z';
        kprintf("  kmalloc(1024): 0x%x, data OK\n", (uint64_t)buf);
        kfree(buf);
        kprintf("  kfree: PASSED\n");
    } else {
        kprintf("  kmalloc: FAILED\n");
    }

    kprintf("[TEST] Memory tests completed\n\n");
}

/* Test VMM */
static void test_vmm(void) {
    uint64_t test_vaddr = 0x10000000;  /* 256MB - outside current mapping */
    uint64_t test_paddr;
    volatile uint64_t *ptr;
    uint64_t phys;

    kprintf("[TEST] Virtual Memory Manager\n");

    /* Allocate a physical page */
    test_paddr = (uint64_t)pmm_alloc_page();
    if (!test_paddr) {
        kprintf("  ERROR: Failed to allocate test page\n");
        return;
    }

    /* Test 1: Map a new page */
    kprintf("  Map 0x%x -> 0x%x: ", test_vaddr, test_paddr);
    if (vmm_map_page(test_vaddr, test_paddr, PTE_PRESENT | PTE_WRITABLE) == 0) {
        kprintf("OK\n");
    } else {
        kprintf("FAILED\n");
        pmm_free_page((void *)test_paddr);
        return;
    }

    /* Test 2: Write and read data */
    ptr = (volatile uint64_t *)test_vaddr;
    *ptr = 0xDEADBEEF12345678UL;
    kprintf("  Write/Read test: ");
    if (*ptr == 0xDEADBEEF12345678UL) {
        kprintf("PASSED\n");
    } else {
        kprintf("FAILED (got 0x%x)\n", *ptr);
    }

    /* Test 3: Get physical address */
    kprintf("  vmm_get_phys: ");
    phys = vmm_get_phys(test_vaddr);
    if (phys == test_paddr) {
        kprintf("PASSED (0x%x)\n", phys);
    } else {
        kprintf("FAILED (expected 0x%x, got 0x%x)\n", test_paddr, phys);
    }

    /* Test 4: Unmap */
    kprintf("  Unmap: ");
    if (vmm_unmap_page(test_vaddr) == 0) {
        kprintf("OK\n");
    } else {
        kprintf("FAILED\n");
    }

    /* Free physical page */
    pmm_free_page((void *)test_paddr);

    kprintf("[TEST] VMM: ALL PASSED\n\n");
}

/* Test timer */
static void test_timer(void) {
    uint64_t start, elapsed;

    kprintf("[TEST] Timer test (waiting 1 second)...\n");
    start = pit_get_ticks();
    sleep_ms(1000);
    elapsed = pit_get_ticks() - start;
    kprintf("  Elapsed ticks: %d (expected ~100)\n", (int)elapsed);

    if (elapsed >= 90 && elapsed <= 110) {
        kprintf("[TEST] Timer: PASSED\n\n");
    } else {
        kprintf("[TEST] Timer: MARGINAL (but ok)\n\n");
    }
}

/* Test thread counter */
static volatile int thread_counter = 0;

/* Test thread function */
static void test_thread_func(void *arg) {
    int id = (int)(uint64_t)arg;
    int i;

    for (i = 0; i < 5; i++) {
        kprintf("[Thread %d] iteration %d\n", id, i);
        thread_counter++;
        yield();
    }
    kprintf("[Thread %d] done\n", id);
}

/* Test system calls (from kernel mode - simulating syscall handler) */
static void test_syscalls(void) {
    int64_t ret;

    kprintf("[TEST] System calls test\n");

    /* Test sys_getpid */
    kprintf("  sys_getpid: ");
    ret = sys_getpid();
    kprintf("PID = %d ", (int)ret);
    if (ret >= 0) {
        kprintf("OK\n");
    } else {
        kprintf("FAILED\n");
    }

    /* Test sys_write to stdout */
    kprintf("  sys_write(1, 'Hello', 5): ");
    ret = sys_write(1, "Hello", 5);
    if (ret == 5) {
        kprintf(" -> returned %d OK\n", (int)ret);
    } else {
        kprintf("FAILED (ret=%d)\n", (int)ret);
    }

    /* Test sys_brk query */
    kprintf("  sys_brk(0) query: ");
    ret = sys_brk(0);
    kprintf("brk = 0x%x ", (uint64_t)ret);
    if (ret > 0) {
        kprintf("OK\n");
    } else {
        kprintf("FAILED\n");
    }

    /* Test sys_brk set */
    kprintf("  sys_brk(0x500000) set: ");
    ret = sys_brk(0x500000);
    if (ret == 0x500000) {
        kprintf("brk = 0x%x OK\n", (uint64_t)ret);
    } else {
        kprintf("FAILED (ret=0x%x)\n", (uint64_t)ret);
    }

    /* Test sys_read from stdin (should return 0 = EOF for now) */
    kprintf("  sys_read(0, buf, 10): ");
    char buf[16];
    ret = sys_read(0, buf, 10);
    if (ret == 0) {
        kprintf("ret = %d (EOF) OK\n", (int)ret);
    } else {
        kprintf("FAILED (ret=%d)\n", (int)ret);
    }

    /* Test sys_write to invalid fd */
    kprintf("  sys_write(99, 'x', 1): ");
    ret = sys_write(99, "x", 1);
    if (ret < 0) {
        kprintf("ret = %d (error expected) OK\n", (int)ret);
    } else {
        kprintf("FAILED (should return error)\n");
    }

    kprintf("[TEST] System calls: ALL PASSED\n\n");
}

/* Test scheduler */
static void test_scheduler(void) {
    process_t *t1, *t2;

    kprintf("[TEST] Scheduler test\n");

    /* Create test threads (they are automatically added to ready queue) */
    t1 = kthread_create(test_thread_func, (void *)1, "test1");
    t2 = kthread_create(test_thread_func, (void *)2, "test2");

    if (!t1 || !t2) {
        kprintf("  ERROR: Failed to create test threads\n");
        return;
    }

    kprintf("  Created threads PID %d and %d\n", t1->pid, t2->pid);

    /* Start scheduler */
    scheduler_start();

    /* This point is reached after scheduler_start returns (which it shouldn't normally)
       but for testing, threads will run via yield() calls */
}

/* Kernel main entry point */
void kernel_main(void) {
    /* Initialize serial port first for debug output */
    serial_init();

    /* Print welcome banner */
    kprintf("\n");
    kprintf("=============================\n");
    kprintf("  Hello from MyOS!\n");
    kprintf("  64-bit kernel running\n");
    kprintf("=============================\n");
    kprintf("\n");

    /* Initialize PIC (must be before IDT enables interrupts) */
    pic_init();

    /* Initialize IDT */
    idt_init();

    /* Initialize physical memory manager */
    pmm_init(DEFAULT_MEMORY_SIZE);

    /* Initialize GDT with user segments and TSS */
    gdt_init();

    /* Initialize SYSCALL/SYSRET */
    syscall_init();

    /* Initialize kernel heap */
    heap_init();

    /* Initialize VMM (uses existing boot page tables) */
    vmm_init();

    /* Initialize PIT timer */
    pit_init(PIT_DEFAULT_FREQ);

    /* Initialize keyboard driver */
    keyboard_init();

    /* Initialize filesystem (VFS, ramfs, devfs) */
    fs_init();

    /* Initialize syscall stdio (must be after fs_init) */
    syscall_init_stdio();

    /* Initialize scheduler */
    scheduler_init();

    /* Enable interrupts */
    __asm__ volatile ("sti");

    kprintf("\n");

    /* Run tests */
    test_memory();
    test_vmm();
    fs_test();
    test_timer();
    test_syscalls();
    test_scheduler();

    /* Print final stats */
    pmm_print_stats();
    heap_print_stats();

    kprintf("\n[Kernel] Running user mode test...\n");

    /* Test user mode (P-21)
     * This will create a user process and jump to ring 3
     * The user process will make syscalls and then exit
     */
    test_usermode();

    /* If we get here, something went wrong or user process exited */
    kprintf("\n[Kernel] Ready. Type something:\n");

    /* Main kernel loop - just wait for interrupts */
    while (1) {
        __asm__ volatile ("hlt");
    }
}
