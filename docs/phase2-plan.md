# 阶段 2: 基础内核服务 - 下一步计划

## 当前状态
MVP 已完成 ✅
- 64位内核启动
- 串口输出
- 基础中断 (IDT/PIC)
- 键盘回显

---

## 阶段 2 目标
完善内核基础设施，为进程管理做准备。

---

## 并行开发任务组

### 组 A: 内存管理 (关键路径)
**优先级**: 🔴 最高 - 阻塞后续所有功能

| 任务ID | 任务名称 | 依赖 | 验证方式 |
|--------|----------|------|----------|
| M-01 | 物理内存检测 | MVP | 串口打印内存布局 |
| M-02 | 页帧分配器 (Bitmap) | M-01 | 分配/释放测试 |
| M-03 | 内核堆 (kmalloc/kfree) | M-02 | 动态分配测试 |

#### M-01: 物理内存检测
```c
// 从 Multiboot2 获取内存映射
// 产出: kernel/mm/pmm.c, kernel/mm/pmm.h

struct memory_region {
    uint64_t base;
    uint64_t length;
    uint32_t type;  // 1=可用, 2=保留
};

void pmm_init(void* multiboot_info);
void pmm_print_memory_map(void);
uint64_t pmm_get_total_memory(void);
```

#### M-02: 页帧分配器
```c
// Bitmap 方式管理 4KB 页帧
// 产出: kernel/mm/pmm.c 扩展

void* pmm_alloc_page(void);      // 分配一个 4KB 页
void  pmm_free_page(void* addr); // 释放页
uint64_t pmm_get_free_pages(void);
```

#### M-03: 内核堆
```c
// 简单的堆分配器 (可用 slab 或 buddy)
// 产出: kernel/mm/heap.c, kernel/mm/heap.h

void  heap_init(void);
void* kmalloc(size_t size);
void  kfree(void* ptr);
void* krealloc(void* ptr, size_t size);
```

**验证脚本**:
```c
void test_memory(void) {
    serial_print("[TEST] Memory allocation\n");

    // 测试页分配
    void* page1 = pmm_alloc_page();
    void* page2 = pmm_alloc_page();
    assert(page1 != page2);
    pmm_free_page(page1);

    // 测试堆分配
    char* buf = kmalloc(1024);
    buf[0] = 'A';
    buf[1023] = 'Z';
    kfree(buf);

    serial_print("[TEST] Memory: PASSED\n");
}
```

---

### 组 B: 调试增强 (可并行)
**优先级**: 🟡 中等 - 提升开发效率

| 任务ID | 任务名称 | 依赖 | 验证方式 |
|--------|----------|------|----------|
| D-01 | kprintf 格式化输出 | MVP | printf 风格输出 |
| D-02 | 栈回溯 (backtrace) | D-01 | panic 时打印调用栈 |
| D-03 | kernel panic 完善 | D-01, D-02 | 异常时详细信息 |

#### D-01: kprintf
```c
// 产出: kernel/lib/kprintf.c

int kprintf(const char* fmt, ...);

// 支持格式:
// %d, %u, %x, %X - 整数
// %s - 字符串
// %c - 字符
// %p - 指针
// %% - 百分号
```

#### D-02: 栈回溯
```c
// 产出: kernel/lib/backtrace.c

void print_backtrace(void);

// 输出示例:
// Backtrace:
//   [0] 0xFFFF8000001234 kernel_panic+0x20
//   [1] 0xFFFF8000005678 page_fault_handler+0x40
//   [2] 0xFFFF800000ABCD isr_stub_14+0x10
```

#### D-03: kernel panic
```c
// 产出: kernel/lib/panic.c

void kernel_panic(const char* msg, ...);

// panic 时输出:
// ==================== KERNEL PANIC ====================
// Message: Division by zero
// RIP: 0xFFFF800000123456
// RSP: 0xFFFF800000ABCDEF
//
// Backtrace:
//   ...
// ======================================================
```

---

### 组 C: 中断完善 (可并行)
**优先级**: 🟡 中等 - 为调度器准备

