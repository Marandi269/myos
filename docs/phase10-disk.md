# Phase 10: 磁盘启动支持 (Disk Boot)

## 目标
支持从磁盘镜像启动，实现持久化存储。

## 前置依赖
- ✅ Phase 5: 文件系统 (VFS, ramfs)
- 📋 建议: Phase 8 网络栈 (可选，virtio 驱动经验复用)

---

## 当前状态 vs 目标

| 项目 | 当前 (ISO) | 目标 (磁盘) |
|------|-----------|-------------|
| 启动方式 | GRUB + ISO | GRUB + 磁盘分区 |
| 根文件系统 | ramfs (内存) | ext2 (磁盘) |
| 存储持久化 | ❌ 不支持 | ✅ 支持 |
| initramfs | 嵌入内核 | 单独文件或分区 |

---

## 任务列表

### D-01: 磁盘镜像创建脚本
**优先级**: P0
**依赖**: 无

**描述**: 创建可启动的磁盘镜像。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| D-01.1 | 创建空磁盘镜像 | dd 创建 64MB 镜像 |
| D-01.2 | 分区表创建 | MBR 分区表 |
| D-01.3 | 文件系统格式化 | ext2 格式化 |
| D-01.4 | GRUB 安装 | grub-install |
| D-01.5 | 内核和配置复制 | kernel.bin + grub.cfg |

**脚本**: `scripts/mkdisk.sh`
```bash
#!/bin/bash
set -e

DISK_IMG="myos.img"
DISK_SIZE=64  # MB
MOUNT_DIR="/tmp/myos_mount"

echo "[1/6] Creating disk image..."
dd if=/dev/zero of=$DISK_IMG bs=1M count=$DISK_SIZE 2>/dev/null

echo "[2/6] Creating partition table..."
parted -s $DISK_IMG mklabel msdos
parted -s $DISK_IMG mkpart primary ext2 1MiB 100%
parted -s $DISK_IMG set 1 boot on

echo "[3/6] Setting up loop device..."
LOOP_DEV=$(sudo losetup -f --show -P $DISK_IMG)
PART_DEV="${LOOP_DEV}p1"

echo "[4/6] Formatting partition..."
sudo mkfs.ext2 -q $PART_DEV

echo "[5/6] Installing GRUB and kernel..."
sudo mkdir -p $MOUNT_DIR
sudo mount $PART_DEV $MOUNT_DIR

sudo grub-install --target=i386-pc \
    --boot-directory=$MOUNT_DIR/boot \
    --modules="part_msdos ext2" \
    $LOOP_DEV 2>/dev/null

sudo mkdir -p $MOUNT_DIR/boot/grub
sudo cp kernel.bin $MOUNT_DIR/boot/
sudo cp grub.cfg $MOUNT_DIR/boot/grub/

# 复制 initramfs (如果存在)
if [ -f initramfs.cpio ]; then
    sudo cp initramfs.cpio $MOUNT_DIR/boot/
fi

echo "[6/6] Cleaning up..."
sudo umount $MOUNT_DIR
sudo losetup -d $LOOP_DEV
rmdir $MOUNT_DIR 2>/dev/null || true

echo "Created $DISK_IMG (${DISK_SIZE}MB)"
ls -lh $DISK_IMG
```

**验证**:
```
$ ./scripts/mkdisk.sh
Created myos.img (64MB)
-rw-r--r-- 1 user user 64M myos.img
```

---

### D-02: Makefile 更新
**优先级**: P0
**依赖**: D-01

**描述**: 添加磁盘相关的构建目标。

**新增目标**:
```makefile
# 创建磁盘镜像
disk: kernel.bin
	./scripts/mkdisk.sh

# 从磁盘启动 (IDE)
run-disk: disk
	qemu-system-x86_64 \
		-drive file=myos.img,format=raw,if=ide \
		-nographic \
		-no-reboot

# 从磁盘启动 (virtio，性能更好)
run-disk-virtio: disk
	qemu-system-x86_64 \
		-drive file=myos.img,format=raw,if=virtio \
		-nographic \
		-no-reboot

# 调试模式
debug-disk: disk
	qemu-system-x86_64 \
		-drive file=myos.img,format=raw,if=ide \
		-nographic \
		-no-reboot \
		-s -S

# 清理
clean:
	... # 现有内容
	rm -f myos.img
```

**验证**:
```bash
make disk
make run-disk
```

---

### D-03: IDE/ATA 磁盘驱动
**优先级**: P1
**依赖**: I-02 (PIC)

