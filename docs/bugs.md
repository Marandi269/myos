# 已知问题 (Bugs)

## 当前 Bug

### 1. 编译警告: 函数类型转换
**严重程度**: 🟢 低
**位置**: `kernel/proc/syscall.c:80-89`

```
warning: cast between incompatible function types from 'int64_t (*)(int, char *, size_t)'
to 'int64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)'
```

**描述**: 系统调用注册时的函数指针类型转换警告。这是内核开发中的常见做法，不影响功能。

**修复建议** (可选):
- 统一系统调用函数签名为 `int64_t (uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)`
- 或使用 union 包装不同类型的函数指针

---

## 已修复

### 1. Heap 内存释放错误 ✓
**验证时间**: 2026-01-08
**修复时间**: 2026-01-08
**修复提交**: 44095e3

**原问题**:
```
[Heap] ERROR: Invalid free (bad magic at 0x121538)
```

**根本原因**: `split_block` 在分割大的 free 块时，会在分配的块后面创建新的 free 块 header。但如果那个位置已经有一个被分配的块（如 fd_table），就会覆盖那个块的 header，导致后续释放时 magic 校验失败。

**修复方式**:
1. `split_block`: 添加安全检查，如果新块位置已有 allocated 块则不分割
2. `merge_free_blocks`: 只合并物理上相邻的块

---

### 2. 链接警告: 缺少 .note.GNU-stack ✓
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
