# Phase 8: 网络栈 (Networking)

## 状态: ✅ 已完成

## 目标
实现基础网络功能，包括网卡驱动、TCP/IP 协议栈和 Socket API。

## 前置依赖
- ✅ Phase 2: 内存管理 (PMM, Heap)
- ✅ Phase 3: 进程管理 (调度器)
- ✅ Phase 4: 系统调用框架
- ✅ Phase 5: 文件系统 (VFS, 文件描述符)
- ✅ Phase 7: IPC (管道基础设施可复用)

---

## 实现总结

### 已完成的模块

| 模块 | 文件 | 状态 |
|------|------|------|
| N-01: PCI + virtio-net | `drivers/pci.c/h`, `drivers/virtio.c/h`, `drivers/virtio_net.c/h` | ✅ |
| N-02: 以太网 | `net/ethernet.c/h` | ✅ |
| N-03: ARP | `net/arp.c/h` | ✅ |
| N-04: IP | `net/ip.c/h` | ✅ |
| N-05: ICMP | `net/icmp.c/h` | ✅ |
| N-06: UDP | `net/udp.c/h` | ✅ |
| N-07: TCP | `net/tcp.c/h` | ✅ |
| N-08: Socket API | `net/socket.c/h` | ✅ |
| N-09: DHCP | `net/dhcp.c/h` | ✅ |
| 网络初始化 | `net/net.c/h` | ✅ |

### 验证结果

```
[NET] Initializing network stack...
[PCI] Scanning PCI bus...
[PCI] 00:03.0 1af4:1000 class=02:00 irq=11
[PCI] Found 6 device(s)
[ethernet] Initialized
[ARP] Initialized
[IP] Configured: 10.0.2.15/255.255.255.0 gateway 10.0.2.2
[ICMP] Initialized
[UDP] Initialized
[TCP] Initialized
[Socket] Initialized
[DHCP] Initialized
[virtio-net] Found device at 00:03.0
[virtio-net] I/O base: 0xc000, IRQ: 11
[virtio-net] MAC: 52:54:00:12:34:56
[virtio-net] RX queue size: 256
[virtio-net] TX queue size: 256
[virtio-net] Initialized
[netdev] Registered eth0
[NET] Network stack initialized

[NET] Running network tests...
[NET] Device: eth0
[NET] MAC: 52:54:00:12:34:56
[NET] IP: 10.0.2.15
[NET] Netmask: 255.255.255.0
[NET] Gateway: 10.0.2.2

[NET] Pinging gateway...
[ICMP] Sending echo request to 10.0.2.2: id=1 seq=1
[ARP] Request: Who has 10.0.2.2?
[IP] Queuing packet, waiting for ARP resolution of 10.0.2.2
[ARP] Added: 10.0.2.2 -> 52:55:0a:00:02:02
[ARP] Reply: 10.0.2.2 is at 52:55:0a:00:02:02
[ARP] Sent queued packet to resolved IP
[NET] Network tests completed
```

---

## 任务列表

### N-01: 网卡驱动 (virtio-net) ✅
**优先级**: P0 (必须首先完成)
**依赖**: I-02 (PIC), M-04 (Heap)

**描述**: 实现 QEMU virtio-net 网卡驱动，这是最简单且性能最好的虚拟网卡。

**子任务**:
| ID | 任务 | 状态 |
|----|------|------|
| N-01.1 | PCI 设备枚举 | ✅ |
| N-01.2 | virtio 队列初始化 | ✅ |
| N-01.3 | MAC 地址读取 | ✅ |
| N-01.4 | 数据包发送 | ✅ |
| N-01.5 | 数据包接收 | ✅ |
| N-01.6 | 网卡接口抽象 | ✅ |

**文件**:
- `kernel/drivers/pci.c/h` - PCI 总线驱动
- `kernel/drivers/virtio.c/h` - virtio 通用层
- `kernel/drivers/virtio_net.c/h` - virtio-net 驱动
- `kernel/net/netdev.c/h` - 网络设备抽象

**数据结构**:
```c
typedef struct netdev {
    char name[16];
    uint8_t mac[6];
    uint16_t mtu;
    int (*send)(struct netdev *dev, void *data, size_t len);
    void (*receive)(struct netdev *dev, void *data, size_t len);
    void *priv;
} netdev_t;
```

---

### N-02: 以太网帧处理 ✅
**优先级**: P0
**依赖**: N-01

**描述**: 解析和构造以太网帧。

**子任务**:
| ID | 任务 | 状态 |
|----|------|------|
| N-02.1 | 以太网头解析 | ✅ |
| N-02.2 | 以太网帧构造 | ✅ |
| N-02.3 | 协议分发 | ✅ |

