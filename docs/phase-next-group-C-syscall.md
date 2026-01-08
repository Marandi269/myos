# Group C: 系统调用 (System Calls)

## 概述

| 项目 | 说明 |
|------|------|
| 负责人 | 开发者 C |
| 任务范围 | S-01 ~ S-12 |
| 前置依赖 | IDT ✅, 进程管理 ✅ |
| 并行组 | Group A (用户态准备), Group B (文件系统) |

## 目标

实现系统调用框架和基础系统调用：
1. syscall 入口和分发
2. 基础 I/O: read, write
3. 进程控制: fork, exec, exit, wait
4. 内存管理: brk, mmap

---

## 任务清单

### 框架

| 任务ID | 任务名称 | 依赖 | 验证方式 |
|--------|----------|------|----------|
| S-01 | syscall 入口 | IDT | 陷入内核成功 |
| S-02 | 系统调用分发表 | S-01 | 调用号路由正确 |

### 基础系统调用

| 任务ID | 任务名称 | 依赖 | 验证方式 |
|--------|----------|------|----------|
| S-03 | sys_write | S-02, fd表 | 用户态打印 |
| S-04 | sys_read | S-02, fd表 | 用户态输入 |
| S-07 | sys_exit | S-02 | 进程退出 |
| S-11 | sys_brk | S-02 | 堆扩展 |
| S-12 | sys_getpid | S-02 | 获取 PID |

### 进程系统调用 (依赖 Group A 完成)

| 任务ID | 任务名称 | 依赖 | 验证方式 |
|--------|----------|------|----------|
| S-05 | sys_fork | P-04 | fork 返回不同值 |
| S-06 | sys_exec | U-01 | 加载执行新程序 |
| S-08 | sys_wait | P-06 | 等待子进程 |

---

## S-01: syscall 入口

### 系统调用号定义

```c
// kernel/syscall/syscall.h

#ifndef _SYSCALL_H
#define _SYSCALL_H

// 系统调用号 (Linux 兼容子集)
#define SYS_READ      0
#define SYS_WRITE     1
#define SYS_OPEN      2
#define SYS_CLOSE     3
#define SYS_STAT      4
#define SYS_FSTAT     5
#define SYS_LSEEK     8
#define SYS_MMAP      9
#define SYS_MPROTECT  10
#define SYS_MUNMAP    11
#define SYS_BRK       12
#define SYS_IOCTL     16
#define SYS_PIPE      22
#define SYS_DUP       32
#define SYS_DUP2      33
#define SYS_GETPID    39
#define SYS_FORK      57
#define SYS_EXECVE    59
#define SYS_EXIT      60
#define SYS_WAIT4     61
#define SYS_KILL      62
#define SYS_GETCWD    79
#define SYS_CHDIR     80

#define MAX_SYSCALL   256

// 系统调用处理函数类型
typedef int64_t (*syscall_fn_t)(uint64_t, uint64_t, uint64_t,
                                 uint64_t, uint64_t, uint64_t);

void syscall_init(void);
int64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2,
                        uint64_t a3, uint64_t a4, uint64_t a5);

#endif
```

### syscall 入口汇编

```asm
# kernel/syscall/syscall_entry.S

.global syscall_entry
.type syscall_entry, @function

# 进入时:
# RAX = syscall number
# RDI = arg1, RSI = arg2, RDX = arg3
# R10 = arg4, R8 = arg5, R9 = arg6
# RCX = user RIP (saved by CPU)
# R11 = user RFLAGS (saved by CPU)

syscall_entry:
    # 切换到内核栈
    swapgs
    mov %rsp, %gs:16            # 保存用户 RSP 到 per-CPU
    mov %gs:8, %rsp             # 加载内核 RSP

    # 保存用户态寄存器
    push %rcx                   # RIP
    push %r11                   # RFLAGS
    push %rax                   # syscall number

    # 保存被调用者保存的寄存器
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15

    # 保存参数寄存器
    push %rdi
    push %rsi
    push %rdx
    push %r10
    push %r8
    push %r9

    # 调用 C 处理函数
    # 参数: syscall_handler(num, a1, a2, a3, a4, a5)
    mov %rax, %rdi              # arg0 = syscall number
    mov 40(%rsp), %rsi          # arg1 = RDI (原始)
    mov 32(%rsp), %rdx          # arg2 = RSI
    mov 24(%rsp), %rcx          # arg3 = RDX
    mov 16(%rsp), %r8           # arg4 = R10
    mov 8(%rsp), %r9            # arg5 = R8

    call syscall_handler

    # RAX 现在包含返回值

    # 恢复寄存器
    add $48, %rsp               # 跳过参数寄存器
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx

    pop %rdi                    # 丢弃保存的 syscall number
    pop %r11                    # RFLAGS
    pop %rcx                    # RIP

    # 恢复用户栈
    mov %gs:16, %rsp
    swapgs

    sysretq
```

---

## S-02: 系统调用分发表

```c
// kernel/syscall/syscall.c

#include "syscall.h"
#include "kprintf.h"

// 系统调用表
static syscall_fn_t syscall_table[MAX_SYSCALL];

// 未实现的系统调用
static int64_t sys_unimplemented(uint64_t a1, uint64_t a2, uint64_t a3,
                                  uint64_t a4, uint64_t a5, uint64_t a6) {
    kprintf("[SYSCALL] Unimplemented syscall\n");
    return -1;  // -ENOSYS
}

// 初始化
void syscall_init(void) {
    // 填充未实现
    for (int i = 0; i < MAX_SYSCALL; i++) {
        syscall_table[i] = sys_unimplemented;
    }

    // 注册已实现的系统调用
    syscall_table[SYS_READ]   = (syscall_fn_t)sys_read;
    syscall_table[SYS_WRITE]  = (syscall_fn_t)sys_write;
    syscall_table[SYS_EXIT]   = (syscall_fn_t)sys_exit;
    syscall_table[SYS_BRK]    = (syscall_fn_t)sys_brk;
    syscall_table[SYS_GETPID] = (syscall_fn_t)sys_getpid;
    // ... 更多

    kprintf("[SYSCALL] Initialized (%d handlers)\n", MAX_SYSCALL);
}

// 分发处理
int64_t syscall_handler(uint64_t num, uint64_t a1, uint64_t a2,
                        uint64_t a3, uint64_t a4, uint64_t a5) {
    if (num >= MAX_SYSCALL) {
        kprintf("[SYSCALL] Invalid syscall: %d\n", num);
        return -1;
    }

    return syscall_table[num](a1, a2, a3, a4, a5, 0);
}
```

---

## S-03: sys_write

```c
// kernel/syscall/sys_io.c

#include "syscall.h"
#include "fd.h"
#include "vfs.h"
#include "process.h"

// ssize_t write(int fd, const void* buf, size_t count)
int64_t sys_write(int fd, const char* buf, size_t count) {
    process_t* proc = current_process();
    if (!proc || !proc->fd_table) {
        return -1;  // -EBADF
    }

    struct file* file = fd_get(proc->fd_table, fd);
    if (!file) {
        return -1;  // -EBADF
    }

    // 验证用户缓冲区 (简化: 假设有效)
    // TODO: 检查 buf 在用户地址空间

    return vfs_write(file, buf, count);
}
```

---

## S-04: sys_read

```c
// ssize_t read(int fd, void* buf, size_t count)
int64_t sys_read(int fd, char* buf, size_t count) {
    process_t* proc = current_process();
    if (!proc || !proc->fd_table) {
        return -1;
    }

    struct file* file = fd_get(proc->fd_table, fd);
    if (!file) {
        return -1;
    }

    return vfs_read(file, buf, count);
}
```

---

## S-07: sys_exit

```c
// kernel/syscall/sys_process.c

#include "process.h"
#include "scheduler.h"

// void exit(int status)
int64_t sys_exit(int status) {
    process_t* proc = current_process();
    if (!proc) {
        return -1;
    }

    process_exit(proc, status);

    // 不应该返回
    schedule();
    return 0;
}
```

---

## S-11: sys_brk

```c
// kernel/syscall/sys_memory.c

#include "process.h"
#include "vmm.h"
#include "pmm.h"

// void* brk(void* addr)
int64_t sys_brk(uint64_t addr) {
    process_t* proc = current_process();
    if (!proc) {
        return -1;
    }

    // 获取当前 brk
    if (addr == 0) {
        return proc->brk;
    }

    // 扩展堆
    uint64_t old_brk = proc->brk;
    uint64_t new_brk = (addr + 0xFFF) & ~0xFFF;  // 页对齐

    if (new_brk > old_brk) {
        // 分配新页
        for (uint64_t page = old_brk; page < new_brk; page += PAGE_SIZE) {
            uint64_t phys = (uint64_t)pmm_alloc_page();
            if (!phys) {
                return proc->brk;  // 失败，返回原 brk
            }
            vmm_map_page_in(proc->page_table, page, phys,
                           PTE_PRESENT | PTE_WRITABLE | PTE_USER);
        }
    }
    // TODO: 收缩堆

    proc->brk = new_brk;
    return new_brk;
}
```

---

## S-12: sys_getpid

```c
// pid_t getpid(void)
int64_t sys_getpid(void) {
    process_t* proc = current_process();
    if (!proc) {
        return -1;
    }
    return proc->pid;
}
```

---

## S-05: sys_fork (依赖 Group A)

```c
// kernel/syscall/sys_process.c

// pid_t fork(void)
int64_t sys_fork(void) {
    process_t* parent = current_process();
    if (!parent) {
        return -1;
    }

    // 创建子进程
    process_t* child = process_create(parent->name);
    if (!child) {
        return -1;  // -ENOMEM
    }

    // 复制地址空间
    child->page_table = vmm_clone_page_table(parent->page_table);

    // 复制文件描述符表
    child->fd_table = fd_table_clone(parent->fd_table);

    // 复制寄存器状态
    memcpy(child->context, parent->context, sizeof(cpu_context_t));

    // 设置返回值
    child->context->rax = 0;   // 子进程返回 0

    // 设置父子关系
    child->parent = parent;
    child->ppid = parent->pid;

    // 加入调度队列
    enqueue_ready(child);

    return child->pid;  // 父进程返回子进程 PID
}
```

---

## S-06: sys_exec

```c
// int execve(const char* path, char* const argv[], char* const envp[])
int64_t sys_exec(const char* path, char** argv, char** envp) {
    process_t* proc = current_process();
    
    // 打开可执行文件
    struct file* file = vfs_open(path, O_RDONLY);
    if (!file) {
        return -1;  // -ENOENT
    }

    // 加载 ELF
    uint64_t entry = elf_load(proc, file);
    vfs_close(file);

    if (entry == 0) {
        return -1;  // -ENOEXEC
    }

    // 设置用户栈和参数
    uint64_t user_stack = setup_user_stack(proc, argv, envp);

    // 跳转到用户态
    jump_to_usermode(entry, user_stack);

    // 不应该返回
    return 0;
}
```

---

## 开发顺序

```
S-01 syscall 入口 ──> S-02 分发表
                          │
          ┌───────────────┼───────────────┐
          │               │               │
          ▼               ▼               ▼
      S-03 write      S-07 exit       S-12 getpid
      S-04 read       S-11 brk
          │
          │ (需要 Group B fd表)
          │
          │ (需要 Group A 用户态)
          ▼
      S-05 fork ──> S-06 exec ──> S-08 wait
```

---

## 验收标准

### 里程碑 1: 框架工作
```
[SYSCALL] Initialized (256 handlers)
[TEST] syscall 0x3F -> sys_unimplemented
```

### 里程碑 2: 基础调用
```
[TEST] sys_getpid() = 1
[TEST] sys_write(1, "Hello", 5) = 5  -> 串口输出 "Hello"
[TEST] sys_brk(0) = 0x400000
```

### 里程碑 3: 进程调用
```
[TEST] fork() parent returns 2, child returns 0
[TEST] exec("/bin/hello") -> "Hello World"
[TEST] exit(42) -> parent wait() returns 42
```

---

## 整合点

需要与其他组整合：
- **Group A**: P-18 SYSCALL MSR 配置后才能使用 syscall 指令
- **Group A**: P-21 态切换完成后 sys_fork/sys_exec 才能工作
- **Group B**: FS-02 fd 表完成后 sys_read/sys_write 才能工作

---

## 用户态调用示例

```c
// userspace/lib/syscall.h

static inline int64_t syscall0(uint64_t num) {
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline int64_t syscall3(uint64_t num, uint64_t a1, uint64_t a2, uint64_t a3) {
    int64_t ret;
    __asm__ volatile (
        "syscall"
        : "=a"(ret)
        : "a"(num), "D"(a1), "S"(a2), "d"(a3)
        : "rcx", "r11", "memory"
    );
    return ret;
}

#define write(fd, buf, count) syscall3(SYS_WRITE, fd, (uint64_t)buf, count)
#define read(fd, buf, count)  syscall3(SYS_READ, fd, (uint64_t)buf, count)
#define exit(status)          syscall1(SYS_EXIT, status)
#define getpid()              syscall0(SYS_GETPID)
```

---

## 文件清单

| 文件 | 描述 |
|------|------|
| kernel/syscall/syscall.h | 系统调用号定义 |
| kernel/syscall/syscall.c | 分发表和初始化 |
| kernel/syscall/syscall_entry.S | syscall 入口汇编 |
| kernel/syscall/sys_io.c | read/write |
| kernel/syscall/sys_process.c | fork/exec/exit/wait |
| kernel/syscall/sys_memory.c | brk/mmap |
