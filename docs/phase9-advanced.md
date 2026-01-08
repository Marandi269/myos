# Phase 9: 高级特性 (Advanced Features)

## 目标
实现高级操作系统特性，包括 SMP 多核支持、线程、动态链接和图形显示。

## 前置依赖
- ✅ Phase 0-7: 核心功能完成
- ⏳ Phase 8: 网络栈 (可选)

---

## 任务列表

### A-01: SMP 多核支持
**优先级**: P1
**依赖**: I-04 (APIC), P-03 (调度器)

**描述**: 支持多处理器/多核系统。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| A-01.1 | ACPI/MADT 解析 | 检测 CPU 数量 |
| A-01.2 | Local APIC 初始化 | 每个 CPU 的 LAPIC |
| A-01.3 | I/O APIC 初始化 | 中断路由 |
| A-01.4 | AP 启动 (IPI) | 启动其他 CPU |
| A-01.5 | 每 CPU 数据结构 | per-cpu 变量 |
| A-01.6 | 自旋锁实现 | 多核同步原语 |
| A-01.7 | SMP 调度器 | 多核负载均衡 |

**文件**:
- `kernel/acpi/acpi.c` - ACPI 表解析
- `kernel/drivers/apic.c` - APIC 驱动
- `kernel/proc/smp.c` - SMP 初始化
- `kernel/lib/spinlock.c` - 自旋锁

**验证**:
```
[ACPI] Found 4 CPUs
[SMP] CPU 0 (BSP) online
[SMP] CPU 1 online
[SMP] CPU 2 online
[SMP] CPU 3 online
[SMP] All 4 CPUs initialized
```

---

### A-02: 线程支持 (pthread)
**优先级**: P1
**依赖**: P-03 (进程管理), M-05 (用户空间内存)

**描述**: 实现 POSIX 线程 (pthread) API。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| A-02.1 | clone() 系统调用 | 共享地址空间创建线程 |
| A-02.2 | 线程本地存储 (TLS) | __thread 变量工作 |
| A-02.3 | pthread_create | 创建线程 |
| A-02.4 | pthread_join | 等待线程 |
| A-02.5 | pthread_exit | 退出线程 |
| A-02.6 | pthread_mutex | 互斥锁 |
| A-02.7 | pthread_cond | 条件变量 |

**系统调用**:
| 调用 | 功能 |
|------|------|
| clone | 创建线程/进程 |
| futex | 快速用户态互斥 |
| set_tid_address | 设置线程 ID 地址 |
| set_thread_area | 设置 TLS |

**验证**:
```c
void *thread_func(void *arg) {
    printf("Thread %d running\n", (int)arg);
    return NULL;
}

pthread_t t1, t2;
pthread_create(&t1, NULL, thread_func, (void*)1);
pthread_create(&t2, NULL, thread_func, (void*)2);
pthread_join(t1, NULL);
pthread_join(t2, NULL);
```

---

### A-03: 动态链接器
**优先级**: P2
**依赖**: U-01 (ELF 加载器)

**描述**: 支持动态链接的共享库 (.so)。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| A-03.1 | ELF 动态段解析 | 解析 PT_DYNAMIC |
| A-03.2 | 共享库加载 | 加载 .so 文件 |
| A-03.3 | 符号解析 | 查找符号地址 |
| A-03.4 | 重定位处理 | R_X86_64_* 重定位 |
| A-03.5 | PLT/GOT | 延迟绑定 |
| A-03.6 | ld.so 实现 | 动态链接器 |

**验证**:
```bash
# 编译共享库
gcc -shared -fPIC -o libfoo.so foo.c

# 使用共享库
gcc -o app app.c -L. -lfoo
./app  # 动态加载 libfoo.so
```

---

### A-04: TTY/PTY 子系统
**优先级**: P2
**依赖**: FS-11 (devfs), I-05 (键盘)

**描述**: 实现终端设备和伪终端。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| A-04.1 | TTY 行规程 | 行编辑、回显 |
| A-04.2 | 终端控制 | termios 接口 |
| A-04.3 | 作业控制 | 前台/后台进程组 |
| A-04.4 | PTY master/slave | /dev/ptmx |
| A-04.5 | 控制终端 | /dev/tty |

**验证**:
```c
// 设置终端属性
struct termios t;
tcgetattr(0, &t);
t.c_lflag &= ~ECHO;  // 关闭回显
tcsetattr(0, TCSANOW, &t);
```

---

### A-05: Framebuffer 图形
**优先级**: P2
**依赖**: M-03 (VMM)

