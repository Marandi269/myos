/*
 * boot.S - MyOS 64-bit kernel boot code
 *
 * This file handles:
 * 1. Multiboot2 header for GRUB
 * 2. GDT setup for 64-bit mode
 * 3. Page table setup (identity mapping first 2MB)
 * 4. Switch to long mode (64-bit)
 * 5. Jump to kernel_main
 */

/* Multiboot2 constants */
.set MULTIBOOT2_MAGIC,    0xE85250D6
.set MULTIBOOT2_ARCH,     0            /* i386 protected mode */
.set MULTIBOOT2_LENGTH,   (multiboot_header_end - multiboot_header)
.set MULTIBOOT2_CHECKSUM, -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH + MULTIBOOT2_LENGTH)

/* Multiboot2 header - must be at the very beginning */
.section .multiboot, "a"
.align 8
multiboot_header:
    .long MULTIBOOT2_MAGIC
    .long MULTIBOOT2_ARCH
    .long MULTIBOOT2_LENGTH
    .long MULTIBOOT2_CHECKSUM

    /* End tag */
    .align 8
    .word 0     /* type = 0 (end) */
    .word 0     /* flags */
    .long 8     /* size */
multiboot_header_end:

/* 64-bit GDT */
.section .data
.align 16
gdt64:
    .quad 0                         /* Null descriptor */
gdt64_code:
    .quad 0x00AF9A000000FFFF        /* Code segment: executable, readable, 64-bit */
gdt64_data:
    .quad 0x00AF92000000FFFF        /* Data segment: writable, 64-bit */
gdt64_end:

gdt64_pointer:
    .word gdt64_end - gdt64 - 1     /* GDT limit */
    .quad gdt64                      /* GDT base */

/* 32-bit bootstrap code */
.section .text
.code32
.global _start
_start:
    /* Disable interrupts */
    cli

    /* Save multiboot info pointer (in EBX) */
    mov %ebx, (multiboot_info_ptr)

    /* Set up stack */
    mov $stack_top, %esp

    /* Check if CPUID is supported */
    call check_cpuid
    test %eax, %eax
    jz no_long_mode

    /* Check if long mode is supported */
    call check_long_mode
    test %eax, %eax
    jz no_long_mode

    /* Set up page tables */
    call setup_page_tables

    /* Enable PAE (Physical Address Extension) */
    mov %cr4, %eax
    or $0x20, %eax                  /* Set PAE bit */
    mov %eax, %cr4

    /* Load page table */
    mov $pml4, %eax
    mov %eax, %cr3

    /* Enable long mode in EFER MSR */
    mov $0xC0000080, %ecx           /* EFER MSR */
    rdmsr
    or $0x100, %eax                 /* Set LME (Long Mode Enable) bit */
    wrmsr

    /* Enable paging and protected mode */
    mov %cr0, %eax
    or $0x80000001, %eax            /* Set PG and PE bits */
    mov %eax, %cr0

    /* Load 64-bit GDT */
    lgdt (gdt64_pointer)

    /* Jump to 64-bit code segment */
    ljmp $0x08, $long_mode_start

/* Check if CPUID is supported */
check_cpuid:
    pushfl
    pop %eax
    mov %eax, %ecx
    xor $0x200000, %eax             /* Flip ID bit */
    push %eax
    popfl
    pushfl
    pop %eax
    push %ecx
    popfl
    xor %ecx, %eax
    and $0x200000, %eax
    ret

/* Check if long mode is supported */
check_long_mode:
    mov $0x80000000, %eax
    cpuid
    cmp $0x80000001, %eax
    jb .no_long_mode

    mov $0x80000001, %eax
    cpuid
    test $0x20000000, %edx          /* Check LM bit */
    jz .no_long_mode

    mov $1, %eax
    ret

.no_long_mode:
    xor %eax, %eax
    ret

no_long_mode:
    /* Hang if long mode not supported */
    hlt
    jmp no_long_mode

