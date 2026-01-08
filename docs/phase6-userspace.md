# Phase 6: 用户空间 (User Space)

## 目标
实现用户态程序运行环境，包括 ELF 加载器、最小 C 库、init 进程和简易 Shell。

## 前置依赖
- ✅ Phase 3.5: 用户态准备 (GDT 用户段、TSS、SYSCALL MSR)
- ✅ Phase 4: 系统调用框架 (read/write/brk/exit/getpid)
- ✅ Phase 5: 文件系统 (VFS、ramfs、devfs、stdio)

---

## 任务列表

### U-01: ELF64 加载器
**优先级**: P0 (必须首先完成)
**依赖**: FS-03 (路径解析), FS-10 (ramfs), VMM

**描述**: 实现 ELF64 格式静态可执行文件的加载和解析。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| U-01.1 | ELF header 解析与验证 | 解析测试 ELF，打印 entry point |
| U-01.2 | Program header 解析 | 打印所有 PT_LOAD 段信息 |
| U-01.3 | 段加载 (PT_LOAD) | 将段内容加载到内存 |
| U-01.4 | 用户地址空间设置 | 创建用户页表，映射代码/数据段 |
| U-01.5 | 用户栈设置 | 在用户空间分配并映射栈页 |
| U-01.6 | 入口点跳转 | 跳转到用户态执行 ELF entry |

**文件**:
- `kernel/proc/elf.h` - ELF 结构定义
- `kernel/proc/elf.c` - ELF 加载器实现

**验证**:
```
[ELF] Loading /bin/hello
[ELF] Entry: 0x400000, Type: EXEC
[ELF] Loaded segment: 0x400000-0x401000 (RX)
[ELF] Loaded segment: 0x600000-0x601000 (RW)
[ELF] User stack: 0x7FFFFFFFE000
[ELF] Jumping to user mode...
Hello from user space!
```

---

### U-02: 最小 C 库 (libc)
**优先级**: P0
**依赖**: S-03 (sys_write), S-04 (sys_read), S-11 (sys_brk)

**描述**: 实现用户态 C 库的最小子集，足以支持基本程序运行。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| U-02.1 | 系统调用包装 | syscall() 宏/函数工作 |
| U-02.2 | _start 入口 | 调用 main() 并处理返回值 |
| U-02.3 | 标准 I/O (printf, puts) | 用户程序打印输出 |
| U-02.4 | 字符串函数 (strlen, strcmp, strcpy) | 字符串测试通过 |
| U-02.5 | 内存函数 (memset, memcpy, memmove) | 内存测试通过 |
| U-02.6 | 堆管理 (malloc, free) | 使用 sys_brk 实现 |
| U-02.7 | exit() | 调用 sys_exit |

**文件**:
```
libc/
├── include/
│   ├── stdio.h
│   ├── stdlib.h
│   ├── string.h
│   ├── unistd.h
│   └── syscall.h
├── src/
│   ├── crt0.S          # _start 入口
│   ├── syscall.S       # 系统调用包装
│   ├── stdio.c         # printf, puts
│   ├── stdlib.c        # malloc, free, exit
│   ├── string.c        # 字符串函数
│   └── memory.c        # memset, memcpy
└── Makefile
```

**验证**:
```c
// user/hello.c
#include <stdio.h>
int main() {
    printf("Hello, %s!\n", "World");
    return 0;
}
```
输出: `Hello, World!`

---

### U-03: initramfs 与文件部署
**优先级**: P0
**依赖**: U-01, U-02, FS-10 (ramfs)

**描述**: 创建 initramfs 镜像，将用户程序打包并在启动时加载。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| U-03.1 | CPIO/TAR 格式解析 | 解析测试归档文件 |
| U-03.2 | initramfs 嵌入内核 | 内核包含 initramfs 数据 |
| U-03.3 | 启动时解压到 ramfs | /bin, /etc 目录存在 |
| U-03.4 | 构建脚本 | make initramfs 生成镜像 |

**文件**:
- `kernel/fs/initramfs.c` - initramfs 解析
- `scripts/mkinitramfs.sh` - 打包脚本
- `initramfs/` - 初始文件系统内容

**目录结构**:
```
initramfs/
├── bin/
│   ├── init
│   ├── sh
│   ├── ls
│   ├── cat
│   └── echo
└── etc/
    └── inittab (可选)
```