**以太网帧格式**:
```
+------------------+------------------+--------+----------------+-----+
| Dest MAC (6)     | Src MAC (6)      | Type(2)| Payload (46-1500) | CRC |
+------------------+------------------+--------+----------------+-----+
```

**EtherType**:
- 0x0800: IPv4
- 0x0806: ARP
- 0x86DD: IPv6

---

### N-03: ARP 协议 ✅
**优先级**: P0
**依赖**: N-02

**描述**: 实现 ARP (地址解析协议)，将 IP 地址映射到 MAC 地址。

**子任务**:
| ID | 任务 | 状态 |
|----|------|------|
| N-03.1 | ARP 表 | ✅ |
| N-03.2 | ARP 请求发送 | ✅ |
| N-03.3 | ARP 应答处理 | ✅ |
| N-03.4 | ARP 应答发送 | ✅ |
| N-03.5 | ARP 待发送队列 | ✅ |

**设计亮点 - 异步 ARP 解析**:

采用分层异步处理模式，IP 层发送包时如果 ARP 缓存未命中：
1. 将 IP 包放入 ARP 待发送队列
2. 发送 ARP 请求
3. 当 ARP 回复到达时，自动发送队列中的包

```c
/* ARP 待发送队列 */
typedef struct arp_pending {
    netdev_t *dev;
    uint32_t ip;
    uint8_t data[ARP_PENDING_PKT_SIZE];
    size_t len;
    bool valid;
} arp_pending_t;

/* 收到 ARP 回复后自动处理待发送包 */
void arp_process_pending(uint32_t ip);
```

**ARP 表结构**:
```c
#define ARP_TABLE_SIZE 64
#define ARP_PENDING_MAX 16

typedef struct arp_entry {
    uint32_t ip;
    uint8_t mac[6];
    uint64_t timestamp;
    bool valid;
} arp_entry_t;
```

---

### N-04: IP 协议 ✅
**优先级**: P0
**依赖**: N-03

**描述**: 实现 IPv4 协议的基本收发功能。

**子任务**:
| ID | 任务 | 状态 |
|----|------|------|
| N-04.1 | IP 头解析 | ✅ |
| N-04.2 | IP 头构造 | ✅ |
| N-04.3 | IP 校验和 | ✅ |
| N-04.4 | IP 配置 | ✅ |
| N-04.5 | 协议分发 | ✅ |

**IP 头结构**:
```c
typedef struct ip_header {
    uint8_t  version_ihl;    /* Version (4) + IHL (4) */
    uint8_t  tos;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_fragment;
    uint8_t  ttl;
    uint8_t  protocol;       /* 1=ICMP, 6=TCP, 17=UDP */
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed)) ip_header_t;
```

---

### N-05: ICMP 协议 ✅
**优先级**: P1
**依赖**: N-04

**描述**: 实现 ICMP 协议，支持 ping 功能。

**子任务**:
| ID | 任务 | 状态 |
|----|------|------|
| N-05.1 | ICMP Echo Reply | ✅ |
| N-05.2 | ICMP Echo Request | ✅ |
| N-05.3 | ICMP 错误消息 | ✅ |

---

### N-06: UDP 协议 ✅
**优先级**: P1
**依赖**: N-04

**描述**: 实现 UDP 协议，支持无连接数据报传输。

**子任务**:
| ID | 任务 | 状态 |
|----|------|------|
| N-06.1 | UDP 头解析/构造 | ✅ |
| N-06.2 | UDP 端口管理 | ✅ |
| N-06.3 | UDP 发送 | ✅ |
| N-06.4 | UDP 接收 | ✅ |

---

### N-07: TCP 协议 ✅
**优先级**: P2
**依赖**: N-04

**描述**: 实现 TCP 协议，支持可靠的流式传输。

**子任务**:
| ID | 任务 | 状态 |
|----|------|------|
| N-07.1 | TCP 状态机 | ✅ |
| N-07.2 | 三次握手 | ✅ |
| N-07.3 | 四次挥手 | ✅ |
| N-07.4 | 数据发送 | ✅ |
| N-07.5 | 数据接收 | ✅ |
| N-07.6 | 重传机制 | ⚠️ 基础实现 |
| N-07.7 | 滑动窗口 | ⚠️ 基础实现 |

**TCP 状态机**:
```
CLOSED -> LISTEN -> SYN_RCVD -> ESTABLISHED -> FIN_WAIT_1 -> ...
       -> SYN_SENT -> ESTABLISHED -> CLOSE_WAIT -> LAST_ACK -> CLOSED
```

---

