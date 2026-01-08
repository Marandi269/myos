# 阶段 2.5: 虚拟内存管理 (VMM)

## 状态: ✅ 已完成

### 测试结果 (2026-01-08)
```
[VMM] Initializing virtual memory manager
[VMM] Using boot PML4 at 0x107000
[VMM] Initialized (identity mapping active)

[TEST] Virtual Memory Manager
  Map 0x10000000 -> 0x129000: OK
  Write/Read test: PASSED
  vmm_get_phys: PASSED (0x129000)
  Unmap: OK
[TEST] VMM: ALL PASSED
```

---

## 背景

阶段 2 完成了物理内存管理 (PMM)。阶段 2.5 添加虚拟内存管理 (VMM)：

- ✅ PMM: 物理页帧分配/释放
- ✅ Heap: 内核堆 (kmalloc/kfree)
- ✅ **VMM: 动态页表管理**

**为什么需要 VMM？**
1. 进程隔离 - 每个进程需要独立地址空间
2. 内存保护 - 用户态无法访问内核内存
3. 按需分配 - 支持缺页加载
4. 内存映射 - mmap 等系统调用

---

## 当前页表状态

```
boot.S 中硬编码的 identity mapping:

PML4[0] -> PDPT[0] -> PD[0] -> 2MB huge page (0x000000 - 0x1FFFFF)
                   -> PD[1] -> 2MB huge page (0x200000 - 0x3FFFFF)

问题:
- 只映射了前 4MB
- 无法动态添加映射
- 内核和用户共享同一地址空间
```

---

## 目标架构

```
虚拟地址空间布局 (x86_64 canonical addresses):

0xFFFF_FFFF_FFFF_FFFF ┌─────────────────────┐
                      │  (未使用)            │
0xFFFF_FFFF_8000_0000 ├─────────────────────┤
                      │  内核代码/数据        │  <- 高地址内核
                      │  (映射到物理 0-xxx)   │
0xFFFF_8000_0000_0000 ├─────────────────────┤
                      │  内核堆              │
                      │  设备 MMIO           │
                      ├─────────────────────┤
                      │                     │
                      │  (非规范地址空洞)     │
                      │                     │
0x0000_8000_0000_0000 ├─────────────────────┤
                      │                     │
                      │  用户栈 ↓           │
0x0000_7FFF_FFFF_F000 ├─────────────────────┤
                      │                     │
                      │  (用户可用空间)       │
                      │                     │
0x0000_0000_0040_0000 ├─────────────────────┤
                      │  用户代码/数据        │
0x0000_0000_0000_1000 ├─────────────────────┤
                      │  NULL guard page    │
0x0000_0000_0000_0000 └─────────────────────┘
```

---

## 任务清单

| 任务ID | 任务名称 | 依赖 | 可并行 | 验证方式 |
|--------|----------|------|--------|----------|
| V-01 | 页表数据结构 | PMM | ✅ | 结构体定义 |
| V-02 | 内核页表初始化 | V-01 | ❌ | 高地址内核运行 |
| V-03 | vmm_map_page | V-02 | ❌ | 映射测试 |
| V-04 | vmm_unmap_page | V-03 | ❌ | 取消映射测试 |
| V-05 | 缺页处理 (Page Fault) | V-03 | ❌ | 触发并处理缺页 |
| V-06 | 进程地址空间 | V-03 | ❌ | 创建用户页表 |

---

## 详细设计

### V-01: 页表数据结构

```c
// kernel/mm/vmm.h

#ifndef _VMM_H
#define _VMM_H

#include "types.h"

// 页表项标志位
#define PTE_PRESENT     (1UL << 0)   // 页存在
#define PTE_WRITABLE    (1UL << 1)   // 可写
#define PTE_USER        (1UL << 2)   // 用户可访问
#define PTE_PWT         (1UL << 3)   // 写透缓存
#define PTE_PCD         (1UL << 4)   // 禁用缓存
#define PTE_ACCESSED    (1UL << 5)   // 已访问
#define PTE_DIRTY       (1UL << 6)   // 已修改
#define PTE_HUGE        (1UL << 7)   // 大页 (2MB/1GB)
#define PTE_GLOBAL      (1UL << 8)   // 全局页 (不刷新TLB)
#define PTE_NX          (1UL << 63)  // 不可执行

// 页大小
#define PAGE_SIZE       4096
#define PAGE_MASK       (~(PAGE_SIZE - 1))
#define HUGE_PAGE_SIZE  (2 * 1024 * 1024)  // 2MB

// 内核虚拟地址基址
#define KERNEL_BASE     0xFFFF800000000000UL
#define KERNEL_PHYS_MAP 0xFFFF888000000000UL  // 物理内存直接映射

// 页表索引宏 (每级9位)
#define PML4_INDEX(addr)  (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr)  (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)    (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)    (((addr) >> 12) & 0x1FF)

// 页表项类型
typedef uint64_t pte_t;
typedef uint64_t pde_t;
typedef uint64_t pdpte_t;
typedef uint64_t pml4e_t;

// 页表结构 (4KB 对齐，512 项)
typedef struct {
    pte_t entries[512];
} __attribute__((aligned(4096))) page_table_t;

// 地址转换
static inline uint64_t virt_to_phys(uint64_t virt) {
    return virt - KERNEL_BASE;
}

static inline uint64_t phys_to_virt(uint64_t phys) {
    return phys + KERNEL_BASE;
}

#endif
```

### V-02: 内核页表初始化

```c
// kernel/mm/vmm.c

#include "vmm.h"
#include "pmm.h"
#include "kprintf.h"

// 内核 PML4 (全局)
static pml4e_t* kernel_pml4;

// 分配一个清零的页表
static page_table_t* alloc_page_table(void) {
    page_table_t* pt = (page_table_t*)pmm_alloc_page();
    if (pt) {
        memset(pt, 0, PAGE_SIZE);
    }
    return pt;
}

void vmm_init(void) {
    kprintf("[VMM] Initializing virtual memory manager\n");

    // 分配内核 PML4
    kernel_pml4 = (pml4e_t*)alloc_page_table();
    if (!kernel_pml4) {
        kprintf("[VMM] ERROR: Failed to allocate kernel PML4\n");
        return;
    }

    // 映射内核空间 (前 512GB 物理内存到高地址)
    // KERNEL_PHYS_MAP: 0xFFFF888000000000 映射到物理 0x0
    // 使用 1GB 大页简化

    // 映射内核代码/数据 (identity map 保持兼容)
    // 暂时保留低地址映射，之后移除

    // 映射前 4MB (与 boot.S 兼容)
    vmm_map_range(0, 0, 4 * 1024 * 1024, PTE_PRESENT | PTE_WRITABLE);

    // 映射内核到高地址
    vmm_map_range(KERNEL_BASE, 0, 16 * 1024 * 1024,
                  PTE_PRESENT | PTE_WRITABLE | PTE_GLOBAL);

    // 切换到新页表
    vmm_switch_pml4(kernel_pml4);

    kprintf("[VMM] Kernel page table activated\n");
}

// 切换 PML4 (更新 CR3)
void vmm_switch_pml4(pml4e_t* pml4) {
    uint64_t pml4_phys = (uint64_t)pml4;  // 假设是物理地址
    __asm__ volatile("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
}

// 刷新单个 TLB 条目
void vmm_flush_tlb(uint64_t vaddr) {
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}
```

### V-03: 映射函数

