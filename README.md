# MyOS

一个从头开发的 Unix-like 操作系统，运行于 x86_64 架构 (QEMU)。

## 当前状态

✅ **Phase 7 IPC 已完成** - 管道、信号、共享内存

```
=============================
  Hello from MyOS!
  64-bit kernel running
=============================

[Kernel] Initializing...
[init] MyOS init (PID 1)
[init] Starting /bin/sh...

MyOS Shell v0.2
$ ls /bin | cat
cat
echo
hello
init
ls
mkdir
pwd
sh
$ kill -9 5
$ echo "Hello World"
Hello World
```

## 进度总览

| 阶段 | 名称 | 状态 |
|------|------|------|
| 0 | 基础设施 | ✅ 完成 |
| 1 | Bootloader | ✅ 完成 |
| 2 | 基础内核服务 | ✅ 完成 |
| 2.5 | 虚拟内存 | ✅ 完成 |
| 3 | 进程管理 | ✅ 完成 |
| 4 | 系统调用 | ✅ 完成 |
| 5 | 文件系统 | ✅ 完成 |
| 6 | 用户空间 | ✅ 完成 |
| 7 | IPC | ✅ 完成 |
| 8 | 网络栈 | ⏳ 可选 |
| 9 | 高级特性 | ⏳ 可选 |

## 功能

### 内核
- [x] GRUB2 Multiboot2 引导
- [x] 64 位长模式
- [x] GDT/IDT/TSS
- [x] PIC 中断控制器
- [x] PIT 定时器 (100Hz)
- [x] PS/2 键盘驱动
- [x] 串口输出 (COM1)

### 内存管理
- [x] 物理内存管理 (PMM, 位图分配器)
- [x] 虚拟内存管理 (VMM, 4级页表)
- [x] 内核堆 (kmalloc/kfree)
- [x] 缺页异常处理

### 进程管理
- [x] 进程控制块 (PCB)
- [x] Round-Robin 调度器
- [x] 上下文切换
- [x] 用户态/内核态切换
- [x] fork/exec/wait

### 文件系统
- [x] VFS 虚拟文件系统
- [x] ramfs 内存文件系统
- [x] devfs 设备文件系统 (/dev/null, /dev/zero, /dev/console)
- [x] initramfs (CPIO newc 格式)
- [x] 文件描述符表

### 系统调用
- [x] read/write/open/close
- [x] fork/execve/exit/wait4
- [x] brk/mmap/munmap
- [x] getpid/getppid/getcwd/chdir
- [x] pipe/dup/dup2
- [x] kill/signal/sigaction

### IPC 进程间通信
- [x] 管道 (pipe)
- [x] 信号 (signal)
- [x] 共享内存 (mmap MAP_SHARED)

### 用户空间
- [x] ELF64 加载器
- [x] 最小 libc (printf, malloc, string 等)
- [x] init 进程 (PID 1)
- [x] Shell (内建命令 + 管道支持)
- [x] coreutils (echo, cat, ls, pwd, mkdir, kill)

## 快速开始

### 依赖

```bash
# Ubuntu/Debian
sudo apt install build-essential grub-pc-bin grub-common xorriso qemu-system-x86

# Arch Linux
sudo pacman -S base-devel grub xorriso qemu
```

### 构建与运行

```bash
# 构建完整系统 (内核 + 用户空间 + initramfs)
make full

# 运行
make run

# 或者分步构建
make clean
make userspace    # 构建用户空间程序
make initramfs    # 生成 initramfs
make              # 构建内核
```

按 `Ctrl+A` 然后 `X` 退出 QEMU。

### 调试

```bash
# 启动 QEMU 并等待 GDB 连接
./scripts/debug.sh

# 另一个终端
gdb -ex "target remote :1234" -ex "symbol-file kernel.bin"
```

## 项目结构

```
myos/
├── Makefile              # 构建系统
├── linker.ld             # 链接脚本
├── grub.cfg              # GRUB 配置
├── kernel/               # 内核源码
│   ├── boot.S            # 启动汇编
│   ├── main.c            # 内核入口
│   ├── mm/               # 内存管理
│   │   ├── pmm.c         # 物理内存管理
│   │   ├── vmm.c         # 虚拟内存管理
│   │   └── heap.c        # 内核堆
│   ├── proc/             # 进程管理
│   │   ├── process.c     # 进程控制
│   │   ├── scheduler.c   # 调度器
│   │   ├── syscall.c     # 系统调用
│   │   └── elf.c         # ELF 加载器
│   ├── fs/               # 文件系统
│   │   ├── vfs.c         # VFS 层
│   │   ├── ramfs/        # ramfs
│   │   ├── devfs/        # devfs
│   │   └── initramfs.c   # initramfs 解析
│   ├── ipc/              # 进程间通信
│   │   ├── pipe.c        # 管道
│   │   ├── signal.c      # 信号
│   │   └── shm.c         # 共享内存
│   ├── drivers/          # 设备驱动
│   │   └── pit.c         # PIT 定时器
│   └── lib/              # 内核库
│       ├── string.c      # 字符串函数
│       └── kprintf.c     # 内核打印
├── libc/                 # 用户态 C 库
│   ├── include/          # 头文件
│   └── src/              # 实现
├── userspace/            # 用户空间程序
│   ├── init/             # init 进程
│   ├── shell/            # Shell
│   └── coreutils/        # 基础命令
├── initramfs/            # 初始文件系统
├── scripts/              # 辅助脚本
└── docs/                 # 文档
```

## 开发计划

详见 [docs/os-design-plan.md](docs/os-design-plan.md)

### 文档索引

| 文档 | 描述 |
|------|------|
| [os-design-plan.md](docs/os-design-plan.md) | 完整开发计划 (9 阶段) |
| [phase6-userspace.md](docs/phase6-userspace.md) | 用户空间详细计划 |
| [phase7-ipc.md](docs/phase7-ipc.md) | IPC 详细计划 |
| [bugs.md](docs/bugs.md) | 已知问题追踪 |

### 下一步 (可选)

- [ ] Phase 8: 网络栈 (TCP/IP, virtio-net)
- [ ] Phase 9: 高级特性 (SMP 多核, pthread 线程)

## 许可证

MIT License
