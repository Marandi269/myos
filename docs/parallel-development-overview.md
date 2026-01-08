# 并行开发计划总览

## 当前状态

| 阶段 | 状态 | 说明 |
|------|------|------|
| 0-2.5 | ✅ 完成 | 基础设施、内存管理、VMM |
| 3 (核心) | ✅ 完成 | PCB、调度器、上下文切换 |
| 3.5 + 4 + 5 | 🔄 并行进行 | 三组同时开发 |

---

## 并行开发组

```
┌─────────────────────────────────────────────────────────────────────┐
│                         并行开发阶段                                  │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   ┌───────────────┐   ┌───────────────┐   ┌───────────────┐        │
│   │    组 A       │   │    组 B       │   │    组 C       │        │
│   │  用户态准备    │   │   文件系统    │   │  系统调用     │        │
│   │               │   │               │   │               │        │
│   │ - GDT 用户段  │   │ - VFS 框架    │   │ - 分发表      │        │
│   │ - TSS 设置    │   │ - ramfs       │   │ - sys_write   │        │
│   │ - SYSCALL MSR │   │ - devfs       │   │ - sys_read    │        │
│   │ - 用户地址空间│   │ - fd 表       │   │ - sys_open    │        │
│   │ - 态切换      │   │ - stdio       │   │ - sys_fork    │        │
│   └───────┬───────┘   └───────┬───────┘   └───────┬───────┘        │
│           │                   │                   │                │
│           └───────────────────┼───────────────────┘                │
│                               │                                    │
│                               ▼                                    │
│                    ┌─────────────────────┐                        │
│                    │      整合测试        │                        │
│                    │  用户程序可运行      │                        │
│                    └──────────┬──────────┘                        │
│                               │                                    │
│                               ▼                                    │
│                    ┌─────────────────────┐                        │
│                    │     阶段 6          │                        │
│                    │  libc + shell       │                        │
│                    └─────────────────────┘                        │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 各组详细文档

| 组 | 文档 | 任务数 | 关键交付 |
|---|------|--------|----------|
| A | [phase-4-syscall-group-A.md](phase-4-syscall-group-A.md) | 8 | TSS, GDT, SYSCALL, 态切换 |
| B | [phase-5-filesystem-group-B.md](phase-5-filesystem-group-B.md) | 8 | VFS, ramfs, devfs, fd |
| C | [phase-4-syscall-group-C.md](phase-4-syscall-group-C.md) | 12 | syscall 分发, 核心系统调用 |

---

## 组间依赖关系

```
组 A 提供:
├── syscall_entry ──────────────────> 组 C (syscall_handler)
├── tss_set_rsp0 ───────────────────> 调度器 (上下文切换)
├── jump_to_usermode ───────────────> 组 C (sys_exec)
└── create_user_address_space ──────> 组 C (sys_fork)

组 B 提供:
├── vfs_lookup ─────────────────────> 组 C (sys_open)
├── vfs_read/write ─────────────────> 组 C (sys_read/write)
├── fd_get/fd_alloc/fd_free ────────> 组 C (文件操作)
├── fd_table_clone ─────────────────> 组 C (sys_fork)
└── setup_stdio ────────────────────> 组 C (进程初始化)

组 C 提供:
└── syscall_handler ────────────────> 组 A (syscall_entry 调用)
```

---

## 开发顺序

### 第一周: 基础框架

| 组 | 任务 | 产出 |
|---|------|------|
| A | A-01 ~ A-04 | GDT, TSS, SYSCALL MSR |
| B | B-01 ~ B-04 | VFS 框架, fd 表 |
| C | C-01 | syscall 分发表 |

**周末整合点**:
- 组 A 完成 TSS，组 C 可以测试 syscall 陷入内核
- 组 B 完成 fd 表，组 C 可以实现 sys_write

### 第二周: 核心功能

| 组 | 任务 | 产出 |
|---|------|------|
| A | A-05 ~ A-08 | 用户地址空间, 态切换 |
| B | B-05 ~ B-08 | ramfs, devfs, stdio |
| C | C-02 ~ C-07 | read, write, open, close, exit, getpid |

**周末整合点**:
- 简单用户程序可以运行
- 可以调用 write 输出到串口

### 第三周: 完善

| 组 | 任务 | 产出 |
|---|------|------|
| A | 调试和优化 | 稳定的态切换 |
| B | 挂载系统完善 | 完整的 VFS |
| C | C-08 ~ C-12 | fork, exec, wait, brk |

**周末整合点**:
- fork + exec 工作
- 可以加载并运行 ELF 程序

---

## 接口约定

### 组 A 导出接口

```c
// kernel/proc/gdt.h
#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20
#define GDT_TSS         0x28

// kernel/proc/tss.h
void tss_init(void);
void tss_set_rsp0(uint64_t rsp0);

// kernel/proc/user.h
pml4e_t* create_user_address_space(void);
void setup_user_stack(pml4e_t* pml4, uint64_t* user_sp);
void jump_to_usermode(uint64_t entry, uint64_t user_sp);

// kernel/proc/syscall_entry.S
extern void syscall_entry(void);
```

### 组 B 导出接口

```c
// kernel/fs/vfs.h
void vfs_init(void);
vfs_node_t* vfs_lookup(const char* path);
ssize_t vfs_read(vfs_node_t* node, void* buf, size_t size, uint64_t offset);
ssize_t vfs_write(vfs_node_t* node, const void* buf, size_t size, uint64_t offset);

// kernel/fs/fd.h
fd_table_t* fd_table_create(void);
fd_table_t* fd_table_clone(fd_table_t* table);
void fd_table_destroy(fd_table_t* table);
int fd_alloc(fd_table_t* table, file_t* file);
int fd_free(fd_table_t* table, int fd);
file_t* fd_get(fd_table_t* table, int fd);

// kernel/fs/stdio.h
void setup_stdio(fd_table_t* table);
```

### 组 C 导出接口

```c
// kernel/syscall/syscall.h
void syscall_init(void);
int64_t syscall_handler(uint64_t nr, ...);  // 由组 A 调用

// 系统调用函数 (内部使用)
int64_t sys_read(...);
int64_t sys_write(...);
// ...
```

---

## 验收里程碑

### M1: syscall 基础 (第一周末)

```
[TSS] Initialized
[SYSCALL] MSR configured
[SYSCALL] Table initialized

[TEST] Trigger syscall from kernel
  syscall_handler called with nr=1
[TEST] PASSED
```

### M2: 用户态输出 (第二周末)

```
[VFS] Initialized
[devfs] Mounted
[STDIO] fd 0/1/2 -> /dev/console

[TEST] Jump to user mode
  [USER] Hello from ring 3!
  [USER] sys_write returned 20
[TEST] PASSED
```

### M3: 进程管理 (第三周末)

```
[TEST] Fork test
  Parent PID: 1
  sys_fork returned 2 (child PID)
  [Child] My PID is 2
  [Parent] Child exited

[TEST] Exec test
  Loading /bin/hello...
  [USER] Hello World!
[TEST] PASSED
```

---

## 沟通机制

1. **每日站会**: 各组报告进度和阻塞
2. **接口变更**: 提前通知其他组
3. **整合分支**: 每周末合并到 main

---

## 风险矩阵

| 风险 | 影响组 | 缓解措施 |
|------|--------|----------|
| TSS 设置错误 | A, C | 使用 QEMU -d int 调试 |
| syscall 寄存器约定不一致 | A, C | 统一使用 Linux ABI |
| fd 并发访问 | B, C | 先单进程测试 |
| 用户指针验证 | C | 实现 copy_from_user |
