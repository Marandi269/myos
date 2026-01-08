# Group B: 文件系统 (File System)

## 概述

| 项目 | 说明 |
|------|------|
| 负责人 | 开发者 B |
| 任务范围 | FS-01 ~ FS-16 |
| 前置依赖 | Heap ✅, PMM ✅ |
| 并行组 | Group A (用户态准备), Group C (系统调用) |

## 目标

实现 VFS 层和基础文件系统，为用户程序提供文件操作：
1. VFS 抽象层
2. 文件描述符管理
3. ramfs 内存文件系统
4. devfs 设备文件系统
5. stdin/stdout/stderr 支持

---

## 任务清单

### 核心 VFS

| 任务ID | 任务名称 | 依赖 | 验证方式 |
|--------|----------|------|----------|
| FS-01 | VFS 抽象层 | Heap | 接口定义编译通过 |
| FS-02 | 文件描述符表 | FS-01 | fd 分配/回收 |
| FS-03 | 路径解析 | FS-01 | 路径分割测试 |
| FS-04 | 目录操作 | FS-01 | opendir/readdir |

### 文件系统实现

| 任务ID | 任务名称 | 依赖 | 验证方式 |
|--------|----------|------|----------|
| FS-10 | ramfs | FS-01 | 创建/读/写文件 |
| FS-11 | devfs | FS-01 | /dev/null, /dev/zero |
| FS-15 | procfs | FS-01 | /proc/self/status |
| FS-16 | stdin/stdout/stderr | FS-02, FS-11 | fd 0/1/2 工作 |

---

## FS-01: VFS 抽象层

### 核心数据结构

```c
// kernel/fs/vfs.h

#ifndef _VFS_H
#define _VFS_H

#include "types.h"

// 前向声明
struct inode;
struct file;
struct super_block;
struct file_system_type;

// 文件类型
#define S_IFREG  0100000  // 普通文件
#define S_IFDIR  0040000  // 目录
#define S_IFCHR  0020000  // 字符设备

// 打开标志
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

// inode 结构
struct inode {
    uint32_t i_ino;
    uint32_t i_mode;
    uint64_t i_size;
    struct inode_operations* i_op;
    struct file_operations* i_fop;
    void* i_private;
};

// inode 操作
struct inode_operations {
    struct inode* (*lookup)(struct inode* dir, const char* name);
    int (*create)(struct inode* dir, const char* name, uint32_t mode);
    int (*mkdir)(struct inode* dir, const char* name, uint32_t mode);
};

// 文件操作
struct file_operations {
    int (*open)(struct inode* inode, struct file* file);
    int (*close)(struct file* file);
    ssize_t (*read)(struct file* file, char* buf, size_t count);
    ssize_t (*write)(struct file* file, const char* buf, size_t count);
    off_t (*lseek)(struct file* file, off_t offset, int whence);
};

// 打开的文件
struct file {
    struct inode* f_inode;
    struct file_operations* f_op;
    uint64_t f_pos;
    uint32_t f_flags;
    uint32_t f_count;
    void* f_private;
};

// 超级块
struct super_block {
    struct file_system_type* s_type;
    struct inode* s_root;
    void* s_fs_info;
};

// 文件系统类型
struct file_system_type {
    const char* name;
    struct super_block* (*mount)(struct file_system_type* fs, const char* source);
    struct file_system_type* next;
};

// VFS 函数
void vfs_init(void);
int vfs_register_fs(struct file_system_type* fs);
int vfs_mount(const char* source, const char* target, const char* fstype);

struct file* vfs_open(const char* path, int flags);
int vfs_close(struct file* file);
ssize_t vfs_read(struct file* file, void* buf, size_t count);
ssize_t vfs_write(struct file* file, const void* buf, size_t count);

#endif
```

---

## FS-02: 文件描述符表

```c
// kernel/fs/fd.h

#ifndef _FD_H
#define _FD_H

#include "vfs.h"

#define MAX_FD 256

struct fd_table {
    struct file* fds[MAX_FD];
};

struct fd_table* fd_table_create(void);
void fd_table_destroy(struct fd_table* table);
int fd_alloc(struct fd_table* table, struct file* file);
int fd_free(struct fd_table* table, int fd);
struct file* fd_get(struct fd_table* table, int fd);

#endif
```

---

## FS-10: ramfs 实现

```c
// kernel/fs/ramfs/ramfs.c

// ramfs inode 私有数据
struct ramfs_inode {
    char* data;
    size_t capacity;
    struct ramfs_inode* children;
    struct ramfs_inode* sibling;
    char name[256];
};

// 文件操作: read/write
static ssize_t ramfs_read(struct file* file, char* buf, size_t count);
static ssize_t ramfs_write(struct file* file, const char* buf, size_t count);

// inode 操作: lookup/create
static struct inode* ramfs_lookup(struct inode* dir, const char* name);
static int ramfs_create(struct inode* dir, const char* name, uint32_t mode);

void ramfs_init(void);
```

---

## FS-11: devfs 设备文件系统

```c
// kernel/fs/devfs/devfs.c

// /dev/null - 读返回 EOF, 写丢弃
// /dev/zero - 读返回零, 写丢弃
// /dev/console - 连接串口

struct dev_entry {
    const char* name;
    struct file_operations* ops;
};

static struct dev_entry devices[] = {
    { "null", &null_ops },
    { "zero", &zero_ops },
    { "console", &console_ops },
    { NULL, NULL }
};

void devfs_init(void);
```

---

## FS-16: stdin/stdout/stderr

```c
// kernel/fs/stdio.c

// 为新进程设置标准文件描述符
int setup_stdio(struct fd_table* table) {
    struct file* console = vfs_open("/dev/console", O_RDWR);
    
    table->fds[0] = console;  // stdin
    table->fds[1] = console;  // stdout
    table->fds[2] = console;  // stderr
    
    return 0;
}
```

---

## 开发顺序

```
FS-01 VFS 抽象 ──┬──> FS-02 fd 表
                │
                ├──> FS-03 路径解析
                │
                ├──> FS-10 ramfs ──┐
                │                  │
                └──> FS-11 devfs ──┴──> FS-16 stdio
```

---

## 验收标准

### 里程碑 1: VFS 框架
```
[VFS] Initialized
[VFS] Registered filesystem: ramfs
[VFS] Mounted ramfs on /
```

### 里程碑 2: 基础文件操作
```
[TEST] Create file /test.txt: OK
[TEST] Write 'Hello': OK
[TEST] Read back: 'Hello'
```

### 里程碑 3: 设备文件
```
[TEST] Open /dev/null: OK
[TEST] Write to /dev/null: OK (discarded)
[TEST] Read /dev/zero: OK (zeros)
```

### 里程碑 4: stdio
```
[TEST] fd 0,1,2 bound to /dev/console
[TEST] write(1, "Hello") -> 串口输出
```

---

## 整合点

完成后与 Group A、Group C 整合：
- Group A 需要 VFS 加载用户 ELF 程序
- Group C 需要 fd 表实现 sys_read/sys_write

---

## 文件清单

| 文件 | 描述 |
|------|------|
| kernel/fs/vfs.h | VFS 接口定义 |
| kernel/fs/vfs.c | VFS 实现 |
| kernel/fs/fd.h | 文件描述符表 |
| kernel/fs/fd.c | fd 实现 |
| kernel/fs/path.c | 路径解析 |
| kernel/fs/ramfs/ramfs.c | ramfs 实现 |
| kernel/fs/devfs/devfs.c | devfs 实现 |
| kernel/fs/stdio.c | stdin/stdout/stderr |
