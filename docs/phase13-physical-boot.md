# 阶段 13: 物理机启动支持 (USB/SD)

## 目标
使 MyOS 能够写入 USB/SD 卡，在物理机上启动运行。

---

## 当前状态分析

### 已支持
| 组件 | 状态 | 说明 |
|------|------|------|
| GRUB Multiboot2 | ✅ | 标准引导协议 |
| x86_64 长模式 | ✅ | 64位 CPU |
| VGA 文本模式 | ✅ | 基础显示 |
| PS/2 键盘 | ✅ | 基础输入 |
| IDE/ATA PIO | ✅ | 老式硬盘 |
| 串口输出 | ✅ | 调试用 |

### 需要补充
| 组件 | 优先级 | 说明 |
|------|--------|------|
| AHCI/SATA 驱动 | P0 | 现代硬盘/SSD |
| USB 存储驱动 | P0 | U盘启动后读写 |
| UEFI 启动支持 | P1 | 现代主板 |
| ACPI 基础 | P1 | 关机/重启 |

---

## 任务分解

### 13.1 AHCI/SATA 驱动 (现代硬盘支持)

| 任务ID | 任务名称 | 描述 | 验证方式 |
|--------|----------|------|----------|
| PB-01 | PCI 设备枚举 | 扫描 PCI 总线 | 列出所有设备 |
| PB-02 | AHCI 控制器检测 | 识别 SATA 控制器 | 打印控制器信息 |
| PB-03 | AHCI 端口初始化 | 配置 HBA 端口 | 检测连接的磁盘 |
| PB-04 | AHCI 读扇区 | SATA READ DMA | 读取磁盘数据 |
| PB-05 | AHCI 写扇区 | SATA WRITE DMA | 写入磁盘数据 |
| PB-06 | 集成到块设备层 | 注册为 /dev/sda | mount ext2 |

### 13.2 USB Mass Storage (U盘读写)

| 任务ID | 任务名称 | 描述 | 验证方式 |
|--------|----------|------|----------|
| PB-10 | USB MSC 类驱动 | Mass Storage Class | 检测 U 盘 |
| PB-11 | SCSI 命令封装 | READ/WRITE (10) | 扇区读写 |
| PB-12 | 集成到块设备层 | 注册为 /dev/sdb | mount ext2 |

### 13.3 启动介质制作

| 任务ID | 任务名称 | 描述 | 验证方式 |
|--------|----------|------|----------|
| PB-20 | 创建 USB 启动脚本 | dd 写入 ISO | U盘启动 |
| PB-21 | 创建 GRUB 安装脚本 | grub-install | 硬盘启动 |
| PB-22 | 支持 Hybrid ISO | isohybrid | USB/CD 通用 |

### 13.4 UEFI 启动支持 (可选)

| 任务ID | 任务名称 | 描述 | 验证方式 |
|--------|----------|------|----------|
| PB-30 | 创建 EFI 可执行文件 | PE32+ 格式 | UEFI 识别 |
| PB-31 | UEFI 引导加载器 | 使用 GRUB EFI | EFI 启动 |
| PB-32 | ESP 分区创建 | FAT32 /EFI/BOOT | 自动引导 |

### 13.5 ACPI 基础支持

| 任务ID | 任务名称 | 描述 | 验证方式 |
|--------|----------|------|----------|
| PB-40 | ACPI 表解析 | RSDP/RSDT/FADT | 打印 ACPI 信息 |
| PB-41 | 关机实现 | ACPI S5 状态 | poweroff 命令 |
| PB-42 | 重启实现 | 键盘控制器/ACPI | reboot 命令 |

---

## 详细实现

### 13.1 AHCI 驱动

#### PCI 枚举
```c
// kernel/drivers/pci.h
struct pci_device {
    uint8_t bus, slot, func;
    uint16_t vendor_id, device_id;
    uint8_t class_code, subclass;
    uint32_t bar[6];
};

// 扫描 PCI 总线
void pci_scan_bus(void);
struct pci_device* pci_find_device(uint8_t class, uint8_t subclass);
```