/* Set up identity-mapped page tables for first 2MB */
setup_page_tables:
    /* Clear page tables */
    mov $pml4, %edi
    xor %eax, %eax
    mov $4096, %ecx                 /* 4096 * 4 = 16KB (4 pages) */
    rep stosl

    /* PML4[0] -> PDPT */
    mov $pml4, %edi
    mov $pdpt, %eax
    or $0x03, %eax                  /* Present + Writable */
    mov %eax, (%edi)

    /* PDPT[0] -> PD */
    mov $pdpt, %edi
    mov $pd, %eax
    or $0x03, %eax                  /* Present + Writable */
    mov %eax, (%edi)

    /* PD[0] -> 2MB huge page at 0x0 */
    mov $pd, %edi
    mov $0x83, %eax                 /* Present + Writable + Huge (2MB page) */
    mov %eax, (%edi)

    /* PD[1] -> 2MB huge page at 0x200000 (for kernel at 1MB) */
    mov $0x200083, %eax             /* 2MB + Present + Writable + Huge */
    mov %eax, 8(%edi)

    ret

/* 64-bit code */
.code64
long_mode_start:
    /* Set up data segments */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Set up 64-bit stack */
    mov $stack_top, %rsp

    /* Clear direction flag */
    cld

    /* Call C kernel entry point */
    call kernel_main

    /* If kernel_main returns, halt */
halt:
    cli
    hlt
    jmp halt

/* ISR stub macro for interrupt handlers */
.macro ISR_STUB num
.global isr_stub_\num
isr_stub_\num:
    /* Save all registers */
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    /* Pass interrupt number as first argument */
    mov $\num, %rdi

    /* Call appropriate handler */
    .if \num < 32
        call exception_handler
    .else
        call irq_handler
    .endif

    /* Restore all registers */
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax

    iretq
.endm

/* Generate ISR stubs */
ISR_STUB 0      /* Division by zero */
ISR_STUB 1      /* Debug */
ISR_STUB 2      /* NMI */
ISR_STUB 3      /* Breakpoint */
ISR_STUB 4      /* Overflow */
ISR_STUB 5      /* Bound range exceeded */
ISR_STUB 6      /* Invalid opcode */
ISR_STUB 7      /* Device not available */
ISR_STUB 8      /* Double fault */
ISR_STUB 9      /* Coprocessor segment overrun */
ISR_STUB 10     /* Invalid TSS */
ISR_STUB 11     /* Segment not present */
ISR_STUB 12     /* Stack segment fault */
ISR_STUB 13     /* General protection fault */
ISR_STUB 14     /* Page fault */
ISR_STUB 15     /* Reserved */
ISR_STUB 16     /* x87 FPU error */
ISR_STUB 17     /* Alignment check */
ISR_STUB 18     /* Machine check */
ISR_STUB 19     /* SIMD floating point */

/* IRQ stubs (IRQ0-15 mapped to INT 32-47) */
ISR_STUB 32     /* IRQ0: Timer */
ISR_STUB 33     /* IRQ1: Keyboard */
ISR_STUB 34     /* IRQ2: Cascade */
ISR_STUB 35     /* IRQ3: COM2 */
ISR_STUB 36     /* IRQ4: COM1 */
ISR_STUB 37     /* IRQ5: LPT2 */
ISR_STUB 38     /* IRQ6: Floppy */
ISR_STUB 39     /* IRQ7: LPT1 */
ISR_STUB 40     /* IRQ8: CMOS RTC */
ISR_STUB 41     /* IRQ9: Free */
ISR_STUB 42     /* IRQ10: Free */
ISR_STUB 43     /* IRQ11: Free */
ISR_STUB 44     /* IRQ12: PS/2 Mouse */
ISR_STUB 45     /* IRQ13: FPU */
ISR_STUB 46     /* IRQ14: Primary ATA */
ISR_STUB 47     /* IRQ15: Secondary ATA */

/* BSS section for page tables and stack */
.section .bss
.align 4096

/* Page tables (need 4KB alignment) */
pml4:
    .skip 4096
pdpt:
    .skip 4096
pd:
    .skip 4096
pt:
    .skip 4096

/* Multiboot info pointer storage */
.global multiboot_info_ptr
multiboot_info_ptr:
    .skip 8

/* Kernel stack (16KB) */
.align 16
stack_bottom:
    .skip 16384
stack_top:
