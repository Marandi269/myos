# 阶段 12: BusyBox 移植

## 状态: ⏸️ 暂停

**原因**: BusyBox 依赖过多，移植工作量大

### 已完成
- ✅ 12.1 Libc 扩展 (errno, setjmp, ctype, time, dirent, pwd, grp, fnmatch)
- ✅ 12.2 系统调用扩展 (50+ syscalls)
- ✅ BusyBox 源码下载并编译 (1.2MB 静态二进制)
- ✅ 集成到 initramfs
- ✅ /etc/passwd, /etc/group, /etc/profile 配置

### 待解决问题
- BusyBox ash 需要更多 syscall 支持 (如完整的 termios)
- 部分 libc 函数实现不完整
- 信号处理需要完善

### 建议
先完成阶段 13 (物理机启动)，后续再回来完善 BusyBox 支持。

---

## 目标
将 BusyBox 移植到 MyOS，提供完整的 Unix 工具集，替代简易 shell 和 coreutils。

---

## 背景分析

### 当前状态
- MyOS 已有基础 libc (stdio, unistd, string, stdlib 等)
- 已实现约 50+ 系统调用
- 有简易 shell 和少量 coreutils

### BusyBox 简介
BusyBox 是一个集成了 300+ Unix 工具的单一可执行文件，非常适合嵌入式和资源受限系统。

### 移植策略
采用 **静态链接 + 最小配置** 方式：
1. 使用 MyOS 的 libc 编译 BusyBox
2. 从最小功能集开始，逐步启用更多 applet
3. 缺失的系统调用逐个补充

---

## 任务分解

### 12.1 Libc 扩展 (必需) ✅ 已完成

BusyBox 依赖大量 POSIX 函数，已扩展 libc：

| 任务ID | 任务名称 | 状态 | 实现文件 |
|--------|----------|------|----------|
| BB-01 | 实现 errno 全局变量 | ✅ | libc/include/errno.h, libc/src/errno.c |
| BB-02 | 实现 setjmp/longjmp | ✅ | libc/include/setjmp.h, libc/src/setjmp.S |
| BB-03 | 扩展 stdio | ✅ | libc/include/stdio.h, libc/src/stdio.c |
| BB-04 | 实现 ctype.h | ✅ | libc/include/ctype.h, libc/src/ctype.c |
| BB-05 | 扩展 string.h | ✅ | libc/include/string.h, libc/src/string.c |
| BB-06 | 实现 stdlib 扩展 | ✅ | libc/include/stdlib.h, libc/src/stdlib.c |
| BB-07 | 实现 time.h | ✅ | libc/include/time.h, libc/src/time.c |
| BB-08 | 实现 dirent.h | ✅ | libc/include/dirent.h, libc/src/dirent.c |
| BB-09 | 实现 pwd.h/grp.h | ✅ | libc/include/pwd.h, grp.h, libc/src/pwd.c, grp.c |
| BB-10 | 实现 fnmatch | ✅ | libc/include/fnmatch.h, libc/src/fnmatch.c |

### 12.2 系统调用扩展 (必需) ✅ 已完成

| 任务ID | 任务名称 | 状态 | 实现文件 |
|--------|----------|------|----------|
| BB-11 | sys_stat / sys_fstat / sys_lstat | ✅ | kernel/proc/syscall_fs.c |
| BB-12 | sys_access | ✅ | kernel/proc/syscall_fs.c |
| BB-13 | sys_chmod / sys_chown | ✅ | kernel/proc/syscall_fs.c |
| BB-14 | sys_link / sys_unlink | ✅ | kernel/proc/syscall_fs.c |
| BB-15 | sys_rename | ⚠️ stub | kernel/proc/syscall_fs.c |
| BB-16 | sys_readlink / sys_symlink | ⚠️ stub | kernel/proc/syscall_fs.c |
| BB-17 | sys_umask | ✅ | kernel/proc/syscall_fs.c |
| BB-18 | sys_uname | ✅ | kernel/proc/syscall_misc.c |
| BB-19 | sys_getuid/geteuid/getgid/getegid | ✅ | kernel/proc/syscall_misc.c |
| BB-20 | sys_setuid / sys_setgid | ✅ | kernel/proc/syscall_misc.c |
| BB-21 | sys_ioctl (基础) | ✅ | kernel/proc/syscall_fs.c |
| BB-22 | sys_fcntl | ✅ | kernel/proc/syscall_fs.c |
| BB-23 | sys_nanosleep | ✅ | kernel/proc/syscall_misc.c |
| BB-24 | sys_getrlimit / sys_setrlimit | ✅ | kernel/proc/syscall_misc.c |
| BB-25 | sys_times | ✅ | kernel/proc/syscall_misc.c |
| BB-26 | sys_clock_gettime | ✅ | kernel/proc/syscall_misc.c |
| BB-27 | sys_gettimeofday | ✅ | kernel/proc/syscall_misc.c |
| BB-28 | sys_setsid | ✅ | kernel/proc/syscall_misc.c |

### 12.3 构建系统

| 任务ID | 任务名称 | 描述 | 验证方式 |
|--------|----------|------|----------|
| BB-30 | 创建 BusyBox 交叉编译配置 | CONFIG_CROSS_COMPILER_PREFIX | 编译通过 |
| BB-31 | 创建最小 .config | 仅启用核心 applet | 生成二进制 |
| BB-32 | 集成到 Makefile | make busybox | 自动构建 |
| BB-33 | 添加到 initramfs | /bin/busybox 和符号链接 | 启动验证 |

### 12.4 分阶段启用 Applet

#### 第一批 (最小可用)
```
sh          - Shell (ash)
echo        - 输出文本
cat         - 显示文件
ls          - 列出目录
pwd         - 显示当前目录
cd          - 切换目录 (shell 内置)
mkdir       - 创建目录
rm          - 删除文件
cp          - 复制文件
mv          - 移动文件
```

#### 第二批 (基础工具)
```
grep        - 文本搜索
sed         - 流编辑器
awk         - 文本处理
head/tail   - 文件头尾
wc          - 字数统计
sort        - 排序
uniq        - 去重
cut         - 字段切割
tr          - 字符转换
```

#### 第三批 (系统工具)
```
ps          - 进程列表
kill        - 发送信号
sleep       - 延时
date        - 日期时间
uname       - 系统信息
id          - 用户信息
env         - 环境变量
test/[      - 条件测试
```

#### 第四批 (高级工具)
```
mount/umount - 挂载/卸载
df          - 磁盘空间
du          - 目录大小
find        - 文件查找
xargs       - 参数构建
tar         - 归档工具
gzip        - 压缩
vi          - 文本编辑器
```

---

## 实现路径

```
┌─────────────────────────────────────────────────────────────┐
│                    阶段 12.1: Libc 扩展                      │
│                                                             │
│  errno → ctype → string扩展 → stdlib扩展 → stdio扩展        │
│                      ↓                                      │
│              setjmp/longjmp → dirent                        │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│                   阶段 12.2: Syscall 扩展                    │
│                                                             │
│  stat/fstat → access → unlink → rename → ioctl → fcntl     │
│                      ↓                                      │
│          uname → getuid/geteuid → nanosleep                 │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│                   阶段 12.3: 构建 BusyBox                    │
│                                                             │
│  下载源码 → 最小配置 → 交叉编译 → 静态链接 → 打包           │
└─────────────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────────────┐
│                   阶段 12.4: 集成测试                        │
│                                                             │
│  替换 /bin/sh → 测试第一批 applet → 逐步启用更多            │
└─────────────────────────────────────────────────────────────┘
```

---

## 详细规划

### 12.1 Libc 扩展详情

#### BB-01: errno
```c
// libc/include/errno.h
#ifndef _ERRNO_H
#define _ERRNO_H

extern int errno;

#define EPERM           1
#define ENOENT          2
#define ESRCH           3
#define EINTR           4
#define EIO             5
// ... 完整错误码列表
#endif

// libc/src/errno.c
int errno = 0;
```

#### BB-02: setjmp/longjmp
```c
// libc/include/setjmp.h
typedef struct {
    uint64_t rbx, rbp, r12, r13, r14, r15;
    uint64_t rsp, rip;
} jmp_buf[1];

int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

// 汇编实现
// libc/src/setjmp.S
```

#### BB-08: dirent
```c
// libc/include/dirent.h
typedef struct {
    int fd;
} DIR;

struct dirent {
    uint64_t d_ino;
    uint64_t d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[256];
};

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
```

### 12.2 关键系统调用

#### BB-11: stat/fstat/lstat
```c
// kernel/proc/syscall.c

struct stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_rdev;
    int64_t  st_size;
    int64_t  st_blksize;
    int64_t  st_blocks;
    uint64_t st_atime;
    uint64_t st_mtime;
    uint64_t st_ctime;
};

int64_t sys_stat(const char *pathname, struct stat *statbuf);
int64_t sys_fstat(int fd, struct stat *statbuf);
int64_t sys_lstat(const char *pathname, struct stat *statbuf);
```

#### BB-18: uname
```c
struct utsname {
    char sysname[65];    // "MyOS"
    char nodename[65];   // hostname
    char release[65];    // "0.1.0"
    char version[65];    // "#1 SMP"
    char machine[65];    // "x86_64"
};

int64_t sys_uname(struct utsname *buf);
```