### N-08: Socket API ✅
**优先级**: P1
**依赖**: N-06 (UDP 优先)

**描述**: 实现 BSD Socket API 系统调用。

**系统调用列表**:
| 系统调用 | 功能 | 状态 |
|----------|------|------|
| socket | 创建套接字 | ✅ |
| bind | 绑定地址 | ✅ |
| listen | 监听连接 | ✅ |
| accept | 接受连接 | ⚠️ 基础实现 |
| connect | 发起连接 | ✅ |
| send/sendto | 发送数据 | ✅ |
| recv/recvfrom | 接收数据 | ✅ |
| shutdown | 关闭连接 | ✅ |
| closesocket | 关闭套接字 | ✅ |

---

### N-09: DHCP 客户端 ✅
**优先级**: P2
**依赖**: N-06 (UDP)

**描述**: 实现 DHCP 客户端，自动获取 IP 配置。

**子任务**:
| ID | 任务 | 状态 |
|----|------|------|
| N-09.1 | DHCP Discover | ✅ |
| N-09.2 | DHCP Offer 处理 | ✅ |
| N-09.3 | DHCP Request | ✅ |
| N-09.4 | DHCP ACK 处理 | ✅ |

---

## QEMU 网络配置

`scripts/run.sh` 已更新，自动启用 virtio-net:

```bash
qemu-system-x86_64 \
    -cdrom "$ISO" \
    -serial mon:stdio \
    -display none \
    -m 128M \
    -no-reboot \
    -no-shutdown \
    -netdev user,id=net0 \
    -device virtio-net-pci,netdev=net0
```

QEMU 用户模式网络默认配置:
- 客户机 IP: 10.0.2.15
- 网关/DNS: 10.0.2.2
- 子网掩码: 255.255.255.0

---

## 架构设计

### 分层结构

```
┌─────────────────────────────────────────────────┐
│              Socket API (用户接口)               │
├─────────────────────────────────────────────────┤
│         TCP          │          UDP             │
├──────────────────────┴──────────────────────────┤
│                 ICMP          │                 │
├───────────────────────────────┴─────────────────┤
│                      IP                         │
├─────────────────────────────────────────────────┤
│                     ARP                         │
├─────────────────────────────────────────────────┤
│                  Ethernet                       │
├─────────────────────────────────────────────────┤
│               virtio-net 驱动                   │
├─────────────────────────────────────────────────┤
│                    PCI                          │
└─────────────────────────────────────────────────┘
```

### ARP 异步解析流程

```
IP层发送包 ─→ 查ARP缓存 ─→ 命中 ─→ 直接发送
                 │
                 └─→ 未命中 ─→ 包入队列 ─→ 发ARP请求
                                              │
收到ARP回复 ←────────────────────────────────┘
     │
     └─→ 更新ARP表 ─→ 处理待发送队列 ─→ 发送排队的包
```

---

## 交付物清单

- [x] `kernel/drivers/pci.c/h` - PCI 总线驱动
- [x] `kernel/drivers/virtio.c/h` - virtio 通用层
- [x] `kernel/drivers/virtio_net.c/h` - virtio-net 驱动
- [x] `kernel/net/netdev.c/h` - 网络设备抽象
- [x] `kernel/net/ethernet.c/h` - 以太网处理
- [x] `kernel/net/arp.c/h` - ARP 协议
- [x] `kernel/net/ip.c/h` - IP 协议
- [x] `kernel/net/icmp.c/h` - ICMP 协议
- [x] `kernel/net/udp.c/h` - UDP 协议
- [x] `kernel/net/tcp.c/h` - TCP 协议
- [x] `kernel/net/socket.c/h` - Socket 实现
- [x] `kernel/net/dhcp.c/h` - DHCP 客户端
- [x] `kernel/net/net.c/h` - 网络栈初始化
- [x] `libc/include/sys/socket.h` - Socket 头文件
- [x] `libc/include/netinet/in.h` - 网络地址
- [x] `libc/include/arpa/inet.h` - 地址转换
- [x] `userspace/coreutils/ping.c` - ping 命令 (占位)
- [x] `userspace/coreutils/ifconfig.c` - 网络配置 (占位)
- [x] 更新 Makefile
- [x] 更新 scripts/run.sh

---

## 后续改进方向

1. **TCP 完善**: 实现完整的重传机制和拥塞控制
2. **DNS 客户端**: 实现域名解析
3. **多网卡支持**: 路由表和网卡选择
4. **IPv6 支持**: 基础 IPv6 协议栈
5. **中断驱动**: 改进为中断驱动而非轮询
6. **性能优化**: 零拷贝、缓冲区池