**描述**: 实现基本的图形显示支持。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| A-05.1 | 获取 framebuffer 地址 | Multiboot2 信息 |
| A-05.2 | 像素绘制 | 绘制像素点 |
| A-05.3 | 矩形填充 | 填充矩形区域 |
| A-05.4 | 位图字体 | 文字渲染 |
| A-05.5 | /dev/fb0 设备 | mmap 访问 |
| A-05.6 | 简单窗口系统 | 基础 GUI (可选) |

**验证**:
```c
// 直接写 framebuffer
int fd = open("/dev/fb0", O_RDWR);
void *fb = mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
// 绘制红色像素
((uint32_t*)fb)[y * width + x] = 0xFF0000;
```

---

### A-06: USB 支持
**优先级**: P3
**依赖**: I-02 (中断), M-04 (Heap)

**描述**: 实现 USB 主机控制器和设备驱动。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| A-06.1 | UHCI/OHCI 控制器 | USB 1.x 支持 |
| A-06.2 | EHCI 控制器 | USB 2.0 支持 |
| A-06.3 | xHCI 控制器 | USB 3.x 支持 |
| A-06.4 | USB 设备枚举 | 检测 USB 设备 |
| A-06.5 | USB HID 驱动 | USB 键盘/鼠标 |
| A-06.6 | USB 存储驱动 | USB 闪存盘 |

**验证**:
```
[USB] EHCI controller found
[USB] Device connected: VID=1234 PID=5678
[USB] HID keyboard detected
```

---

## 开发顺序

```
A-01 (SMP) ──────────────────┐
                             │
A-02 (pthread) ──────────────┤
                             │
A-03 (动态链接) ─────────────┤
                             │
A-04 (TTY/PTY) ──────────────┤
                             │
A-05 (Framebuffer) ──────────┤
                             │
A-06 (USB) ──────────────────┘
```

**建议顺序** (按实用性):
1. **A-02 pthread** - 用户程序多线程支持
2. **A-01 SMP** - 多核利用
3. **A-04 TTY/PTY** - 终端功能完善
4. **A-05 Framebuffer** - 图形显示
5. **A-03 动态链接** - 共享库支持
6. **A-06 USB** - 外设支持

---

## 验证里程碑

| 编号 | 里程碑 | 验证方式 |
|------|--------|----------|
| M9.1 | 多核启动 | 所有 CPU 上线 |
| M9.2 | 线程工作 | pthread 测试通过 |
| M9.3 | 终端完善 | vi/编辑器可用 |
| M9.4 | 图形显示 | 显示图像 |
| M9.5 | 共享库 | 动态加载 .so |

---

## 技术注意事项

### SMP 要点
1. BSP (Bootstrap Processor) 负责初始化
2. AP (Application Processor) 通过 INIT-SIPI-SIPI 启动
3. 需要处理缓存一致性
4. 临界区需要正确的锁保护

### 线程要点
1. clone() 需要正确设置共享标志 (CLONE_VM, CLONE_FS, etc.)
2. TLS 需要设置 FS/GS 段基址
3. futex 是用户态同步的基础

### Framebuffer 要点
1. QEMU 使用 `-vga std` 或 `-vga virtio`
2. 从 Multiboot2 获取 framebuffer 信息
3. 颜色格式通常是 BGRA 或 RGBA

---

## 交付物清单

### A-01 SMP
- [ ] `kernel/acpi/acpi.c/h` - ACPI 解析
- [ ] `kernel/drivers/apic.c/h` - APIC 驱动
- [ ] `kernel/proc/smp.c/h` - SMP 初始化
- [ ] `kernel/lib/spinlock.c/h` - 自旋锁

### A-02 pthread
- [ ] `kernel/proc/clone.c` - clone 实现
- [ ] `kernel/proc/futex.c` - futex 实现
- [ ] `libc/src/pthread.c` - pthread 库
- [ ] `libc/include/pthread.h` - pthread 头文件

### A-04 TTY
- [ ] `kernel/drivers/tty.c/h` - TTY 驱动
- [ ] `kernel/drivers/pty.c/h` - PTY 驱动
- [ ] `libc/include/termios.h` - termios 定义

### A-05 Framebuffer
- [ ] `kernel/drivers/fb.c/h` - Framebuffer 驱动
- [ ] `kernel/lib/font.c` - 位图字体
- [ ] `userspace/fbtest.c` - 测试程序

### A-06 USB
- [ ] `kernel/drivers/usb/usb.c/h` - USB 核心
- [ ] `kernel/drivers/usb/ehci.c` - EHCI 控制器
- [ ] `kernel/drivers/usb/hid.c` - HID 驱动
- [ ] `kernel/drivers/usb/storage.c` - 存储驱动
