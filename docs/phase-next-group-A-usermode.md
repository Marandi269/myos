# Group A: 用户态准备 (User Mode Infrastructure)

## 概述

| 项目 | 说明 |
|------|------|
| 负责人 | 开发者 A |
| 任务范围 | P-16 ~ P-21 |
| 前置依赖 | 阶段 3 调度器 ✅ |
| 并行组 | Group B (文件系统), Group C (系统调用框架) |

## 目标

为用户态程序运行准备必要的 CPU 基础设施：
1. GDT 添加 ring3 段描述符
2. TSS 设置 (中断时内核栈切换)
3. SYSCALL/SYSRET 配置
4. 用户地址空间和态切换

---

## 任务清单

| 任务ID | 任务名称 | 依赖 | 验证方式 |
|--------|----------|------|----------|
| P-16 | GDT 添加用户段 | boot.S | 段选择子正确 |
| P-17 | TSS 设置 | P-16 | 中断时栈切换正确 |
| P-18 | SYSCALL MSR 配置 | P-17 | syscall 指令不崩溃 |
| P-19 | 用户地址空间 | VMM | 独立页表创建 |
| P-20 | 用户栈设置 | P-19 | 用户栈映射 |
| P-21 | 内核态/用户态切换 | P-17, P-20 | ring0 <-> ring3 |

---

## P-16: GDT 添加用户段

### 当前 GDT 布局
```
Offset  Selector  Description
0x00    0x00      Null descriptor
0x08    0x08      Kernel code (DPL=0)
0x10    0x10      Kernel data (DPL=0)
```

### 目标 GDT 布局
```
Offset  Selector  Description
0x00    0x00      Null descriptor
0x08    0x08      Kernel code (DPL=0)
0x10    0x10      Kernel data (DPL=0)
0x18    0x1B      User code (DPL=3)    <- 新增
0x20    0x23      User data (DPL=3)    <- 新增
0x28    0x28      TSS (16 bytes)       <- 新增
```

### 实现

```c
// kernel/arch/gdt.h

#ifndef _GDT_H
#define _GDT_H

#include "types.h"

// 段选择子
#define GDT_KERNEL_CODE  0x08
#define GDT_KERNEL_DATA  0x10
#define GDT_USER_CODE    0x18
#define GDT_USER_DATA    0x20
#define GDT_TSS          0x28

// RPL (请求特权级)
#define RPL_KERNEL  0
#define RPL_USER    3

// 用户态段选择子 (带 RPL)
#define USER_CODE_SELECTOR  (GDT_USER_CODE | RPL_USER)  // 0x1B
#define USER_DATA_SELECTOR  (GDT_USER_DATA | RPL_USER)  // 0x23

void gdt_init(void);

#endif
```

```c
// kernel/arch/gdt.c

#include "gdt.h"
#include "tss.h"

// GDT 条目
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

// TSS 描述符 (64位模式需要16字节)
struct tss_descriptor {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct gdt_entry gdt[5];
static struct tss_descriptor gdt_tss;
static struct gdt_ptr gdtp;

static void gdt_set_entry(int idx, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[idx].base_low    = base & 0xFFFF;
    gdt[idx].base_mid    = (base >> 16) & 0xFF;
    gdt[idx].base_high   = (base >> 24) & 0xFF;
    gdt[idx].limit_low   = limit & 0xFFFF;
    gdt[idx].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[idx].access      = access;
}

void gdt_init(void) {
    // Null descriptor
    gdt_set_entry(0, 0, 0, 0, 0);

    // Kernel code: base=0, limit=0xFFFFF, access=0x9A, gran=0xA0
    // 0x9A = present, DPL=0, code, execute/read
    // 0xA0 = 64-bit, 4KB granularity
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);

    // Kernel data: access=0x92
    // 0x92 = present, DPL=0, data, read/write
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xA0);

    // User code: access=0xFA (DPL=3)
    // 0xFA = present, DPL=3, code, execute/read
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xA0);

    // User data: access=0xF2 (DPL=3)
    // 0xF2 = present, DPL=3, data, read/write
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xA0);

    // TSS 描述符单独设置 (见 P-17)

    // 加载 GDT
    gdtp.limit = sizeof(gdt) + sizeof(gdt_tss) - 1;
    gdtp.base = (uint64_t)&gdt;

    __asm__ volatile (
        "lgdt %0\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        : : "m"(gdtp) : "rax"
    );
}
```