**验证**:
```
[initramfs] Unpacking 5 files...
[initramfs] Created /bin/init (4096 bytes)
[initramfs] Created /bin/sh (8192 bytes)
[initramfs] Done
```

---

### U-04: init 进程
**优先级**: P0
**依赖**: U-01, U-02, U-03

**描述**: 实现 PID 1 的 init 进程，作为所有用户进程的祖先。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| U-04.1 | 内核启动 init | 加载并执行 /bin/init |
| U-04.2 | init 启动 shell | fork + exec /bin/sh |
| U-04.3 | 僵尸进程收割 | init wait() 子进程 |
| U-04.4 | 重启/关机处理 | 响应特定信号 |

**文件**:
- `userspace/init/init.c` - init 实现

**伪代码**:
```c
int main() {
    // 打开 stdin/stdout/stderr
    open("/dev/console", O_RDWR);  // fd 0
    dup(0);  // fd 1
    dup(0);  // fd 2

    printf("[init] Starting...\n");

    pid_t pid = fork();
    if (pid == 0) {
        // 子进程执行 shell
        exec("/bin/sh", NULL);
    }

    // 父进程循环等待子进程
    while (1) {
        wait(NULL);
    }
}
```

**验证**:
```
[init] MyOS init (PID 1)
[init] Starting /bin/sh...
$ _
```

---

### U-05: 简易 Shell
**优先级**: P1
**依赖**: U-04, S-05 (fork), S-06 (exec)

**描述**: 实现基本的命令行解释器。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| U-05.1 | 命令行读取 | 读取用户输入 |
| U-05.2 | 命令解析 | 分割命令和参数 |
| U-05.3 | 内建命令 (cd, exit, pwd) | cd / && pwd 输出 "/" |
| U-05.4 | 外部命令执行 | fork + exec /bin/ls |
| U-05.5 | 等待命令完成 | wait() 并显示返回值 |
| U-05.6 | 基本错误处理 | "command not found" |

**文件**:
- `userspace/shell/sh.c` - shell 实现

**验证**:
```
$ echo hello
hello
$ ls /bin
init
sh
ls
cat
echo
$ cat /etc/motd
Welcome to MyOS!
$ exit
```

---

### U-06: coreutils 基础工具
**优先级**: P1
**依赖**: U-02

**描述**: 实现最基本的命令行工具集。

**工具列表**:
| 命令 | 功能 | 系统调用依赖 |
|------|------|-------------|
| echo | 打印参数 | write |
| cat | 显示文件内容 | open, read, write, close |
| ls | 列出目录 | open, getdents, write, close |
| pwd | 打印当前目录 | getcwd |
| mkdir | 创建目录 | mkdir |
| rm | 删除文件 | unlink |
| cp | 复制文件 | open, read, write, close |

**文件**:
```
userspace/coreutils/
├── echo.c
├── cat.c
├── ls.c
├── pwd.c
├── mkdir.c
├── rm.c
└── cp.c
```

**验证**:
```
$ echo "Hello World"
Hello World
$ cat /test.txt
This is a test file.
$ ls /
bin
dev
etc
test.txt
```

---

### U-07: 补充系统调用
**优先级**: P0 (与其他任务并行)
**依赖**: S-02 (系统调用框架)

**描述**: 用户空间程序所需的额外系统调用。

**需要添加的系统调用**:
| 系统调用 | 用途 | 优先级 |
|----------|------|--------|
| fork | 创建子进程 | P0 |
| execve | 执行新程序 | P0 |
| waitpid | 等待子进程 | P0 |
| getcwd | 获取当前目录 | P1 |
| chdir | 改变当前目录 | P1 |
| getdents | 读取目录项 | P1 |
| mkdir | 创建目录 | P2 |
| unlink | 删除文件 | P2 |
| stat | 获取文件信息 | P2 |

**验证**: 各系统调用的单元测试通过

---

## 开发顺序

```
U-07 (补充 syscall: fork/exec/wait)
    │
    v
U-01 (ELF 加载器)
    │
    v
U-02 (libc 最小实现)
    │
    v
U-03 (initramfs)
    │
    v
U-04 (init 进程)
    │
    v
U-05 (Shell)
    │
    v
U-06 (coreutils)
```

