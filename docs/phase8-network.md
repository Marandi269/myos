# Phase 8: 网络栈 (Networking)

## 目标
实现基础网络功能，包括网卡驱动、TCP/IP 协议栈和 Socket API。

## 前置依赖
- ✅ Phase 2: 内存管理 (PMM, Heap)
- ✅ Phase 3: 进程管理 (调度器)
- ✅ Phase 4: 系统调用框架
- ✅ Phase 5: 文件系统 (VFS, 文件描述符)
- ✅ Phase 7: IPC (管道基础设施可复用)

---

## 任务列表

### N-01: 网卡驱动 (virtio-net)
**优先级**: P0 (必须首先完成)
**依赖**: I-02 (PIC), M-04 (Heap)

**描述**: 实现 QEMU virtio-net 网卡驱动，这是最简单且性能最好的虚拟网卡。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| N-01.1 | PCI 设备枚举 | 检测到 virtio-net 设备 |
| N-01.2 | virtio 队列初始化 | 分配 vring 缓冲区 |
| N-01.3 | MAC 地址读取 | 打印网卡 MAC 地址 |
| N-01.4 | 数据包发送 | 发送原始以太网帧 |
| N-01.5 | 数据包接收 | 接收中断处理 |
| N-01.6 | 网卡接口抽象 | netdev 结构体 |

**文件**:
- `kernel/drivers/pci.c` - PCI 总线驱动
- `kernel/drivers/virtio.c` - virtio 通用层
- `kernel/drivers/virtio_net.c` - virtio-net 驱动
- `kernel/net/netdev.c` - 网络设备抽象

**数据结构**:
```c
typedef struct netdev {
    char name[16];
    uint8_t mac[6];
    int (*send)(struct netdev *dev, void *data, size_t len);
    void (*receive)(struct netdev *dev, void *data, size_t len);
    void *priv;
} netdev_t;
```

**验证**:
```
[PCI] Found device: 1af4:1000 (virtio-net)
[virtio-net] MAC: 52:54:00:12:34:56
[virtio-net] Initialized
```

---

### N-02: 以太网帧处理
**优先级**: P0
**依赖**: N-01

**描述**: 解析和构造以太网帧。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| N-02.1 | 以太网头解析 | 解析目标/源 MAC、类型 |
| N-02.2 | 以太网帧构造 | 构造发送帧 |
| N-02.3 | 协议分发 | 根据 EtherType 分发 |

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

### N-03: ARP 协议
**优先级**: P0
**依赖**: N-02

**描述**: 实现 ARP (地址解析协议)，将 IP 地址映射到 MAC 地址。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| N-03.1 | ARP 表 | 存储 IP-MAC 映射 |
| N-03.2 | ARP 请求发送 | 广播 ARP 请求 |
| N-03.3 | ARP 应答处理 | 更新 ARP 表 |
| N-03.4 | ARP 应答发送 | 响应 ARP 请求 |

**ARP 表结构**:
```c
#define ARP_TABLE_SIZE 64

typedef struct arp_entry {
    uint32_t ip;
    uint8_t mac[6];
    uint32_t timestamp;
    int valid;
} arp_entry_t;
```

**验证**:
```
[ARP] Request: Who has 10.0.2.2? Tell 10.0.2.15
[ARP] Reply: 10.0.2.2 is at 52:55:0a:00:02:02
```

---

### N-04: IP 协议
**优先级**: P0
**依赖**: N-03

**描述**: 实现 IPv4 协议的基本收发功能。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| N-04.1 | IP 头解析 | 解析 IP 数据包 |
| N-04.2 | IP 头构造 | 构造 IP 数据包 |
| N-04.3 | IP 校验和 | 计算/验证校验和 |
| N-04.4 | IP 配置 | 设置本机 IP/子网/网关 |
| N-04.5 | 协议分发 | 分发到 ICMP/UDP/TCP |

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

**验证**:
```
[IP] Configured: 10.0.2.15/24, gateway 10.0.2.2
[IP] Received packet from 10.0.2.2, protocol=ICMP
```

---

### N-05: ICMP 协议
**优先级**: P1
**依赖**: N-04