**描述**: 实现 IDE/ATA 磁盘驱动，支持读写扇区。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| D-03.1 | IDE 控制器检测 | 检测主/从盘 |
| D-03.2 | PIO 读取 | 读取扇区数据 |
| D-03.3 | PIO 写入 | 写入扇区数据 |
| D-03.4 | 块设备抽象 | block_device 接口 |
| D-03.5 | 缓存层 | 简单的块缓存 |

**文件**:
- `kernel/drivers/ide.c` - IDE 驱动
- `kernel/drivers/ide.h` - IDE 接口
- `kernel/drivers/block.c` - 块设备抽象
- `kernel/drivers/block.h` - 块设备接口

**IDE I/O 端口**:
| 端口 | 功能 |
|------|------|
| 0x1F0 | 数据寄存器 |
| 0x1F1 | 错误寄存器 |
| 0x1F2 | 扇区计数 |
| 0x1F3-0x1F5 | LBA 地址 |
| 0x1F6 | 驱动器选择 |
| 0x1F7 | 状态/命令 |

**块设备接口**:
```c
typedef struct block_device {
    char name[16];
    uint64_t size;       /* 总字节数 */
    uint32_t block_size; /* 块大小 (通常 512) */

    int (*read)(struct block_device *dev, uint64_t lba,
                void *buf, size_t count);
    int (*write)(struct block_device *dev, uint64_t lba,
                 const void *buf, size_t count);
    void *priv;
} block_device_t;
```

**验证**:
```
[IDE] Primary master: 64 MB
[IDE] Read sector 0: OK
[IDE] Write sector 100: OK
```

---

### D-04: virtio-blk 驱动 (可选)
**优先级**: P2
**依赖**: PCI 枚举 (Phase 8 N-01)

**描述**: 实现 virtio-blk 驱动，性能优于 IDE。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| D-04.1 | virtio 设备检测 | 通过 PCI 枚举 |
| D-04.2 | virtqueue 初始化 | 分配 vring |
| D-04.3 | 块读取 | 读取扇区 |
| D-04.4 | 块写入 | 写入扇区 |

**文件**:
- `kernel/drivers/virtio_blk.c` - virtio-blk 驱动

**验证**:
```
[PCI] Found device: 1af4:1001 (virtio-blk)
[virtio-blk] Capacity: 64 MB
```

---

### D-05: ext2 文件系统 (只读)
**优先级**: P1
**依赖**: D-03 (IDE 驱动)

**描述**: 实现 ext2 文件系统只读支持。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| D-05.1 | 超级块解析 | 读取 ext2 参数 |
| D-05.2 | 块组描述符 | 解析块组表 |
| D-05.3 | inode 读取 | 读取 inode 结构 |
| D-05.4 | 目录遍历 | 列出目录内容 |
| D-05.5 | 文件读取 | 读取文件数据 |
| D-05.6 | 挂载支持 | mount ext2 / |

**文件**:
- `kernel/fs/ext2/ext2.c` - ext2 实现
- `kernel/fs/ext2/ext2.h` - ext2 结构定义

**ext2 关键结构**:
```c
/* 超级块 (位于 1024 字节偏移) */
struct ext2_superblock {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;   /* 块大小 = 1024 << s_log_block_size */
    /* ... */
    uint16_t s_magic;            /* 0xEF53 */
    /* ... */
};

/* inode 结构 */
struct ext2_inode {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    /* ... */
    uint32_t i_block[15];        /* 数据块指针 */
};
```

**验证**:
```
[ext2] Mounted /dev/hda1 on /mnt
[ext2] Block size: 1024, Inode count: 16384
$ ls /mnt
bin  boot  etc  home
$ cat /mnt/etc/motd
Welcome to MyOS!
```

---

### D-06: ext2 写入支持
**优先级**: P2
**依赖**: D-05

**描述**: 实现 ext2 文件系统写入支持。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| D-06.1 | 块分配 | 分配新数据块 |
| D-06.2 | inode 分配 | 分配新 inode |
| D-06.3 | 文件写入 | 写入文件内容 |
| D-06.4 | 文件创建 | 创建新文件 |
| D-06.5 | 目录创建 | mkdir 支持 |
| D-06.6 | 文件删除 | unlink 支持 |
| D-06.7 | 同步机制 | sync 刷新缓存 |

**验证**:
```
$ echo "Hello" > /mnt/test.txt
$ cat /mnt/test.txt
Hello
$ sync
$ reboot
# 重启后数据仍在
$ cat /mnt/test.txt
Hello
```