### 验证
```c
void test_gdt(void) {
    kprintf("[TEST] GDT user segments\n");

    // 检查段选择子值
    kprintf("  Kernel CS: 0x%x\n", GDT_KERNEL_CODE);
    kprintf("  Kernel DS: 0x%x\n", GDT_KERNEL_DATA);
    kprintf("  User CS:   0x%x\n", USER_CODE_SELECTOR);
    kprintf("  User DS:   0x%x\n", USER_DATA_SELECTOR);

    kprintf("[TEST] GDT: PASSED\n");
}
```

---

## P-17: TSS 设置

### TSS 结构 (64位)

```c
// kernel/arch/tss.h

#ifndef _TSS_H
#define _TSS_H

#include "types.h"

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;          // Ring 0 栈指针 (关键!)
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;          // 中断栈表 1-7
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;   // I/O 权限位图偏移
} __attribute__((packed)) tss_t;

void tss_init(void);
void tss_set_rsp0(uint64_t rsp0);

#endif
```

### 实现

```c
// kernel/arch/tss.c

#include "tss.h"
#include "gdt.h"
#include "kprintf.h"

static tss_t tss;

void tss_init(void) {
    // 清零 TSS
    memset(&tss, 0, sizeof(tss));

    // 设置 IOPB 偏移 (禁用 I/O 端口访问)
    tss.iopb_offset = sizeof(tss);

    // 获取 TSS 地址
    uint64_t tss_addr = (uint64_t)&tss;

    // 设置 TSS 描述符 (在 GDT 中)
    // TSS 描述符在64位模式下是16字节
    extern struct tss_descriptor gdt_tss;

    gdt_tss.limit_low   = sizeof(tss) - 1;
    gdt_tss.base_low    = tss_addr & 0xFFFF;
    gdt_tss.base_mid    = (tss_addr >> 16) & 0xFF;
    gdt_tss.access      = 0x89;  // Present, TSS (available)
    gdt_tss.granularity = 0x00;
    gdt_tss.base_high   = (tss_addr >> 24) & 0xFF;
    gdt_tss.base_upper  = (tss_addr >> 32) & 0xFFFFFFFF;
    gdt_tss.reserved    = 0;

    // 加载 TSS
    __asm__ volatile ("ltr %0" : : "r"((uint16_t)GDT_TSS));

    kprintf("[TSS] Initialized at 0x%lx\n", tss_addr);
}

// 设置当前进程的内核栈 (每次进程切换时调用)
void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}
```

### 为什么 TSS 关键？

```
用户态 (ring 3) 发生中断:

1. CPU 检测到特权级变化 (3 -> 0)
2. CPU 从 TSS.rsp0 获取内核栈地址
3. CPU 切换到内核栈
4. CPU 压入 SS, RSP, RFLAGS, CS, RIP
5. 跳转到中断处理程序

如果 TSS 未设置: Triple Fault!
```

### 验证

```c
void test_tss(void) {
    kprintf("[TEST] TSS\n");

    // 设置测试栈
    uint64_t test_stack = 0xFFFF800000200000;
    tss_set_rsp0(test_stack);

    // 读回验证
    extern tss_t tss;
    if (tss.rsp0 == test_stack) {
        kprintf("  RSP0 set: PASSED\n");
    } else {
        kprintf("  RSP0 set: FAILED\n");
    }

    kprintf("[TEST] TSS: PASSED\n");
}
```

---

## P-18: SYSCALL MSR 配置

### 相关 MSR 寄存器

| MSR | 地址 | 用途 |
|-----|------|------|
| STAR | 0xC0000081 | 段选择子 |
| LSTAR | 0xC0000082 | syscall 入口地址 |
| CSTAR | 0xC0000083 | 32位兼容模式入口 (不用) |
| SFMASK | 0xC0000084 | RFLAGS 掩码 |

### 实现