**描述**: 实现 ICMP 协议，支持 ping 功能。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| N-05.1 | ICMP Echo Reply | 响应 ping 请求 |
| N-05.2 | ICMP Echo Request | 发起 ping 请求 |
| N-05.3 | ping 命令 | 用户态 ping 工具 |

**验证**:
```
$ ping 10.0.2.2
PING 10.0.2.2: 64 bytes from 10.0.2.2: icmp_seq=1 ttl=64 time=0.5ms
PING 10.0.2.2: 64 bytes from 10.0.2.2: icmp_seq=2 ttl=64 time=0.3ms
```

---

### N-06: UDP 协议
**优先级**: P1
**依赖**: N-04

**描述**: 实现 UDP 协议，支持无连接数据报传输。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| N-06.1 | UDP 头解析/构造 | UDP 数据包处理 |
| N-06.2 | UDP 端口管理 | 端口绑定/分配 |
| N-06.3 | UDP 发送 | sendto() 工作 |
| N-06.4 | UDP 接收 | recvfrom() 工作 |

**验证**:
```c
// 简单 UDP echo
int sock = socket(AF_INET, SOCK_DGRAM, 0);
bind(sock, ...);
recvfrom(sock, buf, ...);
sendto(sock, buf, ...);
```

---

### N-07: TCP 协议
**优先级**: P2
**依赖**: N-04

**描述**: 实现 TCP 协议，支持可靠的流式传输。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| N-07.1 | TCP 状态机 | LISTEN/SYN/ESTABLISHED 等 |
| N-07.2 | 三次握手 | 建立连接 |
| N-07.3 | 四次挥手 | 关闭连接 |
| N-07.4 | 数据发送 | 带序列号发送 |
| N-07.5 | 数据接收 | ACK 确认 |
| N-07.6 | 重传机制 | 超时重传 |
| N-07.7 | 滑动窗口 | 流量控制 |

**TCP 状态机**:
```
CLOSED -> LISTEN -> SYN_RCVD -> ESTABLISHED -> FIN_WAIT_1 -> ...
       -> SYN_SENT -> ESTABLISHED -> CLOSE_WAIT -> LAST_ACK -> CLOSED
```

**验证**:
```c
// TCP client
int sock = socket(AF_INET, SOCK_STREAM, 0);
connect(sock, ...);
send(sock, "GET / HTTP/1.0\r\n\r\n", ...);
recv(sock, buf, ...);
close(sock);
```

---

### N-08: Socket API
**优先级**: P1
**依赖**: N-06 (UDP 优先)

**描述**: 实现 BSD Socket API 系统调用。

**系统调用列表**:
| 系统调用 | 功能 | 优先级 |
|----------|------|--------|
| socket | 创建套接字 | P0 |
| bind | 绑定地址 | P0 |
| listen | 监听连接 | P1 |
| accept | 接受连接 | P1 |
| connect | 发起连接 | P1 |
| send/sendto | 发送数据 | P0 |
| recv/recvfrom | 接收数据 | P0 |
| close | 关闭套接字 | P0 |
| setsockopt | 设置选项 | P2 |
| getsockopt | 获取选项 | P2 |

**Socket 结构**:
```c
typedef struct socket {
    int type;           /* SOCK_STREAM, SOCK_DGRAM */
    int protocol;       /* TCP, UDP */
    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;
    int state;          /* TCP state */
    /* 缓冲区 */
    void *recv_buf;
    void *send_buf;
} socket_t;
```

---

### N-09: DHCP 客户端
**优先级**: P2
**依赖**: N-06 (UDP)

**描述**: 实现 DHCP 客户端，自动获取 IP 配置。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| N-09.1 | DHCP Discover | 广播发现请求 |
| N-09.2 | DHCP Offer 处理 | 解析服务器响应 |
| N-09.3 | DHCP Request | 请求 IP 地址 |
| N-09.4 | DHCP ACK 处理 | 配置网络参数 |

**验证**:
```
[DHCP] Discover sent
[DHCP] Offer received: 10.0.2.15
[DHCP] Request sent
[DHCP] ACK received
[DHCP] Configured: IP=10.0.2.15, Mask=255.255.255.0, Gateway=10.0.2.2
```

---

## 开发顺序

