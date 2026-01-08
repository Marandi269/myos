# 阶段 3: 进程管理 (Process Management)

## 目标
实现多任务支持，包括进程创建、调度、上下文切换和基本的进程生命周期管理。

---

## 前置依赖

| 依赖 | 状态 | 说明 |
|------|------|------|
| PMM 物理内存 | ✅ | 页帧分配 |
| Heap 内核堆 | ✅ | kmalloc/kfree |
| VMM 虚拟内存 | ✅ | 页表映射 |
| PIT 定时器 | ✅ | 时间片调度 |
| IDT 中断 | ✅ | 中断上下文保存 |

---

## 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                        调度器 (Scheduler)                    │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐        │
│  │ Process │  │ Process │  │ Process │  │ Process │        │
│  │  PID 0  │  │  PID 1  │  │  PID 2  │  │  PID N  │        │
│  │ (idle)  │  │ (init)  │  │         │  │         │        │
│  └────┬────┘  └────┬────┘  └────┬────┘  └────┬────┘        │
│       │            │            │            │              │
│       ▼            ▼            ▼            ▼              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              进程控制块 (PCB) 链表                    │   │
│  │  - 运行队列 (run_queue)                              │   │
│  │  - 等待队列 (wait_queue)                             │   │
│  │  - 僵尸队列 (zombie_queue)                           │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                     上下文切换 (Context Switch)              │
│  - 保存当前进程寄存器到 PCB                                  │
│  - 切换页表 (CR3)                                           │
│  - 恢复目标进程寄存器                                        │
│  - 切换栈指针 (RSP)                                         │
└─────────────────────────────────────────────────────────────┘
```

---

## 任务清单

### 3.1 核心数据结构

| 任务ID | 任务名称 | 依赖 | 可并行 | 验证方式 |
|--------|----------|------|--------|----------|
| P-01 | 进程控制块 (PCB) | 无 | ✅ | 结构体编译通过 |
| P-02 | 进程状态机 | P-01 | ✅ | 状态转换测试 |
| P-03 | 进程链表/队列 | P-01 | ✅ | 增删查测试 |
| P-04 | PID 分配器 | P-01 | ✅ | 唯一 PID 分配 |

### 3.2 上下文切换

| 任务ID | 任务名称 | 依赖 | 可并行 | 验证方式 |
|--------|----------|------|--------|----------|
| P-05 | CPU 上下文结构 | P-01 | ✅ | 寄存器保存结构 |
| P-06 | switch_to 汇编 | P-05 | ❌ | 两任务来回切换 |
| P-07 | 内核栈管理 | P-01, VMM | ❌ | 每进程独立栈 |

### 3.3 调度器

| 任务ID | 任务名称 | 依赖 | 可并行 | 验证方式 |
|--------|----------|------|--------|----------|
| P-08 | 调度器框架 | P-03 | ✅ | 接口定义 |
| P-09 | Round-Robin 实现 | P-06, P-08 | ❌ | 多任务轮转 |
| P-10 | 定时器触发调度 | P-09, PIT | ❌ | 时间片切换 |
| P-11 | idle 进程 | P-09 | ❌ | CPU 空闲时运行 |

### 3.4 进程生命周期

| 任务ID | 任务名称 | 依赖 | 可并行 | 验证方式 |
|--------|----------|------|--------|----------|
| P-12 | 内核线程创建 | P-09 | ❌ | kthread_create 测试 |
| P-13 | 进程退出 (exit) | P-09 | ❌ | 进程正常终止 |
| P-14 | 进程等待 (wait) | P-13 | ❌ | 父进程回收子进程 |
| P-15 | 僵尸进程处理 | P-13, P-14 | ❌ | 资源正确释放 |

### 3.5 用户态准备 (阶段 4 前置)

**⚠️ 关键依赖**: 用户态进程需要以下基础设施，否则 ring3 代码无法运行。

| 任务ID | 任务名称 | 依赖 | 可并行 | 验证方式 |
|--------|----------|------|--------|----------|
| P-16 | GDT 添加用户段 | boot | ✅ | 用户代码/数据段描述符 |
| P-17 | TSS 设置 | P-16 | ❌ | 中断时内核栈切换正确 |
| P-18 | SYSCALL MSR 配置 | P-17 | ❌ | syscall 指令不崩溃 |
| P-19 | 用户地址空间 | VMM | ✅ | 独立页表创建 |
| P-20 | 用户栈设置 | P-19 | ❌ | 用户栈映射 |
| P-21 | 内核态/用户态切换 | P-17, P-20 | ❌ | ring0 <-> ring3 |

#### P-16: GDT 用户段

```c
// 当前 GDT (boot.S):
// 0x00: null
// 0x08: kernel code (ring 0)
// 0x10: kernel data (ring 0)

// 需要添加:
// 0x18: user code (ring 3)  - 0x00AFFA000000FFFF
// 0x20: user data (ring 3)  - 0x00AFF2000000FFFF
// 0x28: TSS descriptor (16 bytes)
```

#### P-17: TSS (Task State Segment)

```c
// kernel/proc/tss.h

typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;        // 内核栈指针 (ring0)
    uint64_t rsp1;        // ring1 栈 (未使用)
    uint64_t rsp2;        // ring2 栈 (未使用)
    uint64_t reserved1;
    uint64_t ist[7];      // 中断栈表
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset; // IO 权限位图偏移
} __attribute__((packed)) tss_t;

void tss_init(void);
void tss_set_rsp0(uint64_t rsp0);  // 设置当前进程的内核栈
```

**为什么需要 TSS？**
- 用户态 (ring3) 发生中断/异常时，CPU 需要切换到内核栈
- CPU 从 TSS.rsp0 获取内核栈地址
- 没有正确设置 TSS，任何中断都会 triple fault

#### P-18: SYSCALL MSR 配置

```c
// kernel/proc/syscall.c

#define MSR_STAR   0xC0000081  // 段选择子
#define MSR_LSTAR  0xC0000082  // syscall 入口地址
#define MSR_SFMASK 0xC0000084  // RFLAGS 掩码

void syscall_init(void) {
    // STAR: 段选择子
    // bits 32-47: SYSRET CS/SS (user) = 0x18 | 3
    // bits 48-63: SYSCALL CS/SS (kernel) = 0x08
    uint64_t star = ((uint64_t)0x0008 << 32) | ((uint64_t)0x0018 << 48);
    wrmsr(MSR_STAR, star);

    // LSTAR: syscall 处理入口
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    // SFMASK: syscall 时清除的 RFLAGS 位 (IF, DF)
    wrmsr(MSR_SFMASK, 0x200 | 0x400);

    // 启用 SYSCALL/SYSRET (EFER.SCE)
    uint64_t efer = rdmsr(0xC0000080);
    wrmsr(0xC0000080, efer | 1);
}
```

---

## 详细设计

### P-01: 进程控制块 (PCB)

```c
// kernel/proc/process.h

#ifndef _PROCESS_H
#define _PROCESS_H

#include "types.h"
#include "mm/vmm.h"

// 进程状态
typedef enum {
    PROC_UNUSED = 0,    // 未使用的 PCB 槽位
    PROC_CREATED,       // 刚创建
    PROC_READY,         // 就绪，可调度
    PROC_RUNNING,       // 正在运行
    PROC_BLOCKED,       // 阻塞等待
    PROC_ZOMBIE,        // 已退出，等待父进程回收
} proc_state_t;

// CPU 上下文 (用于上下文切换)
typedef struct {
    uint64_t r15, r14, r13, r12;
    uint64_t r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) cpu_context_t;

// 进程控制块
typedef struct process {
    // 基本信息
    uint32_t pid;               // 进程 ID
    uint32_t ppid;              // 父进程 ID
    char name[32];              // 进程名称
    proc_state_t state;         // 进程状态

    // CPU 上下文
    cpu_context_t* context;     // 保存的 CPU 状态
    uint64_t kernel_stack;      // 内核栈顶
    uint64_t kernel_stack_base; // 内核栈底

    // 内存管理
    pml4e_t* page_table;        // 进程页表 (PML4)
    uint64_t brk;               // 堆顶地址

    // 调度信息
    uint32_t priority;          // 优先级
    uint32_t time_slice;        // 剩余时间片
    uint64_t total_time;        // 总运行时间

    // 进程关系
    struct process* parent;     // 父进程
    struct process* children;   // 子进程链表
    struct process* sibling;    // 兄弟进程

    // 链表指针
    struct process* next;       // 队列中下一个
    struct process* prev;       // 队列中上一个

    // 退出状态
    int exit_code;              // 退出码

} process_t;

// 最大进程数
#define MAX_PROCESSES 256

// 默认时间片 (ms)
#define DEFAULT_TIME_SLICE 100

// 内核栈大小 (16KB)
#define KERNEL_STACK_SIZE (16 * 1024)

#endif
```

### P-02: 进程状态机

```
状态转换图:

     ┌──────────────────────────────────────┐
     │                                      │
     ▼                                      │
  UNUSED ──create──> CREATED ──ready──> READY
                                          │ ▲
                                          │ │
                              schedule ───┘ │
                                          │ │
                                          ▼ │
                                       RUNNING
                                          │ │
                         ┌────────────────┘ │
                         │                  │
                   wait/sleep          preempt/yield
                         │                  │
                         ▼                  │
                      BLOCKED ─────wakeup───┘

                      RUNNING ──exit──> ZOMBIE ──wait──> UNUSED
```

### P-05/P-06: 上下文切换

```asm
# kernel/proc/switch.S

.global switch_to
.type switch_to, @function

# void switch_to(process_t* prev, process_t* next)
# rdi = prev, rsi = next

switch_to:
    # 保存 prev 的上下文
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    pushfq

    # 保存 prev 的栈指针
    movq %rsp, 72(%rdi)     # prev->context (offset 需要调整)

    # 切换到 next 的栈
    movq 72(%rsi), %rsp     # next->context

    # 切换页表 (如果不同)
    movq 80(%rsi), %rax     # next->page_table
    movq %cr3, %rbx
    cmpq %rax, %rbx
    je .skip_cr3
    movq %rax, %cr3
.skip_cr3:

    # 恢复 next 的上下文
    popfq
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp

    ret
```

### P-08/P-09: 调度器

```c
// kernel/proc/scheduler.h

#ifndef _SCHEDULER_H
#define _SCHEDULER_H

#include "process.h"

// 调度器接口
void scheduler_init(void);
void schedule(void);          // 选择下一个进程运行
void yield(void);             // 主动让出 CPU
void wakeup(process_t* proc); // 唤醒阻塞进程
void sleep(void* channel);    // 阻塞当前进程

// 进程队列操作
void enqueue_ready(process_t* proc);
void dequeue_ready(process_t* proc);

// 获取当前进程
process_t* current_process(void);

// 定时器回调 (每个时间片调用)
void scheduler_tick(void);

#endif
```

```c
// kernel/proc/scheduler.c (伪代码)

static process_t* run_queue_head;
static process_t* run_queue_tail;
static process_t* current;

void schedule(void) {
    process_t* prev = current;
    process_t* next = NULL;

    // Round-Robin: 取队列头部
    if (run_queue_head) {
        next = run_queue_head;
        dequeue_ready(next);
    } else {
        next = idle_process;
    }

    if (next != prev) {
        current = next;
        next->state = PROC_RUNNING;
        if (prev->state == PROC_RUNNING) {
            prev->state = PROC_READY;
            enqueue_ready(prev);
        }
        switch_to(prev, next);
    }
}

void scheduler_tick(void) {
    if (!current) return;

    current->time_slice--;
    current->total_time++;

    if (current->time_slice <= 0) {
        current->time_slice = DEFAULT_TIME_SLICE;
        schedule();
    }
}
```

### P-12: 内核线程创建

```c
// kernel/proc/kthread.h

typedef void (*kthread_func_t)(void* arg);

// 创建内核线程
process_t* kthread_create(kthread_func_t func, void* arg, const char* name);

// 示例用法
void test_thread_func(void* arg) {
    int id = (int)(uint64_t)arg;
    while (1) {
        kprintf("[Thread %d] running\n", id);
        yield();
    }
}