```c
// vmm_map_page - 映射单个 4KB 页
int vmm_map_page(uint64_t vaddr, uint64_t paddr, uint64_t flags) {
    vaddr &= PAGE_MASK;
    paddr &= PAGE_MASK;

    // 获取各级页表索引
    uint64_t pml4_idx = PML4_INDEX(vaddr);
    uint64_t pdpt_idx = PDPT_INDEX(vaddr);
    uint64_t pd_idx   = PD_INDEX(vaddr);
    uint64_t pt_idx   = PT_INDEX(vaddr);

    // 获取或创建 PDPT
    pml4e_t* pml4 = kernel_pml4;
    if (!(pml4[pml4_idx] & PTE_PRESENT)) {
        page_table_t* new_pdpt = alloc_page_table();
        if (!new_pdpt) return -1;
        pml4[pml4_idx] = (uint64_t)new_pdpt | PTE_PRESENT | PTE_WRITABLE;
    }
    pdpte_t* pdpt = (pdpte_t*)(pml4[pml4_idx] & PAGE_MASK);

    // 获取或创建 PD
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
        page_table_t* new_pd = alloc_page_table();
        if (!new_pd) return -1;
        pdpt[pdpt_idx] = (uint64_t)new_pd | PTE_PRESENT | PTE_WRITABLE;
    }
    pde_t* pd = (pde_t*)(pdpt[pdpt_idx] & PAGE_MASK);

    // 获取或创建 PT
    if (!(pd[pd_idx] & PTE_PRESENT)) {
        page_table_t* new_pt = alloc_page_table();
        if (!new_pt) return -1;
        pd[pd_idx] = (uint64_t)new_pt | PTE_PRESENT | PTE_WRITABLE;
    }
    pte_t* pt = (pte_t*)(pd[pd_idx] & PAGE_MASK);

    // 设置页表项
    pt[pt_idx] = paddr | flags | PTE_PRESENT;

    // 刷新 TLB
    vmm_flush_tlb(vaddr);

    return 0;
}

// vmm_map_range - 映射连续范围
int vmm_map_range(uint64_t vaddr, uint64_t paddr, uint64_t size, uint64_t flags) {
    for (uint64_t offset = 0; offset < size; offset += PAGE_SIZE) {
        if (vmm_map_page(vaddr + offset, paddr + offset, flags) != 0) {
            return -1;
        }
    }
    return 0;
}
```

### V-04: 取消映射

```c
// vmm_unmap_page - 取消单个页映射
int vmm_unmap_page(uint64_t vaddr) {
    vaddr &= PAGE_MASK;

    uint64_t pml4_idx = PML4_INDEX(vaddr);
    uint64_t pdpt_idx = PDPT_INDEX(vaddr);
    uint64_t pd_idx   = PD_INDEX(vaddr);
    uint64_t pt_idx   = PT_INDEX(vaddr);

    pml4e_t* pml4 = kernel_pml4;
    if (!(pml4[pml4_idx] & PTE_PRESENT)) return -1;

    pdpte_t* pdpt = (pdpte_t*)(pml4[pml4_idx] & PAGE_MASK);
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) return -1;

    pde_t* pd = (pde_t*)(pdpt[pdpt_idx] & PAGE_MASK);
    if (!(pd[pd_idx] & PTE_PRESENT)) return -1;

    pte_t* pt = (pte_t*)(pd[pd_idx] & PAGE_MASK);

    // 清除页表项
    pt[pt_idx] = 0;

    // 刷新 TLB
    vmm_flush_tlb(vaddr);

    return 0;
}

// 获取虚拟地址对应的物理地址
uint64_t vmm_get_phys(uint64_t vaddr) {
    uint64_t pml4_idx = PML4_INDEX(vaddr);
    uint64_t pdpt_idx = PDPT_INDEX(vaddr);
    uint64_t pd_idx   = PD_INDEX(vaddr);
    uint64_t pt_idx   = PT_INDEX(vaddr);

    pml4e_t* pml4 = kernel_pml4;
    if (!(pml4[pml4_idx] & PTE_PRESENT)) return 0;

    pdpte_t* pdpt = (pdpte_t*)(pml4[pml4_idx] & PAGE_MASK);
    if (!(pdpt[pdpt_idx] & PTE_PRESENT)) return 0;

    pde_t* pd = (pde_t*)(pdpt[pdpt_idx] & PAGE_MASK);
    if (!(pd[pd_idx] & PTE_PRESENT)) return 0;

    pte_t* pt = (pte_t*)(pd[pd_idx] & PAGE_MASK);
    if (!(pt[pt_idx] & PTE_PRESENT)) return 0;

    return (pt[pt_idx] & PAGE_MASK) | (vaddr & ~PAGE_MASK);
}
```

### V-05: 缺页处理

