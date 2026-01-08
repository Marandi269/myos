# 阶段 4C: 系统调用实现

**开发组**: C
**负责人**: 待分配
**状态**: ⏳ 待开始

---

## 目标

实现系统调用分发机制和核心系统调用，使用户程序能够请求内核服务。

---

## 前置依赖

| 依赖 | 来源组 | 说明 |
|------|--------|------|
| syscall_entry | 组 A | SYSCALL 入口点 |
| VFS | 组 B | 文件操作 |
| fd_table | 组 B | 文件描述符 |
| 进程管理 | 阶段 3 | fork/exit/wait |

---

## 任务清单

| 任务ID | 任务名称 | 依赖 | 验证方式 |
|--------|----------|------|----------|
| C-01 | syscall 分发表 | A-04 | 调用号路由正确 |
| C-02 | sys_write | C-01, B-04 | 用户态打印 |
| C-03 | sys_read | C-01, B-04 | 用户态输入 |
| C-04 | sys_open | C-01, B-03 | 打开文件返回 fd |
| C-05 | sys_close | C-01, B-04 | 关闭文件 |
| C-06 | sys_exit | C-01 | 进程退出 |
| C-07 | sys_getpid | C-01 | 返回 PID |
| C-08 | sys_fork | C-01 | 复制进程 |
| C-09 | sys_exec | C-01, B-03 | 加载执行程序 |
| C-10 | sys_wait | C-01 | 等待子进程 |
| C-11 | sys_brk | C-01 | 堆扩展 |
| C-12 | sys_mmap | C-01 | 内存映射 (简化版) |

---

## 系统调用号定义

创建 `include/syscall_nr.h`:

```c
#ifndef _SYSCALL_NR_H
#define _SYSCALL_NR_H

#define SYS_READ        0
#define SYS_WRITE       1
#define SYS_OPEN        2
#define SYS_CLOSE       3
#define SYS_STAT        4
#define SYS_FSTAT       5
#define SYS_LSEEK       8
#define SYS_MMAP        9
#define SYS_MPROTECT    10
#define SYS_MUNMAP      11
#define SYS_BRK         12
#define SYS_IOCTL       16
#define SYS_PIPE        22
#define SYS_DUP         32
#define SYS_DUP2        33
#define SYS_GETPID      39
#define SYS_FORK        57
#define SYS_EXECVE      59
#define SYS_EXIT        60
#define SYS_WAIT4       61
#define SYS_GETCWD      79
#define SYS_CHDIR       80
#define SYS_MKDIR       83
#define SYS_RMDIR       84
#define SYS_UNLINK      87

#define SYS_MAX         256

#endif
```

---

## 详细设计

### C-01: syscall 分发表

创建 `kernel/syscall/syscall.c`:

```c
#include "syscall.h"
#include "syscall_nr.h"
#include "lib/kprintf.h"
#include "proc/process.h"

// 系统调用函数类型
typedef int64_t (*syscall_fn_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

// 系统调用表
static syscall_fn_t syscall_table[SYS_MAX];

// 注册系统调用
void syscall_register(int nr, syscall_fn_t fn) {
    if (nr >= 0 && nr < SYS_MAX) {
        syscall_table[nr] = fn;
    }
}

// 系统调用处理函数 (从汇编调用)
int64_t syscall_handler(uint64_t nr,
                        uint64_t arg1, uint64_t arg2, uint64_t arg3,
                        uint64_t arg4, uint64_t arg5, uint64_t arg6) {

    if (nr >= SYS_MAX || !syscall_table[nr]) {
        kprintf("[SYSCALL] Unknown syscall %d\n", nr);
        return -1;  // ENOSYS
    }

    return syscall_table[nr](arg1, arg2, arg3, arg4, arg5, arg6);
}

// 初始化
void syscall_init(void) {
    // 清空系统调用表
    for (int i = 0; i < SYS_MAX; i++) {
        syscall_table[i] = NULL;
    }

    // 注册系统调用
    syscall_register(SYS_READ,    sys_read);
    syscall_register(SYS_WRITE,   sys_write);
    syscall_register(SYS_OPEN,    sys_open);
    syscall_register(SYS_CLOSE,   sys_close);
    syscall_register(SYS_BRK,     sys_brk);
    syscall_register(SYS_GETPID,  sys_getpid);
    syscall_register(SYS_FORK,    sys_fork);
    syscall_register(SYS_EXECVE,  sys_exec);
    syscall_register(SYS_EXIT,    sys_exit);
    syscall_register(SYS_WAIT4,   sys_wait);

    kprintf("[SYSCALL] Table initialized (%d entries)\n", SYS_MAX);
}
```

---

### C-02: sys_write

```c
// kernel/syscall/sys_io.c

#include "syscall.h"
#include "fs/fd.h"
#include "fs/vfs.h"
#include "proc/process.h"

// ssize_t write(int fd, const void* buf, size_t count)
int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t count,
                  uint64_t unused1, uint64_t unused2, uint64_t unused3) {
    (void)unused1; (void)unused2; (void)unused3;

    process_t* proc = current_process();
    file_t* file = fd_get(proc->fd_table, fd);

    if (!file) {
        return -9;  // EBADF
    }

    // 验证用户指针 (简化版)
    if (buf == 0) {
        return -14;  // EFAULT
    }

    ssize_t ret = vfs_write(file->node, (void*)buf, count, file->offset);
    if (ret > 0) {
        file->offset += ret;
    }

    return ret;
}
```

---

### C-03: sys_read

```c
// ssize_t read(int fd, void* buf, size_t count)
int64_t sys_read(uint64_t fd, uint64_t buf, uint64_t count,
                 uint64_t unused1, uint64_t unused2, uint64_t unused3) {
    (void)unused1; (void)unused2; (void)unused3;

    process_t* proc = current_process();
    file_t* file = fd_get(proc->fd_table, fd);

    if (!file) {
        return -9;  // EBADF
    }

    if (buf == 0) {
        return -14;  // EFAULT
    }

    ssize_t ret = vfs_read(file->node, (void*)buf, count, file->offset);
    if (ret > 0) {
        file->offset += ret;
    }

    return ret;
}
```

---

### C-04: sys_open

```c
// int open(const char* pathname, int flags)
int64_t sys_open(uint64_t pathname, uint64_t flags, uint64_t mode,
                 uint64_t unused1, uint64_t unused2, uint64_t unused3) {
    (void)mode; (void)unused1; (void)unused2; (void)unused3;

    if (pathname == 0) {
        return -14;  // EFAULT
    }

    const char* path = (const char*)pathname;

    // 查找文件
    vfs_node_t* node = vfs_lookup(path);
    if (!node) {
        // TODO: 如果 O_CREAT 则创建
        return -2;  // ENOENT
    }

    // 打开文件
    file_t* file = file_open(node, flags);
    if (!file) {
        return -12;  // ENOMEM
    }

    // 分配 fd
    process_t* proc = current_process();
    int fd = fd_alloc(proc->fd_table, file);
    if (fd < 0) {
        file_close(file);
        return -24;  // EMFILE
    }

    return fd;
}
```

---

### C-05: sys_close

```c
// int close(int fd)
int64_t sys_close(uint64_t fd, uint64_t unused1, uint64_t unused2,
                  uint64_t unused3, uint64_t unused4, uint64_t unused5) {
    (void)unused1; (void)unused2; (void)unused3; (void)unused4; (void)unused5;

    process_t* proc = current_process();
    return fd_free(proc->fd_table, fd);
}
```

---

### C-06: sys_exit

```c
// kernel/syscall/sys_proc.c

#include "syscall.h"
#include "proc/process.h"
#include "proc/scheduler.h"

// void exit(int status)
int64_t sys_exit(uint64_t status, uint64_t unused1, uint64_t unused2,
                 uint64_t unused3, uint64_t unused4, uint64_t unused5) {
    (void)unused1; (void)unused2; (void)unused3; (void)unused4; (void)unused5;

    process_t* proc = current_process();
    process_exit(proc, (int)status);

    // 不应该返回
    schedule();
    return 0;
}
```

---

### C-07: sys_getpid

```c
// pid_t getpid(void)
int64_t sys_getpid(uint64_t unused1, uint64_t unused2, uint64_t unused3,
                   uint64_t unused4, uint64_t unused5, uint64_t unused6) {
    (void)unused1; (void)unused2; (void)unused3;
    (void)unused4; (void)unused5; (void)unused6;

    process_t* proc = current_process();
    return proc->pid;
}
```

---

### C-08: sys_fork

```c
// pid_t fork(void)
int64_t sys_fork(uint64_t unused1, uint64_t unused2, uint64_t unused3,
                 uint64_t unused4, uint64_t unused5, uint64_t unused6) {
    (void)unused1; (void)unused2; (void)unused3;
    (void)unused4; (void)unused5; (void)unused6;

    process_t* parent = current_process();

    // 创建子进程
    process_t* child = process_create(parent->name);
    if (!child) {
        return -12;  // ENOMEM
    }

    // 设置父子关系
    child->ppid = parent->pid;
    child->parent = parent;

    // 复制地址空间
    child->page_table = vmm_clone_page_table(parent->page_table);

    // 复制文件描述符表
    child->fd_table = fd_table_clone(parent->fd_table);

    // 复制 CPU 上下文
    // 子进程从 fork 返回 0
    memcpy(child->context, parent->context, sizeof(cpu_context_t));
    child->context->rax = 0;  // 子进程返回值

    // 加入就绪队列
    scheduler_add(child);

    // 父进程返回子进程 PID
    return child->pid;
}
```

---

### C-09: sys_exec

```c
// int execve(const char* pathname, char* const argv[], char* const envp[])
int64_t sys_exec(uint64_t pathname, uint64_t argv, uint64_t envp,
                 uint64_t unused1, uint64_t unused2, uint64_t unused3) {
    (void)argv; (void)envp;  // TODO: 参数传递
    (void)unused1; (void)unused2; (void)unused3;

    if (pathname == 0) {
        return -14;  // EFAULT
    }

    const char* path = (const char*)pathname;

    // 打开可执行文件
    vfs_node_t* node = vfs_lookup(path);
    if (!node) {
        return -2;  // ENOENT
    }

    // 加载 ELF (简化版)
    uint64_t entry_point = elf_load(node);
    if (entry_point == 0) {
        return -8;  // ENOEXEC
    }

    process_t* proc = current_process();

    // 重置地址空间
    vmm_destroy_user_pages(proc->page_table);

    // 设置新的入口点
    // 重置栈
    uint64_t user_sp = 0x7FFFFFFFF000;
    setup_user_stack(proc->page_table, &user_sp);

    // 跳转到新程序 (不返回)
    jump_to_usermode(entry_point, user_sp);

    return 0;  // 不会到达
}
```

---

### C-10: sys_wait

```c
// pid_t wait4(pid_t pid, int* status, int options, struct rusage* rusage)
int64_t sys_wait(uint64_t pid, uint64_t status_ptr, uint64_t options,
                 uint64_t rusage, uint64_t unused1, uint64_t unused2) {
    (void)options; (void)rusage; (void)unused1; (void)unused2;

    process_t* parent = current_process();

    // 等待任意子进程 (pid == -1) 或特定子进程
    process_t* child = NULL;

    while (1) {
        // 查找已退出的子进程
        child = find_zombie_child(parent, (int)pid);
        if (child) {
            break;
        }

        // 检查是否有子进程
        if (!has_children(parent)) {
            return -10;  // ECHILD
        }

        // 阻塞等待
        sleep_on(&parent->wait_queue);
    }

    // 获取退出状态
    if (status_ptr) {
        *(int*)status_ptr = child->exit_code;
    }

    pid_t child_pid = child->pid;

    // 回收子进程
    process_reap(child);

    return child_pid;
}
```