void test_threads(void) {
    kthread_create(test_thread_func, (void*)1, "thread1");
    kthread_create(test_thread_func, (void*)2, "thread2");
    kthread_create(test_thread_func, (void*)3, "thread3");
}
```

---

## 开发依赖图

```
P-01 PCB ─────┬──> P-02 状态机
              │
              ├──> P-03 进程队列
              │
              ├──> P-04 PID 分配
              │
              └──> P-05 CPU 上下文 ──> P-06 switch_to
                                            │
P-07 内核栈 ◄───────────────────────────────┤
                                            │
P-08 调度器框架 ◄───────────────────────────┤
        │                                   │
        └──> P-09 Round-Robin ◄─────────────┘
                    │
                    ├──> P-10 定时器调度
                    │
                    ├──> P-11 idle 进程
                    │
                    └──> P-12 kthread_create
                              │
                              ├──> P-13 exit
                              │         │
                              │         └──> P-14 wait
                              │                   │
                              │                   └──> P-15 僵尸处理
                              │
                              └──> P-16 GDT 用户段 ──> P-17 TSS
                                                           │
                                                           └──> P-18 SYSCALL MSR
                                                                      │
                                   P-19 用户地址空间 ◄─────────────────┤
                                        │                             │
                                        └──> P-20 用户栈              │
                                                  │                   │
                                                  └──> P-21 态切换 ◄──┘
```

---

## 并行开发分配

### 开发者 A: 核心数据结构

```
P-01 PCB 结构 ──> P-02 状态机 ──> P-03 队列 ──> P-04 PID
```

### 开发者 B: 上下文切换

```
P-05 CPU 上下文 ──> P-06 switch_to 汇编 ──> P-07 内核栈
```

### 开发者 C: 调度器

```
P-08 调度器框架 ──> P-09 Round-Robin ──> P-10 定时器 ──> P-11 idle
```

**整合点**: P-06 + P-09 完成后合并测试

---

## 验收标准

### 里程碑 1: 上下文切换工作

```
[Scheduler] Initialized
[Thread] Created: idle (PID 0)
[Thread] Created: thread1 (PID 1)
[Thread] Created: thread2 (PID 2)

[Thread 1] running
[Thread 2] running
[Thread 1] running
[Thread 2] running
...
```

### 里程碑 2: 时间片调度

```
[PIT] Tick 100
[Scheduler] Preempt PID 1 -> PID 2
[PIT] Tick 200
[Scheduler] Preempt PID 2 -> PID 1
...
```

### 里程碑 3: 进程生命周期

```
[Process] PID 1 created by PID 0
[Process] PID 1 exited with code 0
[Process] PID 0 reaped zombie PID 1
```

---

## 测试用例

```c
void test_scheduler(void) {
    kprintf("\n[TEST] Scheduler\n");

    // 测试 1: 创建内核线程
    process_t* t1 = kthread_create(test_func, (void*)1, "test1");
    process_t* t2 = kthread_create(test_func, (void*)2, "test2");
    kprintf("  Created threads: PID %d, %d\n", t1->pid, t2->pid);

    // 测试 2: 验证调度
    for (int i = 0; i < 10; i++) {
        yield();
    }
    kprintf("  Context switch: PASSED\n");

    // 测试 3: 进程退出
    // ... (在 test_func 中调用 exit)

    kprintf("[TEST] Scheduler: ALL PASSED\n");
}
```

---

## 目录结构

```
kernel/
├── proc/                    # 新增
│   ├── process.h           # PCB 定义
│   ├── process.c           # 进程管理
│   ├── scheduler.h         # 调度器接口
│   ├── scheduler.c         # 调度器实现
│   ├── switch.S            # 上下文切换汇编
│   ├── kthread.c           # 内核线程
│   └── pid.c               # PID 分配器
```

---

## 风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| 上下文切换崩溃 | 先用 kprintf 调试，逐步验证寄存器 |
| 栈溢出 | 添加栈保护页 (guard page) |
| 死锁 | 先实现单核，避免复杂锁 |
| 内存泄漏 | 退出时检查所有资源释放 |

---

## 下一阶段预告

**阶段 4: 系统调用** (依赖阶段 3 完成)
- syscall/sysret 指令
- 系统调用表
- fork/exec/exit/wait 系统调用
- 用户态程序执行