#### AHCI 控制器
```c
// kernel/drivers/ahci.h
#define AHCI_CLASS      0x01  // Mass Storage
#define AHCI_SUBCLASS   0x06  // SATA
#define AHCI_PROGIF     0x01  // AHCI

struct ahci_hba {
    volatile uint32_t cap;      // Host Capabilities
    volatile uint32_t ghc;      // Global Host Control
    volatile uint32_t is;       // Interrupt Status
    volatile uint32_t pi;       // Ports Implemented
    // ... 更多寄存器
};

struct ahci_port {
    volatile uint32_t clb;      // Command List Base
    volatile uint32_t clbu;     // Command List Base Upper
    volatile uint32_t fb;       // FIS Base
    volatile uint32_t fbu;      // FIS Base Upper
    volatile uint32_t is;       // Interrupt Status
    volatile uint32_t ie;       // Interrupt Enable
    volatile uint32_t cmd;      // Command
    // ...
};

int ahci_init(void);
int ahci_read(int port, uint64_t lba, uint32_t count, void *buf);
int ahci_write(int port, uint64_t lba, uint32_t count, void *buf);
```

### 13.2 USB Mass Storage

```c
// kernel/drivers/usb/usb_msc.h
#define USB_CLASS_MSC   0x08
#define USB_SUBCLASS_SCSI 0x06
#define USB_PROTO_BBB   0x50  // Bulk-Only Transport

struct usb_msc_device {
    struct usb_device *udev;
    uint8_t bulk_in_ep;
    uint8_t bulk_out_ep;
    uint32_t block_size;
    uint64_t block_count;
};

// SCSI 命令
int usb_msc_read(struct usb_msc_device *dev, uint64_t lba,
                 uint32_t count, void *buf);
int usb_msc_write(struct usb_msc_device *dev, uint64_t lba,
                  uint32_t count, void *buf);
```

### 13.3 启动介质制作脚本

#### scripts/make_usb.sh
```bash
#!/bin/bash
# 创建可启动 USB
# 用法: ./make_usb.sh /dev/sdX

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 <device>"
    echo "Example: $0 /dev/sdb"
    exit 1
fi

DEVICE=$1
ISO="myos.iso"

# 安全检查
if [ ! -b "$DEVICE" ]; then
    echo "Error: $DEVICE is not a block device"
    exit 1
fi

echo "WARNING: This will DESTROY all data on $DEVICE"
read -p "Continue? (yes/no): " confirm
if [ "$confirm" != "yes" ]; then
    echo "Aborted."
    exit 1
fi

# 写入 ISO
echo "Writing $ISO to $DEVICE..."
sudo dd if=$ISO of=$DEVICE bs=4M status=progress conv=fsync

echo "Done! You can now boot from $DEVICE"
```

#### scripts/make_hybrid_iso.sh
```bash
#!/bin/bash
# 创建 Hybrid ISO (可同时从 CD 和 USB 启动)

set -e

ISO="myos.iso"

# 检查 isohybrid
if ! command -v isohybrid &> /dev/null; then
    echo "Installing syslinux-utils..."
    sudo apt-get install -y syslinux-utils
fi

# 转换为 hybrid ISO
echo "Converting to hybrid ISO..."
isohybrid $ISO

echo "Done! $ISO can now boot from both CD and USB"
```

### 13.4 Makefile 更新

```makefile
# 新增目标
.PHONY: usb hybrid

# 创建可启动 USB
usb: iso
	@echo "Creating bootable USB..."
	@if [ -z "$(DEVICE)" ]; then \
		echo "Usage: make usb DEVICE=/dev/sdX"; \
		exit 1; \
	fi
	@./scripts/make_usb.sh $(DEVICE)

# 创建 hybrid ISO
hybrid: iso
	@./scripts/make_hybrid_iso.sh
```

