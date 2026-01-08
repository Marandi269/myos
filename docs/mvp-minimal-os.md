# MVP: 最小可运行操作系统

## 目标
在 QEMU 上启动一个最小的 64 位操作系统内核，能够：
1. 从 GRUB 引导启动
2. 进入 64 位长模式
3. 通过串口输出 "Hello from MyOS!"
4. 响应键盘中断并回显字符

---

## 架构图

```
┌─────────────────────────────────────────┐
│              QEMU x86_64                │
├─────────────────────────────────────────┤
│  GRUB2 (Multiboot2)                     │
│         │                               │
│         ▼                               │
│  ┌─────────────────────────────────┐   │
│  │      boot.S (汇编入口)           │   │
│  │  - 设置栈                        │   │
│  │  - 加载 GDT                      │   │
│  │  - 启用长模式                    │   │
│  │  - 跳转到 kernel_main            │   │
│  └─────────────────────────────────┘   │
│         │                               │
│         ▼                               │
│  ┌─────────────────────────────────┐   │
│  │      kernel_main (C入口)         │   │
│  │  - 初始化串口                    │   │
│  │  - 初始化 IDT                    │   │
│  │  - 初始化 PIC                    │   │
│  │  - 启用键盘中断                  │   │
│  │  - 进入主循环                    │   │
│  └─────────────────────────────────┘   │
└─────────────────────────────────────────┘
```

---

## 任务分配

### 团队角色

| 角色 | 负责模块 | 可并行 |
|------|----------|--------|
| **开发者 A** | 构建系统 + 引导代码 | 独立启动 |
| **开发者 B** | 串口驱动 + 打印函数 | 依赖 A 完成基础 |
| **开发者 C** | 中断系统 (IDT/PIC/键盘) | 依赖 A 完成基础 |

---

## 详细任务清单

### 任务 MVP-01: 项目结构与构建系统
**负责人**: 开发者 A
**依赖**: 无
**产出**: 可编译的空项目

#### 目录结构
```
myos/
├── Makefile
├── linker.ld
├── grub.cfg
├── kernel/
│   ├── boot.S
│   ├── main.c
│   ├── serial.c
│   ├── serial.h
│   ├── idt.c
│   ├── idt.h
│   ├── pic.c
│   ├── pic.h
│   ├── keyboard.c
│   └── keyboard.h
├── include/
│   └── types.h
└── scripts/
    └── run.sh
```

#### Makefile 要求
```makefile
# 交叉编译器 (或使用系统 gcc 带 -m64)
CC = x86_64-elf-gcc
AS = x86_64-elf-as
LD = x86_64-elf-ld

# 编译选项
CFLAGS = -ffreestanding -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -fno-stack-protector -Wall -Wextra -O2
LDFLAGS = -T linker.ld -nostdlib

# 目标
all: myos.iso

kernel.bin: boot.o main.o serial.o idt.o pic.o keyboard.o
	$(LD) $(LDFLAGS) -o $@ $^

myos.iso: kernel.bin grub.cfg
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/
	cp grub.cfg iso/boot/grub/
	grub-mkrescue -o $@ iso

run: myos.iso
	./scripts/run.sh

clean:
	rm -rf *.o *.bin *.iso iso/
```

#### 验证方式
```bash
make clean && make
# 成功生成 myos.iso
```

---

### 任务 MVP-02: 链接脚本
**负责人**: 开发者 A
**依赖**: MVP-01
**产出**: linker.ld

#### linker.ld 内容
```ld
ENTRY(_start)

SECTIONS
{
    . = 1M;  /* 内核加载到 1MB 处 */

    .boot :
    {
        *(.multiboot)
    }

    .text :
    {
        *(.text)
    }

    .rodata :
    {
        *(.rodata)
    }

    .data :
    {
        *(.data)
    }

    .bss :
    {
        *(COMMON)
        *(.bss)
    }
}
```

#### 验证方式
```bash
# 链接成功，kernel.bin 入口点在 1MB
objdump -f kernel.bin | grep "start address"
# 输出: start address 0x0000000000100000
```

---

### 任务 MVP-03: GRUB 配置
**负责人**: 开发者 A
**依赖**: MVP-01
**产出**: grub.cfg

#### grub.cfg 内容
```
set timeout=0
set default=0

menuentry "MyOS" {
    multiboot2 /boot/kernel.bin
    boot
}
```

#### 验证方式
```bash
# ISO 能被 QEMU 启动，GRUB 菜单出现
qemu-system-x86_64 -cdrom myos.iso
```

---

### 任务 MVP-04: 引导汇编代码
**负责人**: 开发者 A
**依赖**: MVP-02
**产出**: kernel/boot.S

#### boot.S 要求

```asm
# Multiboot2 头部
.section .multiboot
.align 8
multiboot_header:
    .long 0xE85250D6                # magic
    .long 0                         # architecture (i386)
    .long multiboot_header_end - multiboot_header
    .long -(0xE85250D6 + 0 + (multiboot_header_end - multiboot_header))
    # 结束标签
    .word 0
    .word 0
    .long 8
multiboot_header_end:

# 64位 GDT
.section .data
.align 16
gdt64:
    .quad 0                         # null descriptor
    .quad 0x00AF9A000000FFFF        # code segment
    .quad 0x00AF92000000FFFF        # data segment
gdt64_pointer:
    .word . - gdt64 - 1
    .quad gdt64

# 启动代码
.section .text
.code32
.global _start
_start:
    # 禁用中断
    cli

    # 设置栈
    mov $stack_top, %esp

    # 检查 CPUID 和长模式支持 (简化版，假设支持)

    # 设置页表 (identity mapping 前 2MB)
    # ... (需要设置 PML4, PDPT, PD)

    # 启用 PAE
    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4

    # 加载页表
    mov $pml4, %eax
    mov %eax, %cr3

    # 启用长模式
    mov $0xC0000080, %ecx
    rdmsr
    or $0x100, %eax
    wrmsr

    # 启用分页
    mov %cr0, %eax
    or $0x80000001, %eax
    mov %eax, %cr0

    # 加载 64 位 GDT
    lgdt gdt64_pointer

    # 跳转到 64 位代码
    ljmp $0x08, $long_mode_start

.code64
long_mode_start:
    # 设置段寄存器
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    # 调用 C 入口
    call kernel_main

    # 永远不应该返回
    hlt
    jmp .

# 页表 (静态分配，identity map 前 2MB)
.section .bss
.align 4096
pml4:
    .skip 4096
pdpt:
    .skip 4096
pd:
    .skip 4096

.align 16
stack_bottom:
    .skip 16384  # 16KB 栈
stack_top:
```

#### 验证方式
```bash
# QEMU 启动后不崩溃，能跳转到 kernel_main
# 在 kernel_main 中加一个无限循环，CPU 使用率应该是 100%
```

---

### 任务 MVP-05: 基础类型定义
**负责人**: 开发者 A
**依赖**: MVP-01
**产出**: include/types.h

#### types.h 内容
```c
#ifndef _TYPES_H
#define _TYPES_H

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

typedef uint64_t size_t;
typedef int64_t  ssize_t;

#define NULL ((void*)0)
#define true  1
#define false 0
typedef int bool;

#endif
```

---

### 任务 MVP-06: 串口驱动
**负责人**: 开发者 B
**依赖**: MVP-04, MVP-05
**产出**: kernel/serial.c, kernel/serial.h

#### serial.h 内容
```c
#ifndef _SERIAL_H
#define _SERIAL_H

#include "types.h"

#define SERIAL_COM1 0x3F8

void serial_init(void);
void serial_putchar(char c);
void serial_print(const char *str);
void serial_print_hex(uint64_t value);

#endif
```

#### serial.c 要求
```c
#include "serial.h"

// 端口 I/O 内联函数
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void serial_init(void) {
    // 禁用中断
    outb(SERIAL_COM1 + 1, 0x00);
    // 设置波特率 115200
    outb(SERIAL_COM1 + 3, 0x80);    // Enable DLAB
    outb(SERIAL_COM1 + 0, 0x01);    // divisor low byte (115200)
    outb(SERIAL_COM1 + 1, 0x00);    // divisor high byte
    outb(SERIAL_COM1 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(SERIAL_COM1 + 2, 0xC7);    // Enable FIFO
    outb(SERIAL_COM1 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

static int serial_is_transmit_empty(void) {
    return inb(SERIAL_COM1 + 5) & 0x20;
}

void serial_putchar(char c) {
    while (!serial_is_transmit_empty());
    outb(SERIAL_COM1, c);
}

void serial_print(const char *str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(*str++);
    }
}

void serial_print_hex(uint64_t value) {
    serial_print("0x");
    char hex[] = "0123456789ABCDEF";
    for (int i = 60; i >= 0; i -= 4) {
        serial_putchar(hex[(value >> i) & 0xF]);
    }
}
```

#### 验证方式
```bash
# QEMU 串口输出可见
qemu-system-x86_64 -cdrom myos.iso -serial stdio -nographic
# 预期输出: Hello from MyOS!
```

---

### 任务 MVP-07: IDT (中断描述符表)
**负责人**: 开发者 C
**依赖**: MVP-04, MVP-05
**产出**: kernel/idt.c, kernel/idt.h

#### idt.h 内容
```c
#ifndef _IDT_H
#define _IDT_H

#include "types.h"

// IDT 条目结构 (64位模式)
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

void idt_init(void);
void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags);

#endif
```

#### idt.c 要求
```c
#include "idt.h"
#include "serial.h"

#define IDT_ENTRIES 256

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

// 外部中断处理程序 (在汇编中定义)
extern void isr_stub_0(void);   // 除零
extern void isr_stub_33(void);  // 键盘 (IRQ1)
// ... 其他中断

void idt_set_gate(uint8_t num, uint64_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low  = handler & 0xFFFF;
    idt[num].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[num].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[num].selector    = selector;
    idt[num].ist         = 0;
    idt[num].type_attr   = flags;
    idt[num].zero        = 0;
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint64_t)&idt;

    // 清空 IDT
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    // 设置异常处理 (0-31)
    idt_set_gate(0, (uint64_t)isr_stub_0, 0x08, 0x8E);  // 除零异常

    // 设置 IRQ 处理 (32-47)
    idt_set_gate(33, (uint64_t)isr_stub_33, 0x08, 0x8E); // 键盘

    // 加载 IDT
    __asm__ volatile ("lidt %0" : : "m"(idtp));

    serial_print("[IDT] Initialized\n");
}
```

#### 中断处理汇编 (添加到 boot.S 或单独文件)
```asm
.extern keyboard_handler
.extern exception_handler

.macro ISR_STUB num
.global isr_stub_\num
isr_stub_\num:
    push %rax
    push %rbx
    push %rcx
    push %rdx
    push %rsi
    push %rdi
    push %rbp
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    mov $\num, %rdi
    .if \num < 32
        call exception_handler
    .else
        call irq_handler
    .endif

    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rbp
    pop %rdi
    pop %rsi
    pop %rdx
    pop %rcx
    pop %rbx
    pop %rax
    iretq
.endm

ISR_STUB 0
ISR_STUB 33
```

#### 验证方式
```bash
# 触发除零异常，串口输出异常信息
# int x = 1/0; 应该输出 "Exception: Division by zero"
```

---

### 任务 MVP-08: PIC (可编程中断控制器)
**负责人**: 开发者 C
**依赖**: MVP-07
**产出**: kernel/pic.c, kernel/pic.h

#### pic.h 内容
```c
#ifndef _PIC_H
#define _PIC_H

#include "types.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

#define PIC_EOI   0x20

void pic_init(void);
void pic_send_eoi(uint8_t irq);
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);

#endif
```

#### pic.c 要求
```c
#include "pic.h"
#include "serial.h"

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

void pic_init(void) {
    // ICW1: 开始初始化
    outb(PIC1_CMD, 0x11);
    io_wait();
    outb(PIC2_CMD, 0x11);
    io_wait();

    // ICW2: 中断向量偏移 (IRQ0 -> 32, IRQ8 -> 40)
    outb(PIC1_DATA, 0x20);  // IRQ 0-7  -> INT 32-39
    io_wait();
    outb(PIC2_DATA, 0x28);  // IRQ 8-15 -> INT 40-47
    io_wait();

    // ICW3: 级联设置
    outb(PIC1_DATA, 0x04);  // IRQ2 连接从片
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    // ICW4: 8086 模式
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    // 屏蔽所有中断 (之后按需开启)
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    serial_print("[PIC] Initialized\n");
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
    }
    outb(PIC1_CMD, PIC_EOI);
}

void pic_clear_mask(uint8_t irq) {
    uint16_t port;
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
    }
    uint8_t mask = inb(port) & ~(1 << irq);
    outb(port, mask);
}
```