---

### C-11: sys_brk

```c
// void* brk(void* addr)
int64_t sys_brk(uint64_t addr, uint64_t unused1, uint64_t unused2,
                uint64_t unused3, uint64_t unused4, uint64_t unused5) {
    (void)unused1; (void)unused2; (void)unused3; (void)unused4; (void)unused5;

    process_t* proc = current_process();

    // 如果 addr 为 0，返回当前 brk
    if (addr == 0) {
        return proc->brk;
    }

    // 验证新地址
    if (addr < proc->brk_start || addr > proc->brk_max) {
        return -12;  // ENOMEM
    }

    uint64_t old_brk = proc->brk;
    uint64_t new_brk = (addr + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    // 扩展堆
    if (new_brk > old_brk) {
        for (uint64_t page = old_brk; page < new_brk; page += PAGE_SIZE) {
            uint64_t paddr = (uint64_t)pmm_alloc_page();
            vmm_map_page_in(proc->page_table, page, paddr,
                           PTE_PRESENT | PTE_WRITABLE | PTE_USER);
        }
    }
    // 收缩堆
    else if (new_brk < old_brk) {
        for (uint64_t page = new_brk; page < old_brk; page += PAGE_SIZE) {
            uint64_t paddr = vmm_get_phys_in(proc->page_table, page);
            vmm_unmap_page_in(proc->page_table, page);
            pmm_free_page((void*)paddr);
        }
    }

    proc->brk = new_brk;
    return new_brk;
}
```

---

## 用户态调用示例

用户程序调用系统调用的方式:

```c
// libc/syscall.h

static inline long syscall(long nr, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8  __asm__("r8")  = a5;
    register long r9  __asm__("r9")  = a6;
    __asm__ volatile(
        "syscall"
        : "=a"(ret)
        : "a"(nr), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
        : "rcx", "r11", "memory"
    );
    return ret;
}

// 示例
static inline void exit(int status) {
    syscall(SYS_EXIT, status, 0, 0, 0, 0, 0);
}

static inline ssize_t write(int fd, const void* buf, size_t count) {
    return syscall(SYS_WRITE, fd, (long)buf, count, 0, 0, 0);
}
```

---

## 验收标准

```
[SYSCALL] Table initialized (256 entries)

[TEST] Syscall (from kernel, simulated)
  sys_getpid: returned PID 1
  sys_write(1, "Hello", 5): returned 5
  sys_open("/dev/null", 0): returned fd 3
  sys_close(3): returned 0
[TEST] Syscall: PASSED

[TEST] Syscall (from user mode)
  [USER] Calling getpid...
  [USER] My PID is 2
  [USER] Calling write...
  Hello from userspace!
  [USER] Calling exit(0)...
  [KERNEL] Process 2 exited with code 0
[TEST] User syscall: PASSED
```

---

## 交付文件

| 文件 | 描述 |
|------|------|
| include/syscall_nr.h | 系统调用号定义 |
| kernel/syscall/syscall.h | syscall 头文件 |
| kernel/syscall/syscall.c | 分发表和初始化 |
| kernel/syscall/sys_io.c | read/write/open/close |
| kernel/syscall/sys_proc.c | fork/exec/exit/wait/getpid |
| kernel/syscall/sys_mem.c | brk/mmap |

---

## 与其他组的接口

| 需要 | 来源组 | 接口 |
|------|--------|------|
| syscall 入口 | 组 A | `syscall_entry` 汇编 |
| 文件操作 | 组 B | `vfs_read/write`, `fd_get` |
| 用户态跳转 | 组 A | `jump_to_usermode()` |

| 提供 | 给 | 接口 |
|------|-----|------|
| 系统调用处理 | 组 A | `syscall_handler()` |

---

## 风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| 用户指针验证不足 | 实现 copy_from_user/copy_to_user |
| fork 复制不完整 | 逐步测试每个资源复制 |
| exec 内存泄漏 | 先清理旧资源再加载新程序 |