---

## 启动测试清单

### BIOS/Legacy 启动
```
1. 制作启动 U 盘: make usb DEVICE=/dev/sdX
2. 进入 BIOS 设置 (F2/Del)
3. 设置 USB 为第一启动项
4. 保存并重启
5. 验证 MyOS 启动
```

### UEFI 启动 (可选)
```
1. 制作 UEFI 启动盘: make usb-efi DEVICE=/dev/sdX
2. 进入 UEFI 设置
3. 禁用 Secure Boot
4. 选择 USB 启动
5. 验证 MyOS 启动
```

---

## 验证里程碑

| 里程碑 | 验证方式 | 预期输出 |
|--------|----------|----------|
| M13.1 | PCI 枚举 | 列出所有 PCI 设备 |
| M13.2 | AHCI 检测 | 识别 SATA 控制器和磁盘 |
| M13.3 | AHCI 读写 | 读写 SATA 硬盘成功 |
| M13.4 | USB 启动盘 | U 盘启动 MyOS |
| M13.5 | USB 存储读写 | 挂载 U 盘文件系统 |
| M13.6 | 关机/重启 | poweroff/reboot 命令 |

---

## 硬件兼容性

### 已测试 (QEMU)
- virtio 设备
- IDE/ATA
- xHCI USB

### 目标支持
| 硬件类型 | 接口 | 驱动 |
|----------|------|------|
| 机械硬盘 | SATA | AHCI |
| SSD | SATA | AHCI |
| U 盘 | USB 2.0/3.0 | xHCI + MSC |
| SD 卡 | USB 读卡器 | xHCI + MSC |

### 暂不支持
| 硬件 | 原因 |
|------|------|
| NVMe SSD | 需要额外驱动 |
| eMMC | 需要 MMC 驱动 |
| 独立 SD 控制器 | 需要 SDHCI 驱动 |

---

## 目录结构变更

```
myos/
├── kernel/
│   └── drivers/
│       ├── pci.c           # 新增: PCI 枚举
│       ├── pci.h
│       ├── ahci.c          # 新增: AHCI/SATA 驱动
│       ├── ahci.h
│       ├── acpi.c          # 新增: ACPI 支持
│       ├── acpi.h
│       └── usb/
│           ├── usb_msc.c   # 新增: USB Mass Storage
│           └── usb_msc.h
├── scripts/
│   ├── make_usb.sh         # 新增: USB 制作脚本
│   ├── make_hybrid_iso.sh  # 新增: Hybrid ISO 脚本
│   └── test_physical.sh    # 新增: 物理机测试指南
└── docs/
    └── physical-boot-guide.md  # 新增: 物理机启动指南
```

---

## 风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| AHCI 硬件差异 | 严格按 AHCI 规范实现 |
| UEFI 复杂 | 先支持 Legacy BIOS，UEFI 可选 |
| 物理机调试困难 | 依赖串口输出调试 |
| USB 3.0 兼容性 | 已有 xHCI 驱动基础 |

---

## 预计工作量

| 阶段 | 任务数 | 复杂度 |
|------|--------|--------|
| 13.1 AHCI 驱动 | 6 | 高 |
| 13.2 USB MSC | 3 | 中 |
| 13.3 启动脚本 | 3 | 低 |
| 13.4 UEFI (可选) | 3 | 高 |
| 13.5 ACPI | 3 | 中 |
| **总计** | **18** | **中高** |

---

## 快速开始

完成此阶段后，用户可以：

```bash
# 1. 编译系统
make clean && make

# 2. 创建可启动 U 盘
make usb DEVICE=/dev/sdb

# 3. 插入 U 盘到目标机器，从 USB 启动

# 4. 看到 MyOS 启动界面
MyOS v0.1.0 booting...
[OK] Memory initialized
[OK] AHCI controller found
[OK] SATA disk: 256GB
[OK] Starting shell...

/ # _
```