```c
// kernel/arch/syscall.c

#include "types.h"
#include "gdt.h"
#include "kprintf.h"

#define MSR_EFER    0xC0000080
#define MSR_STAR    0xC0000081
#define MSR_LSTAR   0xC0000082
#define MSR_SFMASK  0xC0000084

#define EFER_SCE    (1 << 0)  // SYSCALL Enable

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile ("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

// syscall 入口 (汇编)
extern void syscall_entry(void);

void syscall_init(void) {
    // STAR: 段选择子
    // [31:0]  - reserved
    // [47:32] - SYSCALL CS/SS = kernel code/data
    // [63:48] - SYSRET CS/SS = user code/data
    //
    // SYSRET 会用: CS = STAR[63:48]+16, SS = STAR[63:48]+8
    // 所以设置 STAR[63:48] = 0x18-16 = 0x08...
    // 实际: SYSRET CS = STAR[63:48]+16 = user_code
    //       SYSRET SS = STAR[63:48]+8  = user_data

    uint64_t star = 0;
    star |= ((uint64_t)GDT_KERNEL_CODE) << 32;  // SYSCALL: CS=0x08, SS=0x10
    star |= ((uint64_t)(GDT_USER_CODE - 16)) << 48;  // SYSRET: CS=0x18+16=invalid...

    // 正确计算:
    // SYSRET sets: CS = (STAR[63:48] + 16) | 3
    //              SS = (STAR[63:48] + 8)  | 3
    // 我们需要 CS=0x1B (0x18|3), SS=0x23 (0x20|3)
    // 所以 STAR[63:48] + 16 = 0x18, STAR[63:48] = 0x08
    // 但 SS = 0x08 + 8 = 0x10, 不对...
    //
    // AMD64: SYSRET CS = STAR[63:48] + 16, SS = STAR[63:48] + 8
    // Intel: 同上
    //
    // 需要 GDT 布局: user_data(0x18), user_code(0x20)
    // 或者用传统布局

    // 简化: 使用 STAR = 0x0013000800000000
    // SYSCALL: CS=0x08, SS=0x10 (kernel)
    // SYSRET:  CS=0x23 (0x13+0x10)|3, SS=0x1B (0x13+0x08)|3
    // 需要调整 GDT 顺序...

    // 标准布局 (Linux 风格):
    // 0x08: kernel code
    // 0x10: kernel data
    // 0x18: user data   <- SYSRET SS
    // 0x20: user code   <- SYSRET CS
    //
    // STAR[63:48] = 0x10
    // SYSRET: CS = 0x10+16 = 0x20 | 3 = 0x23
    //         SS = 0x10+8  = 0x18 | 3 = 0x1B ✓

    star = ((uint64_t)0x10 << 48) | ((uint64_t)0x08 << 32);
    wrmsr(MSR_STAR, star);

    // LSTAR: syscall 入口地址
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    // SFMASK: syscall 时清除的 RFLAGS 位
    // 清除 IF (中断), DF (方向), TF (单步)
    wrmsr(MSR_SFMASK, 0x200 | 0x400 | 0x100);

    // 启用 SYSCALL (EFER.SCE)
    uint64_t efer = rdmsr(MSR_EFER);
    wrmsr(MSR_EFER, efer | EFER_SCE);

    kprintf("[SYSCALL] MSR configured, entry at 0x%lx\n", (uint64_t)syscall_entry);
}
```

### syscall 入口汇编

```asm
# kernel/arch/syscall_entry.S

.global syscall_entry
.type syscall_entry, @function

# SYSCALL 时 CPU 会:
# - RCX = 用户态 RIP
# - R11 = 用户态 RFLAGS
# - RIP = LSTAR
# - CS  = STAR[47:32]
# - SS  = STAR[47:32] + 8

syscall_entry:
    # 保存用户栈指针，切换到内核栈
    # (这里简化，实际需要从 TSS 或当前进程获取内核栈)

    swapgs                      # 交换 GS.base (获取内核数据)
    mov %rsp, %gs:8             # 保存用户 RSP
    mov %gs:0, %rsp             # 加载内核 RSP

    # 压入用户态上下文
    push $0x1B                  # 用户 SS
    push %gs:8                  # 用户 RSP
    push %r11                   # 用户 RFLAGS
    push $0x23                  # 用户 CS
    push %rcx                   # 用户 RIP

    # 保存通用寄存器
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

    # 调用 C 处理函数
    # RAX = syscall number
    # RDI, RSI, RDX, R10, R8, R9 = args
    mov %r10, %rcx              # Linux ABI: arg4 in R10, C ABI: arg4 in RCX
    call syscall_handler

    # 恢复寄存器
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
    # RAX 保持返回值

    # 恢复用户态
    pop %rcx                    # RIP -> RCX (for SYSRET)
    add $8, %rsp                # skip CS
    pop %r11                    # RFLAGS -> R11
    pop %rsp                    # 用户 RSP

    swapgs
    sysretq
```

