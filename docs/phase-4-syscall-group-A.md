# 阶段 4A: 用户态基础设施

**开发组**: A
**负责人**: 待分配
**状态**: ⏳ 待开始

---

## 目标

完成用户态运行的基础设施，使 ring3 代码能够正常执行并通过中断返回内核。

---

## 前置依赖

| 依赖 | 状态 | 来源 |
|------|------|------|
| 调度器 | ✅ | 阶段 3 |
| VMM | ✅ | 阶段 2.5 |
| IDT | ✅ | 阶段 1 |

---

## 任务清单

| 任务ID | 任务名称 | 依赖 | 验证方式 |
|--------|----------|------|----------|
| A-01 | GDT 添加用户段 | 无 | 段选择子正确 |
| A-02 | TSS 初始化 | A-01 | ltr 指令成功 |
| A-03 | TSS 内核栈切换 | A-02 | 用户态中断不崩溃 |
| A-04 | SYSCALL MSR 配置 | A-02 | syscall 指令陷入内核 |
| A-05 | 用户地址空间创建 | VMM | 独立页表 |
| A-06 | 用户栈映射 | A-05 | 用户栈可访问 |
| A-07 | ring0 -> ring3 切换 | A-03, A-06 | 跳转到用户代码 |
| A-08 | ring3 -> ring0 返回 | A-04, A-07 | syscall 返回正常 |

---

## 详细设计

### A-01: GDT 添加用户段

修改 `kernel/boot.S` 或创建 `kernel/proc/gdt.c`:

```c
// GDT 布局 (64-bit long mode)
// Index  Offset  Description
// 0      0x00    Null descriptor
// 1      0x08    Kernel Code (ring 0)
// 2      0x10    Kernel Data (ring 0)
// 3      0x18    User Code (ring 3)
// 4      0x20    User Data (ring 3)
// 5      0x28    TSS (16 bytes, spans 0x28-0x37)

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20
#define GDT_TSS         0x28

// 用户段描述符
// Code: 0x00AFFA000000FFFF (DPL=3, executable, readable)
// Data: 0x00AFF2000000FFFF (DPL=3, writable)

static uint64_t gdt[] = {
    0x0000000000000000,  // Null
    0x00AF9A000000FFFF,  // Kernel Code
    0x00AF92000000FFFF,  // Kernel Data
    0x00AFFA000000FFFF,  // User Code (DPL=3)
    0x00AFF2000000FFFF,  // User Data (DPL=3)
    0x0000000000000000,  // TSS low (填充)
    0x0000000000000000,  // TSS high
};
```

**验证**:
```c
void test_gdt(void) {
    // 检查当前 CS 是内核代码段
    uint16_t cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    kprintf("CS = 0x%x (expected 0x08)\n", cs);
}
```

---

### A-02: TSS 初始化

创建 `kernel/proc/tss.c`:

```c
#include "tss.h"
#include "lib/string.h"

static tss_t tss __attribute__((aligned(16)));

void tss_init(void) {
    memset(&tss, 0, sizeof(tss));

    // 设置 IO 权限位图偏移 (禁用)
    tss.iopb_offset = sizeof(tss);

    // 在 GDT 中设置 TSS 描述符
    uint64_t tss_addr = (uint64_t)&tss;
    uint64_t tss_limit = sizeof(tss) - 1;

    // TSS 描述符格式 (16 bytes in 64-bit mode)
    // 参考: Intel SDM Vol 3, Section 7.2.3
    extern uint64_t gdt[];

    // Low 8 bytes
    gdt[5] = (tss_limit & 0xFFFF) |
             ((tss_addr & 0xFFFF) << 16) |
             (((tss_addr >> 16) & 0xFF) << 32) |
             ((uint64_t)0x89 << 40) |  // Type: 64-bit TSS (available)
             (((tss_limit >> 16) & 0xF) << 48) |
             (((tss_addr >> 24) & 0xFF) << 56);

    // High 8 bytes
    gdt[6] = (tss_addr >> 32) & 0xFFFFFFFF;

    // 加载 TSS
    __asm__ volatile("ltr %0" : : "r"((uint16_t)GDT_TSS));

    kprintf("[TSS] Initialized at 0x%lx\n", tss_addr);
}

void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}
```

**验证**:
```c
void test_tss(void) {
    tss_init();
    // 如果 ltr 没有 #GP，则成功
    kprintf("[TSS] Load successful\n");
}
```

---

### A-03: TSS 内核栈切换

在上下文切换时更新 TSS.rsp0:

```c
// kernel/proc/scheduler.c

void switch_to_process(process_t* next) {
    // 设置 TSS.rsp0 为下一个进程的内核栈顶
    tss_set_rsp0(next->kernel_stack);

    // 执行上下文切换
    // ...
}
```

**验证**:
```c
// 创建一个简单的用户态测试
// 在用户态触发中断 (如 int 0x80)
// 检查是否正确切换到内核栈
```

---

### A-04: SYSCALL MSR 配置

创建 `kernel/proc/syscall_init.c`:

```c
#define MSR_EFER   0xC0000080
#define MSR_STAR   0xC0000081
#define MSR_LSTAR  0xC0000082
#define MSR_SFMASK 0xC0000084

#define EFER_SCE   (1 << 0)  // SYSCALL Enable

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

extern void syscall_entry(void);  // 在汇编中定义

void syscall_init(void) {
    // STAR: 段选择子
    // Bits 32-47: SYSRET CS and SS (+8 for SS, +16 for 64-bit CS)
    // Bits 48-63: SYSCALL CS and SS
    uint64_t star = ((uint64_t)(GDT_USER_CODE - 16) << 48) |  // SYSRET base
                    ((uint64_t)GDT_KERNEL_CODE << 32);         // SYSCALL base
    wrmsr(MSR_STAR, star);

    // LSTAR: syscall 入口点
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    // SFMASK: syscall 时清除的 RFLAGS 位
    // 清除 IF (中断) 和 DF (方向)
    wrmsr(MSR_SFMASK, 0x200 | 0x400);

    // 启用 SYSCALL/SYSRET
    uint64_t efer = rdmsr(MSR_EFER);
    wrmsr(MSR_EFER, efer | EFER_SCE);

    kprintf("[SYSCALL] Initialized\n");
}
```

汇编入口 `kernel/proc/syscall_entry.S`:

```asm
.global syscall_entry
.type syscall_entry, @function

syscall_entry:
    # syscall 时:
    # - RCX = 用户 RIP
    # - R11 = 用户 RFLAGS
    # - 其他寄存器保持用户态值

    # 切换到内核栈 (从 TSS.rsp0 或 per-CPU 变量获取)
    # 简化版: 使用当前进程的内核栈
    swapgs                      # 交换 GS base (指向 per-CPU 数据)
    mov %rsp, %gs:8             # 保存用户 RSP
    mov %gs:0, %rsp             # 加载内核 RSP

    # 保存用户态寄存器
    push %rcx                   # 用户 RIP
    push %r11                   # 用户 RFLAGS
    push %gs:8                  # 用户 RSP

    # 保存其他寄存器
    push %rax
    push %rbx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r12
    push %r13
    push %r14
    push %r15

    # 调用 C 处理函数
    # rdi = syscall number (已在 rax 中)
    mov %rax, %rdi
    call syscall_handler

    # 恢复寄存器
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rbx
    # rax 保持返回值

    pop %gs:8                   # 恢复用户 RSP (暂存)
    pop %r11                    # 用户 RFLAGS
    pop %rcx                    # 用户 RIP

    mov %gs:8, %rsp             # 切换回用户栈
    swapgs                      # 恢复 GS

    sysretq
```

---

### A-05 ~ A-08: 用户空间和态切换

```c
// kernel/proc/user.c

// 创建用户地址空间
pml4e_t* create_user_address_space(void) {
    pml4e_t* pml4 = pmm_alloc_page();
    memset(pml4, 0, PAGE_SIZE);

    // 复制内核映射 (高地址部分)
    extern pml4e_t* kernel_pml4;
    for (int i = 256; i < 512; i++) {
        pml4[i] = kernel_pml4[i];
    }

    return pml4;
}

// 映射用户栈
void setup_user_stack(pml4e_t* pml4, uint64_t* user_sp) {
    // 用户栈位于 0x7FFFFFFFF000 (用户空间顶部)
    uint64_t stack_top = 0x7FFFFFFFF000;
    uint64_t stack_pages = 16;  // 64KB 栈

    for (int i = 0; i < stack_pages; i++) {
        uint64_t vaddr = stack_top - (i + 1) * PAGE_SIZE;
        uint64_t paddr = (uint64_t)pmm_alloc_page();
        vmm_map_page_in(pml4, vaddr, paddr,
                        PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    }

    *user_sp = stack_top;
}

// 跳转到用户态
void jump_to_usermode(uint64_t entry, uint64_t user_sp) {
    __asm__ volatile(
        "mov %0, %%rcx\n"       // RCX = 用户入口
        "mov %1, %%rsp\n"       // RSP = 用户栈
        "mov $0x202, %%r11\n"   // R11 = RFLAGS (IF=1)
        "swapgs\n"
        "sysretq\n"
        :
        : "r"(entry), "r"(user_sp)
        : "rcx", "r11"
    );
}
```

---

## 验收标准

```
[GDT] User segments added (0x18, 0x20)
[TSS] Initialized at 0xXXXXXX
[TSS] Load successful
[SYSCALL] Initialized

[TEST] User mode
  Creating user address space... OK
  Setting up user stack at 0x7FFFFFFFF000... OK
  Jumping to user mode...
  [USER] Hello from ring 3!
  [USER] Calling syscall...
  [KERNEL] Syscall #0 from user
  [USER] Syscall returned!
[TEST] User mode: PASSED
```

---

## 交付文件

| 文件 | 描述 |
|------|------|
| kernel/proc/gdt.c | GDT 管理 |
| kernel/proc/gdt.h | GDT 头文件 |
| kernel/proc/tss.c | TSS 管理 |
| kernel/proc/tss.h | TSS 头文件 |
| kernel/proc/syscall_init.c | SYSCALL MSR 配置 |
| kernel/proc/syscall_entry.S | syscall 入口汇编 |
| kernel/proc/user.c | 用户空间管理 |

---

## 与其他组的接口

| 接口 | 提供给 | 说明 |
|------|--------|------|
| `syscall_handler()` | 组 B | syscall 分发入口 |
| `tss_set_rsp0()` | 调度器 | 上下文切换时更新 |
| `jump_to_usermode()` | 组 C | exec 实现使用 |
| `create_user_address_space()` | 组 C | fork/exec 使用 |

---

## 风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| TSS 设置错误导致 triple fault | 先用 QEMU -d int 调试 |
| SYSCALL 段选择子错误 | 仔细检查 STAR MSR 计算 |
| 用户栈地址冲突 | 使用规范的用户空间布局 |