```
N-01 (virtio-net 驱动)
    │
    v
N-02 (以太网)
    │
    v
N-03 (ARP)
    │
    v
N-04 (IP)
    │
    ├──────────────┬──────────────┐
    v              v              v
N-05 (ICMP)    N-06 (UDP)    N-07 (TCP)
    │              │              │
    v              v              v
  ping          N-08 (Socket API)
                   │
                   v
               N-09 (DHCP)
```

**建议顺序**:
1. N-01 ~ N-04: 网络基础设施
2. N-05: ICMP (验证网络连通性)
3. N-06 + N-08: UDP + Socket API
4. N-09: DHCP (自动配置)
5. N-07: TCP (复杂，可选)

---

## 验证里程碑

| 编号 | 里程碑 | 验证方式 |
|------|--------|----------|
| M8.1 | 网卡初始化 | 检测到 virtio-net，打印 MAC |
| M8.2 | ARP 工作 | 能解析网关 MAC |
| M8.3 | ping 工作 | 能 ping 通网关 |
| M8.4 | UDP 工作 | UDP echo 测试通过 |
| M8.5 | DHCP 工作 | 自动获取 IP |
| M8.6 | TCP 工作 | HTTP GET 成功 |

**最终验证**:
```
$ ifconfig
eth0: 10.0.2.15/24
      MAC: 52:54:00:12:34:56
      Gateway: 10.0.2.2

$ ping 10.0.2.2
PING 10.0.2.2: 64 bytes, icmp_seq=1, ttl=64, time=0.5ms
PING 10.0.2.2: 64 bytes, icmp_seq=2, ttl=64, time=0.3ms

$ nc -u 10.0.2.2 7    # UDP echo
hello
hello
```

---

## QEMU 网络配置

```bash
# 用户模式网络 (默认)
qemu-system-x86_64 -cdrom myos.iso \
    -netdev user,id=net0 \
    -device virtio-net-pci,netdev=net0

# 带端口转发
qemu-system-x86_64 -cdrom myos.iso \
    -netdev user,id=net0,hostfwd=tcp::8080-:80 \
    -device virtio-net-pci,netdev=net0
```

QEMU 用户模式网络默认配置:
- 客户机 IP: 10.0.2.15
- 网关/DNS: 10.0.2.2
- 子网掩码: 255.255.255.0

---

## 技术注意事项

### virtio-net 要点
1. 需要实现 PCI 设备枚举
2. virtio 使用 vring 环形队列
3. 需要正确处理设备特性协商
4. 发送/接收需要内存屏障

### 网络栈要点
1. 所有数据包使用网络字节序 (大端)
2. 需要正确计算各层校验和
3. ARP 表需要超时机制
4. TCP 需要定时器支持重传

### 性能考虑
1. 使用零拷贝尽量减少数据复制
2. 缓冲区池复用
3. 中断合并减少中断次数

---

## 交付物清单

- [ ] `kernel/drivers/pci.c/h` - PCI 总线驱动
- [ ] `kernel/drivers/virtio.c/h` - virtio 通用层
- [ ] `kernel/drivers/virtio_net.c/h` - virtio-net 驱动
- [ ] `kernel/net/netdev.c/h` - 网络设备抽象
- [ ] `kernel/net/ethernet.c/h` - 以太网处理
- [ ] `kernel/net/arp.c/h` - ARP 协议
- [ ] `kernel/net/ip.c/h` - IP 协议
- [ ] `kernel/net/icmp.c/h` - ICMP 协议
- [ ] `kernel/net/udp.c/h` - UDP 协议
- [ ] `kernel/net/tcp.c/h` - TCP 协议 (可选)
- [ ] `kernel/net/socket.c/h` - Socket 实现
- [ ] `kernel/net/dhcp.c/h` - DHCP 客户端
- [ ] `libc/include/sys/socket.h` - Socket 头文件
- [ ] `libc/include/netinet/in.h` - 网络地址
- [ ] `libc/include/arpa/inet.h` - 地址转换
- [ ] `userspace/coreutils/ping.c` - ping 命令
- [ ] `userspace/coreutils/ifconfig.c` - 网络配置
- [ ] 更新 Makefile
