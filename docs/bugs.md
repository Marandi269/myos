# 已知问题 (Issues)

## 当前 Bug

### 1. Heap 内存释放错误
**严重程度**: 🟡 中等
**位置**: `kernel/mm/heap.c`, `kernel/fs/fd.c`

```
[Heap] ERROR: Invalid free (bad magic at 0x121538)
```

**描述**: 文件系统测试过程中出现无效的内存释放操作。尝试修复引用计数问题但仍然存在。

**可能原因**:
- 双重释放 (double free)
- 释放了未分配的内存
- 内存越界写入破坏了相邻块的 magic

**影响**: 当前不影响主要功能测试通过。

---

### 2. 编译警告: 函数类型转换
**严重程度**: 🟢 低
**位置**: `kernel/proc/syscall.c:80-89`

```
warning: cast between incompatible function types from 'int64_t (*)(int, char *, size_t)'
to 'int64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)'
```

**描述**: 系统调用注册时的函数指针类型转换警告。这是内核开发中的常见做法。

**修复建议** (可选):
- 统一系统调用函数签名为 `int64_t (uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)`
- 或使用 union 包装不同类型的函数指针

---

## 已修复

### 1. 链接警告: 缺少 .note.GNU-stack ✓
**修复时间**: 2026-01-08
**修复提交**: 5a1e630

**原问题**:
```
ld: warning: kernel/proc/syscall_asm.o: missing .note.GNU-stack section implies executable stack
```

**修复方式**: 在所有汇编文件末尾添加:
```asm
.section .note.GNU-stack,"",@progbits
```

受影响文件: `boot.S`, `switch.S`, `gdt_asm.S`, `syscall_asm.S`

---

### 3. 编译警告: 未使用的函数 ✓
**修复时间**: 2026-01-08
**修复提交**: 5a1e630

**原问题**:
```
warning: 'test_scheduler' defined but not used
```

**修复方式**: 在 `kernel_main()` 中重新启用 `test_scheduler()` 调用
