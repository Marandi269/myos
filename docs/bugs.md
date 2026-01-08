# 已知问题 (Issues)

## 当前 Bug

### 1. Heap 内存释放错误
**严重程度**: 🟡 中等
**发现时间**: 2026-01-08
**位置**: `kernel/mm/heap.c`

```
[Heap] ERROR: Invalid free (bad magic at 0x121538)
```

**描述**: 文件系统测试过程中出现无效的内存释放操作，magic number 校验失败。

**可能原因**:
- 双重释放 (double free)
- 释放了未分配的内存
- 内存越界写入破坏了相邻块的 magic

**影响**: 当前不影响主要功能测试，但可能导致内存泄漏或后续崩溃。

**修复建议**:
1. 在 kfree 中添加更详细的调试信息
2. 检查 VFS/ramfs 中的内存分配和释放配对
3. 考虑添加内存分配追踪机制

---

### 2. 编译警告: 函数类型转换
**严重程度**: 🟢 低
**位置**: `kernel/proc/syscall.c:80-89`

```
warning: cast between incompatible function types from 'int64_t (*)(int, char *, size_t)'
to 'int64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)'
```

**描述**: 系统调用注册时的函数指针类型转换警告。

**修复建议**:
- 统一系统调用函数签名为 `int64_t (uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)`
- 或使用 union 包装不同类型的函数指针

---

### 3. 编译警告: 未使用的函数
**严重程度**: 🟢 低
**位置**: `kernel/main.c:288`

```
warning: 'test_scheduler' defined but not used
```

**修复建议**: 删除该函数或在测试中启用。

---

### 4. 链接警告: 缺少 .note.GNU-stack
**严重程度**: 🟢 低
**位置**: `kernel/proc/syscall_asm.o`, `kernel/boot.o`

```
ld: warning: kernel/proc/syscall_asm.o: missing .note.GNU-stack section implies executable stack
```

**修复建议**: 在汇编文件末尾添加:
```asm
.section .note.GNU-stack,"",@progbits
```

---

## 已修复

*(暂无)*