| 任务ID | 任务名称 | 依赖 | 验证方式 |
|--------|----------|------|----------|
| I-01 | 完整异常处理 (0-31) | MVP | 各种异常可捕获 |
| I-02 | PIT 定时器 | MVP | 定时中断触发 |
| I-03 | 中断计数统计 | I-02 | 打印中断统计 |

#### I-02: PIT 定时器
```c
// 产出: kernel/drivers/pit.c, kernel/drivers/pit.h

#define PIT_FREQUENCY 100  // 100Hz = 10ms 间隔

void pit_init(uint32_t frequency);
uint64_t pit_get_ticks(void);
void pit_handler(void);  // IRQ0 处理

// 可选: 简单的 sleep
void sleep_ms(uint32_t ms);
```

**验证**:
```c
void test_timer(void) {
    uint64_t start = pit_get_ticks();
    sleep_ms(1000);  // 等待 1 秒
    uint64_t elapsed = pit_get_ticks() - start;
    kprintf("Elapsed ticks: %d (expected ~100)\n", elapsed);
}
```

---

### 组 D: 代码质量 (可并行)
**优先级**: 🟢 低 - 长期维护

| 任务ID | 任务名称 | 依赖 | 验证方式 |
|--------|----------|------|----------|
| Q-01 | 修复链接警告 | MVP | 无 warning |
| Q-02 | 添加 test.sh 自动测试 | MVP | CI 可用 |
| Q-03 | 添加 assert 宏 | D-01 | assert 失败时 panic |

---

## 目录结构更新

```
myos/
├── kernel/
│   ├── boot.S
│   ├── main.c
│   ├── mm/                  # 新增
│   │   ├── pmm.c           # 物理内存管理
│   │   ├── pmm.h
│   │   ├── heap.c          # 内核堆
│   │   └── heap.h
│   ├── lib/                 # 新增
│   │   ├── kprintf.c
│   │   ├── panic.c
│   │   ├── backtrace.c
│   │   └── string.c        # memset, memcpy 等
│   ├── drivers/
│   │   ├── serial.c
│   │   ├── keyboard.c
│   │   └── pit.c           # 新增
│   └── interrupt/           # 重构
│       ├── idt.c
│       ├── pic.c
│       └── exceptions.c    # 新增: 完整异常处理
```

---

## 开发顺序建议

```
Week 1: 并行启动
┌─────────────────────────────────────────────────┐
│  开发者 A          开发者 B          开发者 C   │
│  ─────────         ─────────         ─────────  │
│  M-01 内存检测     D-01 kprintf      I-01 异常  │
│       │                │                  │     │
│       ▼                ▼                  ▼     │
│  M-02 页帧分配     D-02 backtrace    I-02 PIT  │
└─────────────────────────────────────────────────┘
                         │
                         ▼
Week 2: 整合
┌─────────────────────────────────────────────────┐
│            M-03 内核堆 (依赖 M-02)               │
│            D-03 panic (依赖 D-01, D-02)         │
│            I-03 中断统计 (依赖 I-02)            │
└─────────────────────────────────────────────────┘
                         │
                         ▼
验证里程碑: 内存分配正常，定时器工作，panic 信息完整
```

---

## 验收标准

### 阶段 2 完成标志

1. **内存测试通过**
   ```
   [Memory] Total: 128 MB, Free: 120 MB
   [TEST] pmm_alloc_page: PASSED
   [TEST] kmalloc/kfree: PASSED
   ```

2. **定时器工作**
   ```
   [PIT] Initialized at 100 Hz
   [Timer] Tick: 100 (1 second elapsed)
   ```

3. **Panic 信息完整**
   ```
   ==================== KERNEL PANIC ====================
   Division by zero at 0xFFFF800000123456
   Backtrace:
     [0] 0xFFFF800000123456
     [1] 0xFFFF800000789ABC
   ======================================================
   ```

---

## 风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| Multiboot2 解析复杂 | 先硬编码内存大小，后完善 |
| 堆分配器 bug | 使用简单的线性分配器开始 |
| 定时器不准 | 先用 PIT，后升级到 HPET/TSC |

---

## 下一阶段预告

**阶段 3: 进程管理** (依赖阶段 2 完成)
- 进程控制块 (PCB)
- 上下文切换
- 简单调度器 (Round-Robin)
- 内核线程
