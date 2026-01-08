/*
 * gdt_asm.S - GDT flush assembly routine
 */

.global gdt_flush
.type gdt_flush, @function

gdt_flush:
    /* Load GDT */
    lgdt (%rdi)

    /* Reload code segment via far return */
    pushq $0x08         /* Kernel code segment */
    leaq .reload_cs(%rip), %rax
    pushq %rax
    lretq

.reload_cs:
    /* Reload data segments */
    movw $0x10, %ax     /* Kernel data segment */
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss

    ret