#### 验证方式
```bash
# PIC 初始化后，IRQ 不会导致系统崩溃
```

---

### 任务 MVP-09: 键盘驱动
**负责人**: 开发者 C
**依赖**: MVP-08
**产出**: kernel/keyboard.c, kernel/keyboard.h

#### keyboard.h 内容
```c
#ifndef _KEYBOARD_H
#define _KEYBOARD_H

void keyboard_init(void);
void keyboard_handler(void);

#endif
```

#### keyboard.c 要求
```c
#include "keyboard.h"
#include "pic.h"
#include "serial.h"
#include "types.h"

#define KEYBOARD_DATA_PORT 0x60

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// 简化的扫描码到 ASCII 映射 (US 键盘, 小写)
static const char scancode_to_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

void keyboard_init(void) {
    // 启用键盘中断 (IRQ1)
    pic_clear_mask(1);
    serial_print("[Keyboard] Initialized\n");
}

void keyboard_handler(void) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // 只处理按下事件 (忽略释放，高位为1表示释放)
    if (scancode & 0x80) {
        pic_send_eoi(1);
        return;
    }

    if (scancode < sizeof(scancode_to_ascii)) {
        char c = scancode_to_ascii[scancode];
        if (c) {
            serial_putchar(c);
        }
    }

    pic_send_eoi(1);
}
```

#### 验证方式
```bash
# 在 QEMU 中按键，串口回显字符
qemu-system-x86_64 -cdrom myos.iso -serial stdio
# 按 'a' -> 串口输出 'a'
```

---

### 任务 MVP-10: 内核主函数
**负责人**: 开发者 A (整合)
**依赖**: MVP-06, MVP-07, MVP-08, MVP-09
**产出**: kernel/main.c

#### main.c 内容
```c
#include "serial.h"
#include "idt.h"
#include "pic.h"
#include "keyboard.h"

void exception_handler(int num) {
    serial_print("[Exception] ");
    serial_print_hex(num);
    serial_print("\n");

    // 停机
    while (1) {
        __asm__ volatile ("hlt");
    }
}

void irq_handler(int num) {
    if (num == 33) {  // IRQ1 = 键盘
        keyboard_handler();
    }
}

void kernel_main(void) {
    // 初始化串口
    serial_init();
    serial_print("\n=============================\n");
    serial_print("  Hello from MyOS!\n");
    serial_print("  64-bit kernel running\n");
    serial_print("=============================\n\n");

    // 初始化中断
    pic_init();
    idt_init();

    // 初始化键盘
    keyboard_init();

    // 启用中断
    __asm__ volatile ("sti");

    serial_print("[Kernel] Ready. Type something:\n");

    // 主循环
    while (1) {
        __asm__ volatile ("hlt");  // 等待中断
    }
}
```

---

### 任务 MVP-11: QEMU 运行脚本
**负责人**: 开发者 A
**依赖**: MVP-01
**产出**: scripts/run.sh

#### run.sh 内容
```bash
#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ISO="$PROJECT_DIR/myos.iso"

if [ ! -f "$ISO" ]; then
    echo "Error: $ISO not found. Run 'make' first."
    exit 1
fi

# 默认模式: 串口输出到终端
qemu-system-x86_64 \
    -cdrom "$ISO" \
    -serial stdio \
    -m 128M \
    -no-reboot \
    -no-shutdown \
    "$@"
```

#### 调试脚本 scripts/debug.sh
```bash
#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ISO="$PROJECT_DIR/myos.iso"

# 启动 QEMU 并等待 GDB 连接
qemu-system-x86_64 \
    -cdrom "$ISO" \
    -serial stdio \
    -m 128M \
    -s -S \
    -no-reboot \
    "$@"

# 另一个终端运行:
# gdb -ex "target remote :1234" -ex "symbol-file kernel.bin"
```

---

## 开发时间线 (任务依赖图)

