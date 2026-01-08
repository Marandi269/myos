/*
 * syscall.c - System call implementation
 */

#include "syscall.h"
#include "gdt.h"
#include "lib/kprintf.h"

/* Read MSR */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

/* Write MSR */
static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

/* External syscall entry point (in syscall_asm.S) */
extern void syscall_entry(void);

/* Kernel syscall stack (defined in syscall_asm.S) */
extern uint64_t kernel_syscall_stack;

/* Kernel stack for syscalls (8KB) */
static uint8_t syscall_stack[8192] __attribute__((aligned(16)));

/* Initialize SYSCALL/SYSRET */
void syscall_init(void) {
    uint64_t star, efer;

    /*
     * STAR MSR layout:
     * Bits 0-31:  Reserved
     * Bits 32-47: SYSCALL CS/SS (kernel) - CS = this value, SS = this value + 8
     * Bits 48-63: SYSRET CS/SS (user)    - SS = this value + 8, CS = this value + 16
     *
     * Our GDT layout (optimized for SYSRET):
     *   0x08 = Kernel Code
     *   0x10 = Kernel Data
     *   0x18 = User Data   (before User Code!)
     *   0x20 = User Code
     *   0x28 = TSS
     *
     * SYSCALL: CS = STAR[47:32] = 0x08, SS = 0x08 + 8 = 0x10
     * SYSRET:  SS = STAR[63:48] + 8 | 3 = 0x10 + 8 | 3 = 0x1B (User Data)
     *          CS = STAR[63:48] + 16 | 3 = 0x10 + 16 | 3 = 0x23 (User Code)
     */

    /* Initialize kernel syscall stack (stack grows down, so use top of array) */
    kernel_syscall_stack = (uint64_t)&syscall_stack[sizeof(syscall_stack)];
    kprintf("[SYSCALL] Kernel stack at 0x%lx\n", kernel_syscall_stack);

    /* STAR: bits 32-47 = kernel CS (0x08), bits 48-63 = user base (0x10) */
    star = ((uint64_t)GDT_KERNEL_CODE << 32) | ((uint64_t)(GDT_KERNEL_DATA) << 48);
    wrmsr(MSR_STAR, star);

    /* LSTAR: syscall entry point */
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* SFMASK: clear IF and DF on syscall */
    wrmsr(MSR_SFMASK, 0x200 | 0x400);

    /* Enable SYSCALL/SYSRET in EFER */
    efer = rdmsr(MSR_EFER);
    wrmsr(MSR_EFER, efer | EFER_SCE);

    kprintf("[SYSCALL] Initialized\n");
}

/* System call numbers */
#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_GETPID  2

/* System call handler */
uint64_t syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2,
                         uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;

    switch (num) {
        case SYS_EXIT:
            kprintf("[SYSCALL] exit(%d)\n", (int)arg1);
            /* TODO: implement process exit */
            return 0;

        case SYS_WRITE:
            /* write(fd, buf, len) - for now just print to serial */
            if (arg1 == 1) {  /* stdout */
                const char *buf = (const char *)arg2;
                uint64_t len = arg3;
                for (uint64_t i = 0; i < len; i++) {
                    kprintf("%c", buf[i]);
                }
                return len;
            }
            return -1;

        case SYS_GETPID:
            /* TODO: return actual PID */
            return 1;

        default:
            kprintf("[SYSCALL] Unknown syscall %d\n", (int)num);
            return -1;
    }
}