---

## 验证里程碑

| 编号 | 里程碑 | 验证方式 |
|------|--------|----------|
| M6.1 | ELF 加载成功 | 加载并执行静态链接的 hello 程序 |
| M6.2 | libc 工作 | printf("Hello") 正常输出 |
| M6.3 | init 启动 | PID 1 运行，显示启动信息 |
| M6.4 | Shell 可用 | 能输入命令并执行 |
| M6.5 | 基本命令工作 | echo, cat, ls 正常 |

**最终验证**:
```
=============================
  Hello from MyOS!
  64-bit kernel running
=============================

[Kernel] Initializing...
[Kernel] Loading /bin/init...
[init] MyOS init (PID 1)
[init] Starting /bin/sh...

MyOS v0.1
$ echo "Welcome to MyOS!"
Welcome to MyOS!
$ ls /bin
init
sh
ls
cat
echo
$ cat /etc/motd
Welcome to MyOS - A Unix-like Operating System
$ exit
[init] Shell exited, respawning...
```

---

## 技术注意事项

### ELF 加载关键点
1. 只支持静态链接 ET_EXEC 类型
2. 地址空间布局:
   - 代码段: 0x400000+
   - 数据段: 0x600000+ (根据 ELF)
   - 堆: 数据段之后
   - 栈: 0x7FFFFFFFE000 (向下增长)
3. 必须设置正确的页权限 (R/W/X)

### libc 实现要点
1. _start 必须用汇编实现，设置栈并调用 main
2. printf 可以先实现简化版本 (只支持 %s, %d, %x)
3. malloc/free 使用简单的线性分配器即可

### fork/exec 实现
1. fork: 复制地址空间 (可用 COW 优化)
2. exec: 替换当前进程地址空间
3. 必须正确处理文件描述符继承

---

## 交付物清单

- [x] `kernel/proc/elf.c` - ELF 加载器
- [x] `kernel/proc/elf.h` - ELF 结构定义
- [x] `kernel/fs/initramfs.c` - initramfs CPIO 解析
- [x] `kernel/fs/initramfs.h` - initramfs 接口
- [x] `libc/` - 用户态 C 库
  - [x] `include/syscall.h` - 系统调用包装
  - [x] `include/stdio.h`, `stdlib.h`, `string.h`, `unistd.h` - 标准头文件
  - [x] `src/crt0.S` - C 运行时入口
  - [x] `src/string.c` - 字符串函数
  - [x] `src/stdlib.c` - malloc/free/exit
  - [x] `src/stdio.c` - printf/puts
  - [x] `src/unistd.c` - POSIX 系统调用
  - [x] `src/wait.c` - wait/waitpid
  - [x] `src/dirent.c` - 目录操作
- [x] `userspace/init/init.c` - init 进程 (PID 1)
- [x] `userspace/shell/sh.c` - 简易 shell
- [x] `userspace/coreutils/` - 基础工具
  - [x] `echo.c` - 打印参数
  - [x] `cat.c` - 显示文件内容
  - [x] `ls.c` - 列出目录
  - [x] `pwd.c` - 打印当前目录
  - [x] `mkdir.c` - 创建目录
  - [x] `hello.c` - 测试程序
- [x] `initramfs/` - 初始文件系统目录
- [x] `scripts/mkinitramfs.sh` - 打包脚本
- [x] 更新后的 Makefile

## 实现状态

| 任务 | 状态 | 说明 |
|------|------|------|
| U-07 补充系统调用 | ✅ 完成 | fork, execve, wait4, getcwd, chdir, mkdir, getdents64 |
| U-01 ELF64 加载器 | ✅ 完成 | 支持静态链接 ET_EXEC/ET_DYN |
| U-02 最小 libc | ✅ 完成 | printf, malloc, string 等 |
| U-03 initramfs | ✅ 完成 | CPIO newc 格式 |
| U-04 init 进程 | ✅ 完成 | PID 1, shell 重生 |
| U-05 简易 Shell | ✅ 完成 | 内建命令 + 外部命令 |
| U-06 coreutils | ✅ 完成 | echo, cat, ls, pwd, mkdir |

## 构建说明

```bash
# 构建用户空间程序
make userspace

# 生成 initramfs
make initramfs

# 构建完整系统
make full

# 运行
make run
```
