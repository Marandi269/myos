# MyOS

一个从头开发的 Unix-like 操作系统，运行于 x86_64 架构 (QEMU)。

## 当前状态

✅ **MVP 已完成** - 最小可运行内核

```
=============================
  Hello from MyOS!
  64-bit kernel running
=============================

[PIC] Initialized
[IDT] Initialized
[Keyboard] Initialized

[Kernel] Ready. Type something:
```

## 功能

- [x] GRUB2 Multiboot2 引导
- [x] 64 位长模式
- [x] 串口输出 (COM1)
- [x] 中断描述符表 (IDT)
- [x] 可编程中断控制器 (PIC)
- [x] PS/2 键盘驱动

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
# 构建
make clean && make

# 运行 (串口输出到终端)
make run

# 或者
./scripts/run.sh
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
├── Makefile            # 构建系统
├── linker.ld           # 链接脚本
├── grub.cfg            # GRUB 配置
├── kernel/             # 内核源码
│   ├── boot.S          # 启动汇编
│   ├── main.c          # 内核入口
│   ├── serial.c/h      # 串口驱动
│   ├── idt.c/h         # 中断描述符表
│   ├── pic.c/h         # PIC 驱动
│   └── keyboard.c/h    # 键盘驱动
├── include/            # 公共头文件
│   └── types.h         # 基本类型定义
├── scripts/            # 辅助脚本
│   ├── run.sh          # 运行脚本
│   └── debug.sh        # 调试脚本
└── docs/               # 文档
    ├── os-design-plan.md   # 完整开发计划
    └── mvp-minimal-os.md   # MVP 任务文档
```

## 开发计划

详见 [docs/os-design-plan.md](docs/os-design-plan.md)

### 下一步

- [ ] 物理内存管理
- [ ] 虚拟内存 (分页)
- [ ] 内核堆 (kmalloc)
- [ ] PIT 定时器
- [ ] 进程调度
- [ ] 系统调用
- [ ] 用户空间

## 文档

- [完整开发计划](docs/os-design-plan.md) - 9 个阶段、100+ 可拆分任务
- [MVP 任务文档](docs/mvp-minimal-os.md) - 最小可运行内核的详细任务 ✅
- [阶段 2 计划](docs/phase2-plan.md) - 内存管理、定时器、调试增强

## 许可证

MIT License