#### BB-21: ioctl (基础)
```c
// 仅实现终端相关
#define TCGETS      0x5401
#define TCSETS      0x5402
#define TIOCGWINSZ  0x5413
#define TIOCSWINSZ  0x5414

int64_t sys_ioctl(int fd, unsigned long request, void *arg);
```

### 12.3 BusyBox 配置

```makefile
# busybox/.config 最小配置示例

CONFIG_STATIC=y
CONFIG_CROSS_COMPILER_PREFIX="x86_64-myos-"
CONFIG_SYSROOT="/path/to/myos/sysroot"

# Shell
CONFIG_ASH=y
CONFIG_ASH_JOB_CONTROL=n
CONFIG_HUSH=n

# 核心工具
CONFIG_CAT=y
CONFIG_ECHO=y
CONFIG_LS=y
CONFIG_MKDIR=y
CONFIG_PWD=y
CONFIG_RM=y
CONFIG_CP=y
CONFIG_MV=y

# 禁用不需要的功能
CONFIG_FEATURE_PREFER_APPLETS=y
CONFIG_FEATURE_SH_STANDALONE=y
CONFIG_FEATURE_EDITING=n
CONFIG_FEATURE_TAB_COMPLETION=n
```

---

## 验证里程碑

| 里程碑 | 验证方式 | 预期输出 |
|--------|----------|----------|
| M12.1 | libc 扩展完成 | 所有新函数可链接 |
| M12.2 | stat 系统调用 | `ls -l` 显示文件属性 |
| M12.3 | BusyBox 编译通过 | 生成静态 busybox 二进制 |
| M12.4 | ash shell 启动 | BusyBox shell 提示符 |
| M12.5 | 基础命令可用 | echo, cat, ls, pwd 正常 |
| M12.6 | 文件操作正常 | cp, mv, rm 可用 |
| M12.7 | 管道工作 | `ls | grep` 正常 |
| **M12.8** | **交互式环境** | **完整可用的交互式 Shell** |

---

## 最终目标：交互式环境

BusyBox 移植成功后，系统启动应呈现一个 **完整可交互的 Unix 环境**：

### 启动流程
```
MyOS v0.1.0 booting...
[OK] Memory initialized
[OK] Filesystem mounted
[OK] Network configured
[OK] Starting init...

BusyBox v1.36.1 (MyOS) built-in shell (ash)
Enter 'help' for a list of built-in commands.

/ # _
```

### 交互式功能清单

| 功能 | 命令示例 | 依赖 |
|------|----------|------|
| 命令执行 | `ls -la /bin` | ash + stat |
| 管道操作 | `cat /etc/passwd \| grep root` | pipe |
| 重定向 | `echo hello > /tmp/test.txt` | open/write |
| 命令历史 | ↑/↓ 键回溯 | readline (可选) |
| Tab 补全 | `ls /e<TAB>` → `/etc` | readline (可选) |
| 作业控制 | `sleep 100 &`, `jobs`, `fg` | signal + waitpid |
| 环境变量 | `export PATH=/bin:/usr/bin` | environ |
| 脚本执行 | `sh /etc/init.d/network` | execve |
| 信号处理 | `Ctrl+C` 终止, `Ctrl+Z` 暂停 | signal |

### 初始环境配置

#### /etc/passwd
```
root:x:0:0:root:/root:/bin/sh
nobody:x:65534:65534:nobody:/:/bin/false
```

#### /etc/group
```
root:x:0:root
nogroup:x:65534:
```

#### /etc/profile (Shell 启动脚本)
```sh
# MyOS Shell Profile
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
export HOME=/root
export PS1='\u@myos:\w\$ '
export TERM=vt100

# Welcome message
echo "Welcome to MyOS!"
echo "Type 'help' for available commands."
```

#### /etc/inittab (init 配置)
```
# 系统启动后运行的脚本
::sysinit:/etc/init.d/rcS

# 启动交互式 shell
::respawn:/bin/sh

# Ctrl+Alt+Del 处理
::ctrlaltdel:/sbin/reboot
```

#### /etc/init.d/rcS (启动脚本)
```sh
#!/bin/sh
# 挂载文件系统
mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs dev /dev

# 设置主机名
hostname myos

# 配置网络 (如果有)
if [ -x /sbin/ifconfig ]; then
    ifconfig lo 127.0.0.1 up
    ifconfig eth0 up
    # DHCP 或静态 IP
fi

echo "System initialization complete."
```

### 交互示例会话