---

### D-07: 根文件系统切换
**优先级**: P1
**依赖**: D-05

**描述**: 支持从 ramfs 切换到磁盘根文件系统。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| D-07.1 | pivot_root 实现 | 切换根目录 |
| D-07.2 | initramfs 解压到 ramfs | 早期启动 |
| D-07.3 | 磁盘探测和挂载 | 找到根分区 |
| D-07.4 | 切换到磁盘根 | init 在磁盘上运行 |

**启动流程**:
```
1. GRUB 加载 kernel.bin 和 initramfs.cpio
2. 内核解压 initramfs 到 ramfs
3. 运行 /init (initramfs 中的)
4. /init 探测磁盘，挂载 ext2 根分区
5. pivot_root 切换到磁盘根
6. exec /sbin/init (磁盘上的)
```

---

## 开发顺序

```
D-01 (磁盘镜像脚本) ──> D-02 (Makefile)
                              │
                              v
                       D-03 (IDE 驱动)
                              │
              ┌───────────────┼───────────────┐
              v               v               v
       D-04 (virtio)   D-05 (ext2 只读)  D-07 (根切换)
                              │
                              v
                       D-06 (ext2 写入)
```

**建议顺序**:
1. **D-01 + D-02**: 脚本和 Makefile (可立即验证磁盘启动)
2. **D-03**: IDE 驱动 (硬件交互)
3. **D-05**: ext2 只读 (读取磁盘文件)
4. **D-07**: 根文件系统切换
5. **D-06**: ext2 写入 (持久化)
6. **D-04**: virtio-blk (可选，性能优化)

---

## 验证里程碑

| 编号 | 里程碑 | 验证方式 |
|------|--------|----------|
| M10.1 | 磁盘镜像启动 | `make run-disk` 成功启动 |
| M10.2 | IDE 驱动工作 | 读取磁盘扇区 |
| M10.3 | ext2 只读 | 读取磁盘上的文件 |
| M10.4 | 根切换 | 从磁盘运行 shell |
| M10.5 | ext2 写入 | 文件持久化保存 |

---

## QEMU 启动命令对比

```bash
# 当前 (ISO 启动)
qemu-system-x86_64 -cdrom myos.iso -nographic

# 磁盘 (IDE，兼容性好)
qemu-system-x86_64 \
    -drive file=myos.img,format=raw,if=ide \
    -nographic

# 磁盘 (virtio，性能好)
qemu-system-x86_64 \
    -drive file=myos.img,format=raw,if=virtio \
    -nographic

# 多磁盘
qemu-system-x86_64 \
    -drive file=myos.img,format=raw,if=ide,index=0 \
    -drive file=data.img,format=raw,if=ide,index=1 \
    -nographic
```

---

## 磁盘分区方案

### 方案 A: 单分区 (简单)
```
+------------------------+
| Partition 1: / (ext2)  |
| - /boot/kernel.bin     |
| - /boot/grub/*         |
| - /bin, /etc, ...      |
+------------------------+
```

### 方案 B: 多分区 (推荐)
```
+------------------------+
| Partition 1: /boot     | 16MB, ext2
| - kernel.bin           |
| - grub/*               |
| - initramfs.cpio       |
+------------------------+
| Partition 2: /         | 剩余空间, ext2
| - /bin, /etc, /home    |
+------------------------+
```

---

## 交付物清单

- [ ] `scripts/mkdisk.sh` - 磁盘镜像创建脚本
- [ ] `Makefile` 更新 - disk/run-disk 目标
- [ ] `kernel/drivers/ide.c/h` - IDE 驱动
- [ ] `kernel/drivers/block.c/h` - 块设备抽象
- [ ] `kernel/drivers/virtio_blk.c/h` - virtio-blk 驱动 (可选)
- [ ] `kernel/fs/ext2/ext2.c/h` - ext2 文件系统
- [ ] `kernel/proc/pivot_root.c` - 根切换
- [ ] 更新 grub.cfg - 磁盘启动配置

---

## 技术注意事项

### IDE 驱动要点
1. 使用 PIO 模式 (简单，不需要 DMA)
2. 28-bit LBA 足够支持 128GB
3. 需要正确处理忙等待
4. 错误处理要完善

### ext2 要点
1. 超级块在 1024 字节偏移
2. 块组描述符紧跟超级块
3. 间接块指针处理 (大文件)
4. 写入需要更新 bitmap 和超级块

### 缓存策略
1. 实现简单的块缓存
2. 定期 sync 到磁盘
3. 写入时更新缓存