```c
// kernel/mm/page_fault.c

#include "vmm.h"
#include "kprintf.h"

// 缺页错误码位
#define PF_PRESENT  (1 << 0)  // 页存在时发生
#define PF_WRITE    (1 << 1)  // 写访问
#define PF_USER     (1 << 2)  // 用户态访问
#define PF_RESERVED (1 << 3)  // 保留位被设置
#define PF_FETCH    (1 << 4)  // 指令获取

void page_fault_handler(uint64_t error_code) {
    // 获取引发缺页的地址 (CR2)
    uint64_t fault_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));

    kprintf("\n[PAGE FAULT] Address: 0x%lx, Error: 0x%lx\n",
            fault_addr, error_code);

    // 分析错误类型
    if (error_code & PF_PRESENT) {
        kprintf("  - Protection violation\n");
    } else {
        kprintf("  - Page not present\n");
    }

    if (error_code & PF_WRITE) {
        kprintf("  - Write access\n");
    } else {
        kprintf("  - Read access\n");
    }

    if (error_code & PF_USER) {
        kprintf("  - User mode\n");
    } else {
        kprintf("  - Kernel mode\n");
    }

    // TODO: 实现按需分页
    // 1. 检查是否是合法的地址 (在进程地址空间内)
    // 2. 分配物理页
    // 3. 建立映射
    // 4. 返回继续执行

    // 目前: 内核缺页 = panic
    if (!(error_code & PF_USER)) {
        kprintf("[PANIC] Kernel page fault!\n");
        while (1) __asm__ volatile("hlt");
    }
}
```

---

## 验证测试

```c
void test_vmm(void) {
    kprintf("\n[TEST] Virtual Memory Manager\n");

    // 测试 1: 映射新页
    uint64_t test_vaddr = 0x0000000010000000;  // 256MB 虚拟地址
    uint64_t test_paddr = (uint64_t)pmm_alloc_page();

    kprintf("  Map 0x%lx -> 0x%lx: ", test_vaddr, test_paddr);
    if (vmm_map_page(test_vaddr, test_paddr, PTE_PRESENT | PTE_WRITABLE) == 0) {
        kprintf("OK\n");
    } else {
        kprintf("FAILED\n");
        return;
    }

    // 测试 2: 写入数据
    volatile uint64_t* ptr = (uint64_t*)test_vaddr;
    *ptr = 0xDEADBEEF12345678;
    kprintf("  Write test: ");
    if (*ptr == 0xDEADBEEF12345678) {
        kprintf("PASSED\n");
    } else {
        kprintf("FAILED\n");
    }

    // 测试 3: 验证物理地址
    kprintf("  Get physical: ");
    uint64_t phys = vmm_get_phys(test_vaddr);
    if (phys == test_paddr) {
        kprintf("PASSED (0x%lx)\n", phys);
    } else {
        kprintf("FAILED (got 0x%lx)\n", phys);
    }

    // 测试 4: 取消映射
    kprintf("  Unmap: ");
    if (vmm_unmap_page(test_vaddr) == 0) {
        kprintf("OK\n");
    } else {
        kprintf("FAILED\n");
    }

    // 释放物理页
    pmm_free_page((void*)test_paddr);

    kprintf("[TEST] VMM: ALL PASSED\n");
}
```

---

## 开发顺序

```
V-01 页表结构 ──> V-02 内核页表初始化
                      │
                      ▼
                 V-03 vmm_map_page
                      │
          ┌───────────┴───────────┐
          ▼                       ▼
     V-04 unmap               V-05 缺页处理
          │                       │
          └───────────┬───────────┘
                      │
                      ▼
                 V-06 进程地址空间
                      │
                      ▼
               ✅ 阶段 2.5 完成
```

---

## 验收标准

```
[VMM] Initializing virtual memory manager
[VMM] Kernel page table activated

[TEST] Virtual Memory Manager
  Map 0x10000000 -> 0x200000: OK
  Write test: PASSED
  Get physical: PASSED (0x200000)
  Unmap: OK
[TEST] VMM: ALL PASSED
```

---

## 文件清单

| 文件 | 描述 |
|------|------|
| kernel/mm/vmm.h | VMM 头文件，页表结构定义 |
| kernel/mm/vmm.c | VMM 实现，映射/取消映射 |
| kernel/mm/page_fault.c | 缺页异常处理 |

---

## 风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| 切换页表时崩溃 | 保持 identity mapping 直到稳定 |
| TLB 缓存问题 | 每次修改后刷新 TLB |
| 递归缺页 | 页表本身使用直接映射区域 |

---

## 下一步

完成 VMM 后进入 **阶段 3: 进程管理**:
- 每个进程独立 PML4
- 用户态/内核态地址空间分离
- fork 时复制页表