```sh
/ # uname -a
MyOS myos 0.1.0 #1 SMP x86_64

/ # cat /proc/version
MyOS version 0.1.0

/ # ls -l /bin
total 512
lrwxrwxrwx    1 root     root            7 Jan  1 00:00 cat -> busybox
lrwxrwxrwx    1 root     root            7 Jan  1 00:00 ls -> busybox
lrwxrwxrwx    1 root     root            7 Jan  1 00:00 sh -> busybox
-rwxr-xr-x    1 root     root       524288 Jan  1 00:00 busybox

/ # echo "Hello, MyOS!" > /tmp/hello.txt
/ # cat /tmp/hello.txt
Hello, MyOS!

/ # ps
PID   USER     COMMAND
    1 root     init
    2 root     sh

/ # mkdir -p /home/user
/ # cd /home/user
/home/user # pwd
/home/user

/ # for i in 1 2 3; do echo "Count: $i"; done
Count: 1
Count: 2
Count: 3

/ # cat /proc/meminfo
MemTotal:       128000 kB
MemFree:         64000 kB
...

/ # ifconfig eth0
eth0      Link encap:Ethernet  HWaddr 52:54:00:12:34:56
          inet addr:10.0.2.15  Bcast:10.0.2.255  Mask:255.255.255.0
          UP BROADCAST RUNNING MULTICAST  MTU:1500

/ # ping -c 3 10.0.2.2
PING 10.0.2.2 (10.0.2.2): 56 data bytes
64 bytes from 10.0.2.2: seq=0 ttl=64 time=0.5 ms
64 bytes from 10.0.2.2: seq=1 ttl=64 time=0.3 ms
64 bytes from 10.0.2.2: seq=2 ttl=64 time=0.3 ms

/ # vi /tmp/test.c
(启动 vi 编辑器)

/ # exit
```

### 12.5 交互式环境任务

| 任务ID | 任务名称 | 描述 | 验证方式 |
|--------|----------|------|----------|
| BB-40 | 创建 /etc/passwd, /etc/group | 用户数据库 | getpwuid 测试 |
| BB-41 | 创建 /etc/profile | Shell 启动配置 | 环境变量生效 |
| BB-42 | 实现 /proc 虚拟文件系统 | /proc/meminfo 等 | cat /proc/version |
| BB-43 | 启用 ash 行编辑 | 方向键、删除键 | 交互体验 |
| BB-44 | 实现 hostname | 主机名支持 | hostname 命令 |
| BB-45 | 创建 init 脚本框架 | /etc/init.d/* | 自动启动服务 |
| BB-46 | 实现 tty 登录 (可选) | getty + login | 多用户登录 |

---

## 风险与缓解

| 风险 | 缓解措施 |
|------|----------|
| BusyBox 依赖过多 | 从最小配置开始，逐步启用 |
| libc 不兼容 | 使用 stub 函数返回 -ENOSYS |
| 编译错误多 | 使用 -Wno-error，逐个修复 |
| ash 需要作业控制 | 先禁用 JOB_CONTROL |
| 缺少 /etc/passwd | 创建最小化配置文件 |

---

## 目录结构变更

```
myos/
├── busybox/                 # BusyBox 源码 (git submodule)
│   └── .config             # MyOS 定制配置
├── libc/
│   ├── include/
│   │   ├── errno.h         # 新增
│   │   ├── setjmp.h        # 新增
│   │   ├── ctype.h         # 新增
│   │   ├── time.h          # 新增
│   │   ├── pwd.h           # 新增
│   │   ├── grp.h           # 新增
│   │   └── sys/
│   │       ├── stat.h      # 新增
│   │       └── utsname.h   # 新增
│   └── src/
│       ├── errno.c         # 新增
│       ├── setjmp.S        # 新增
│       ├── ctype.c         # 新增
│       ├── time.c          # 新增
│       └── dirent.c        # 新增
├── sysroot/                 # 交叉编译 sysroot
│   ├── bin/
│   │   └── busybox -> /bin/busybox
│   ├── etc/
│   │   ├── passwd
│   │   └── group
│   └── usr/
│       └── include/ -> ../../libc/include
└── initramfs/
    └── bin/
        └── busybox         # 编译后的 BusyBox
```

---

## 预计工作量

| 阶段 | 任务数 | 复杂度 |
|------|--------|--------|
| 12.1 Libc 扩展 | 10 | 中 |
| 12.2 Syscall 扩展 | 15 | 高 |
| 12.3 构建系统 | 4 | 低 |
| 12.4 集成测试 | - | 中 |
| **总计** | **29+** | **高** |

---

## 替代方案

如果 BusyBox 移植难度过大，可考虑：

1. **toybox**: 更轻量的 BusyBox 替代品，BSD 许可
2. **继续自研**: 逐步完善现有 shell 和 coreutils
3. **移植 dash**: 仅移植一个轻量 shell

---

## 参考资料

- [BusyBox 官方文档](https://busybox.net/downloads/BusyBox.html)
- [BusyBox 最小配置指南](https://busybox.net/FAQ.html#configure)
- [Linux x86_64 系统调用表](https://blog.rchapman.org/posts/Linux_System_Call_Table_for_x86_64/)