```
MVP-01 (构建系统)
   │
   ├──> MVP-02 (链接脚本)
   │       │
   ├──> MVP-03 (GRUB配置)
   │       │
   └──> MVP-05 (类型定义)
           │
           ▼
       MVP-04 (引导汇编) ◄─────────────────┐
           │                               │
           ├───────────────┬───────────────┤
           │               │               │
           ▼               ▼               ▼
       MVP-06          MVP-07          MVP-11
       (串口)          (IDT)           (脚本)
           │               │
           │               ▼
           │           MVP-08
           │           (PIC)
           │               │
           │               ▼
           │           MVP-09
           │           (键盘)
           │               │
           └───────┬───────┘
                   │
                   ▼
               MVP-10 (整合)
                   │
                   ▼
              ✅ MVP 完成
```

---

## 验收标准

### 必须通过的测试

1. **构建测试**
   ```bash
   make clean && make
   # 成功生成 myos.iso，无编译错误/警告
   ```

2. **启动测试**
   ```bash
   ./scripts/run.sh | head -10
   # 输出包含 "Hello from MyOS!"
   ```

3. **键盘测试**
   ```bash
   # 手动测试: 运行后按键，串口回显字符
   ./scripts/run.sh
   # 按 'hello' -> 串口显示 'hello'
   ```

4. **稳定性测试**
   ```bash
   # 运行 60 秒不崩溃
   timeout 60 ./scripts/run.sh
   ```

### 自动化验证脚本

```bash
#!/bin/bash
# scripts/test.sh

echo "=== MVP Verification ==="

# 测试 1: 构建
echo -n "Build test... "
make clean > /dev/null 2>&1
if make > /dev/null 2>&1; then
    echo "PASS"
else
    echo "FAIL"
    exit 1
fi

# 测试 2: 启动输出
echo -n "Boot test... "
OUTPUT=$(timeout 5 qemu-system-x86_64 \
    -cdrom myos.iso \
    -serial stdio \
    -nographic \
    -no-reboot 2>/dev/null)

if echo "$OUTPUT" | grep -q "Hello from MyOS"; then
    echo "PASS"
else
    echo "FAIL"
    exit 1
fi

# 测试 3: 子系统初始化
echo -n "Subsystem init test... "
if echo "$OUTPUT" | grep -q "\[PIC\] Initialized" && \
   echo "$OUTPUT" | grep -q "\[IDT\] Initialized" && \
   echo "$OUTPUT" | grep -q "\[Keyboard\] Initialized"; then
    echo "PASS"
else
    echo "FAIL"
    exit 1
fi

echo "=== All tests PASSED ==="
```

---

## 常见问题排查

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| GRUB 找不到内核 | Multiboot 头部错误 | 检查 magic number 和校验和 |
| Triple fault | GDT/IDT 设置错误 | 使用 QEMU -d int 查看中断 |
| 串口无输出 | 串口未初始化/波特率错误 | 检查 serial_init() |
| 键盘无响应 | PIC 未正确配置 | 检查 IRQ mask |
| 页错误 | 页表设置不正确 | 检查 identity mapping |

### QEMU 调试命令

```bash
# 查看中断
qemu-system-x86_64 -cdrom myos.iso -d int -no-reboot

# 查看寄存器
qemu-system-x86_64 -cdrom myos.iso -d cpu -no-reboot

# GDB 调试
qemu-system-x86_64 -cdrom myos.iso -s -S &
gdb -ex "target remote :1234" -ex "symbol-file kernel.bin"
```

---

## 交付物清单

| 文件 | 负责人 | 状态 |
|------|--------|------|
| Makefile | A | ✅ |
| linker.ld | A | ✅ |
| grub.cfg | A | ✅ |
| kernel/boot.S | A | ✅ |
| include/types.h | A | ✅ |
| kernel/serial.c/h | B | ✅ |
| kernel/idt.c/h | C | ✅ |
| kernel/pic.c/h | C | ✅ |
| kernel/keyboard.c/h | C | ✅ |
| kernel/main.c | A | ✅ |
| scripts/run.sh | A | ✅ |
| scripts/debug.sh | A | ✅ |
| scripts/test.sh | A | ⬜ |

---

## MVP 完成状态

**✅ MVP 已完成并通过测试！**

### 测试结果 (2026-01-08)

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

### 运行方式

```bash
cd /workspace/myos
make clean && make
./scripts/run.sh
```

按 `Ctrl+A` 然后 `X` 退出 QEMU。
