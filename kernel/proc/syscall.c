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

/* Initialize SYSCALL/SYSRET */
void syscall_init(void) {
    uint64_t star, efer;

    /*
     * STAR MSR layout:
     * Bits 0-31:  Reserved
     * Bits 32-47: SYSCALL CS/SS (kernel) - CS = this value, SS = this value + 8
     * Bits 48-63: SYSRET CS/SS (user) - CS = this value + 16, SS = this value + 8
     *
     * For our GDT:
     *   0x08 = Kernel Code
     *   0x10 = Kernel Data
     *   0x18 = User Code (but SYSRET uses value+16, so we need 0x08)
     *   0x20 = User Data (but SYSRET uses value+8)
     *
     * SYSCALL: CS = STAR[47:32], SS = STAR[47:32] + 8
     * SYSRET:  CS = STAR[63:48] + 16, SS = STAR[63:48] + 8
     *
     * We want:
     *   SYSCALL: CS = 0x08, SS = 0x10 -> STAR[47:32] = 0x08
     *   SYSRET:  CS = 0x18|3 = 0x1B, SS = 0x20|3 = 0x23
     *            -> STAR[63:48] = 0x08 (so CS = 0x08+16 = 0x18, SS = 0x08+8 = 0x10)
     *
     * Wait, SYSRET adds 16 to get CS and 8 to get SS, with RPL=3.
     * So STAR[63:48] should be 0x08:
     *   CS = 0x08 + 16 = 0x18, with RPL=3 -> 0x1B
     *   SS = 0x08 + 8 = 0x10, with RPL=3 -> 0x13
     *
     * But we want SS = 0x20|3 = 0x23. So STAR[63:48] should be 0x18:
     *   CS = 0x18 + 16 = 0x28 (wrong!)
     *
     * Actually the layout for SYSRET is different. Let me check again:
     * SYSRET loads: SS = STAR[63:48] + 8, CS = STAR[63:48] + 16
     * Both get RPL=3 automatically.
     *
     * Our GDT: 0x00=null, 0x08=kcode, 0x10=kdata, 0x18=udata, 0x20=ucode, 0x28=tss
     *
     * Wait, the standard layout is: ucode before udata. Let me use:
     * 0x18 = User Code
     * 0x20 = User Data
     *
     * For SYSRET: STAR[63:48] = 0x10 (which is user base - 8)
     *   SS = 0x10 + 8 = 0x18 | 3 = 0x1B  -- but that's user code!
     *
     * The AMD64 ABI expects: user code = 0x23, user data = 0x2B
     * Which means: GDT[4] = user code, GDT[5] = user data
     *
     * Simpler approach: Match Linux layout
     *   0x08 = Kernel Code
     *   0x10 = Kernel Data
     *   0x18 = User Data  (reversed!)
     *   0x20 = User Code  (reversed!)
     *
     * Then STAR[63:48] = 0x10:
     *   SS.sel = 0x10 + 8 = 0x18 | 3 = 0x1B (user data) ✓
     *   CS.sel = 0x10 + 16 = 0x20 | 3 = 0x23 (user code) ✓
     */

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