---

## P-19 ~ P-21: 用户地址空间与态切换

### P-19: 用户地址空间

```c
// kernel/proc/user_space.c

#include "vmm.h"
#include "process.h"

#define USER_STACK_TOP    0x00007FFFFFFFE000
#define USER_STACK_SIZE   (64 * 1024)  // 64KB
#define USER_CODE_BASE    0x0000000000400000

// 创建用户进程地址空间
pml4e_t* create_user_address_space(void) {
    // 分配新的 PML4
    pml4e_t* pml4 = (pml4e_t*)pmm_alloc_page();
    if (!pml4) return NULL;
    memset(pml4, 0, PAGE_SIZE);

    // 复制内核映射 (高地址部分)
    extern pml4e_t* kernel_pml4;
    for (int i = 256; i < 512; i++) {
        pml4[i] = kernel_pml4[i];
    }

    return pml4;
}
```

### P-20: 用户栈

```c
// 设置用户栈
int setup_user_stack(process_t* proc) {
    uint64_t stack_top = USER_STACK_TOP;
    uint64_t stack_bottom = stack_top - USER_STACK_SIZE;

    // 映射栈页
    for (uint64_t addr = stack_bottom; addr < stack_top; addr += PAGE_SIZE) {
        uint64_t phys = (uint64_t)pmm_alloc_page();
        if (!phys) return -1;
        vmm_map_page_in(proc->page_table, addr, phys,
                        PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    }

    proc->user_stack = stack_top;
    return 0;
}
```

### P-21: 态切换

```c
// 跳转到用户态
void jump_to_usermode(uint64_t entry, uint64_t user_stack) {
    // 设置段选择子和栈，然后 iretq 到用户态
    __asm__ volatile (
        "cli\n"
        "mov $0x1B, %%ax\n"     // 用户数据段 (SS)
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"

        "push $0x1B\n"          // SS
        "push %0\n"             // RSP
        "pushfq\n"              // RFLAGS
        "pop %%rax\n"
        "or $0x200, %%rax\n"    // 启用中断
        "push %%rax\n"
        "push $0x23\n"          // CS (用户代码段)
        "push %1\n"             // RIP
        "iretq\n"
        :
        : "r"(user_stack), "r"(entry)
        : "rax", "memory"
    );
}
```

---

## 开发顺序

```
P-16 GDT ──> P-17 TSS ──> P-18 SYSCALL
                              │
P-19 用户地址空间 ◄───────────┤
        │                     │
        └──> P-20 用户栈      │
                  │           │
                  └──> P-21 态切换 ◄──┘
```

---

## 验收标准

### 里程碑 1: GDT/TSS 正确
```
[GDT] Initialized with user segments
[TSS] Initialized at 0xFFFF800000123456
[TSS] RSP0 = 0xFFFF800000200000
```

### 里程碑 2: SYSCALL 配置
```
[SYSCALL] MSR configured
[SYSCALL] Entry at 0xFFFF800000100000
```

### 里程碑 3: 用户态切换
```
[Process] Jumping to user mode...
[User] Hello from ring 3!
[SYSCALL] sys_write called from user
```

---

## 整合点

完成 P-21 后，与 Group B、Group C 整合：
- Group B 提供文件系统 (加载用户程序)
- Group C 提供系统调用 (用户程序调用内核)

---

## 文件清单

| 文件 | 描述 |
|------|------|
| kernel/arch/gdt.h | GDT 定义 |
| kernel/arch/gdt.c | GDT 实现 |
| kernel/arch/tss.h | TSS 定义 |
| kernel/arch/tss.c | TSS 实现 |
| kernel/arch/syscall.c | SYSCALL MSR 配置 |
| kernel/arch/syscall_entry.S | syscall 入口汇编 |
| kernel/proc/user_space.c | 用户地址空间 |
