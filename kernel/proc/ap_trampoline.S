/*
 * ap_trampoline.S - Application Processor startup trampoline
 *
 * This code runs in 16-bit real mode when an AP is started via SIPI.
 * It transitions to 32-bit protected mode, then to 64-bit long mode,
 * and finally jumps to the ap_init() C function.
 */

.section .text
.code16

.global ap_trampoline_start
.global ap_trampoline_end

ap_trampoline_start:
    cli

    /* Set segment registers */
    xor     %ax, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %ss

    /* Load a temporary GDT for protected mode transition */
    lgdt    (ap_gdt_ptr - ap_trampoline_start + 0x8000)

    /* Enable protected mode */
    mov     %cr0, %eax
    or      $1, %al
    mov     %eax, %cr0

    /* Far jump to 32-bit protected mode code */
    ljmp    $0x08, $(ap_protected_mode - ap_trampoline_start + 0x8000)

.code32
ap_protected_mode:
    /* Set up 32-bit segment selectors */
    mov     $0x10, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs
    mov     %ax, %ss

    /* Enable PAE */
    mov     %cr4, %eax
    or      $(1 << 5), %eax   /* PAE bit */
    mov     %eax, %cr4

    /* Load page table address (from BSP) */
    /* The BSP stores the PML4 address at 0x8F00 */
    mov     (0x8F00), %eax
    mov     %eax, %cr3

    /* Enable long mode via EFER MSR */
    mov     $0xC0000080, %ecx  /* EFER MSR */
    rdmsr
    or      $(1 << 8), %eax    /* LME bit */
    wrmsr

    /* Enable paging */
    mov     %cr0, %eax
    or      $(1 << 31), %eax   /* PG bit */
    mov     %eax, %cr0

    /* Load 64-bit GDT and jump to 64-bit code */
    lgdt    (ap_gdt64_ptr - ap_trampoline_start + 0x8000)
    ljmp    $0x08, $(ap_long_mode - ap_trampoline_start + 0x8000)

.code64
ap_long_mode:
    /* Set up 64-bit segment selectors */
    mov     $0x10, %ax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs
    mov     %ax, %ss

    /* Load the stack pointer (each AP gets its own stack) */
    /* Stack pointer stored at 0x8F08 */
    mov     (0x8F08), %rsp

    /* Clear direction flag */
    cld

    /* Jump to ap_init in C code */
    /* Address stored at 0x8F10 */
    mov     (0x8F10), %rax
    call    *%rax

    /* Should never reach here */
1:  hlt
    jmp     1b

/* Alignment */
.align 16

/* Temporary 32-bit GDT for protected mode */
ap_gdt:
    .quad   0x0000000000000000  /* Null descriptor */
    .quad   0x00CF9A000000FFFF  /* 32-bit code segment */
    .quad   0x00CF92000000FFFF  /* 32-bit data segment */
ap_gdt_end:

ap_gdt_ptr:
    .word   ap_gdt_end - ap_gdt - 1
    .long   ap_gdt - ap_trampoline_start + 0x8000

/* 64-bit GDT for long mode */
ap_gdt64:
    .quad   0x0000000000000000  /* Null descriptor */
    .quad   0x00AF9A000000FFFF  /* 64-bit code segment */
    .quad   0x00AF92000000FFFF  /* 64-bit data segment */
ap_gdt64_end:

ap_gdt64_ptr:
    .word   ap_gdt64_end - ap_gdt64 - 1
    .long   ap_gdt64 - ap_trampoline_start + 0x8000

ap_trampoline_end:

/*
 * Memory layout at 0x8F00 (set by BSP before SIPI):
 *   0x8F00: PML4 physical address (4 bytes)
 *   0x8F08: Stack pointer for this AP (8 bytes)
 *   0x8F10: ap_init function address (8 bytes)
 */
