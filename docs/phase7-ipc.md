# Phase 7: 进程间通信 (IPC)

## 目标
实现进程间通信机制，包括管道、信号和共享内存。

## 前置依赖
- ✅ Phase 3: 进程管理 (PCB, 调度器)
- ✅ Phase 4: 系统调用框架
- ✅ Phase 5: 文件系统 (VFS, 文件描述符)
- ✅ Phase 6: 用户空间 (fork/exec)

---

## 任务列表

### IPC-01: 管道 (Pipe)
**优先级**: P0 (核心功能)
**依赖**: FS-02 (文件描述符表)

**描述**: 实现匿名管道，支持父子进程间单向数据流。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| IPC-01.1 | 管道数据结构 | 定义 pipe_t 结构 |
| IPC-01.2 | pipe() 系统调用 | 返回读写 fd 对 |
| IPC-01.3 | 管道读操作 | 阻塞/非阻塞读取 |
| IPC-01.4 | 管道写操作 | 写入环形缓冲区 |
| IPC-01.5 | 管道关闭处理 | 写端关闭时读返回 EOF |
| IPC-01.6 | 管道缓冲区管理 | 4KB 环形缓冲区 |

**文件**:
- `kernel/ipc/pipe.c` - 管道实现
- `kernel/ipc/pipe.h` - 管道接口

**数据结构**:
```c
#define PIPE_BUF_SIZE 4096

typedef struct pipe {
    char buffer[PIPE_BUF_SIZE];
    size_t read_pos;
    size_t write_pos;
    size_t count;           /* 缓冲区中的数据量 */
    int readers;            /* 读端引用计数 */
    int writers;            /* 写端引用计数 */
    /* 等待队列 (可选，用于阻塞) */
} pipe_t;
```

**验证**:
```c
int fd[2];
pipe(fd);
if (fork() == 0) {
    close(fd[0]);
    write(fd[1], "hello", 5);
    exit(0);
}
close(fd[1]);
char buf[10];
read(fd[0], buf, 5);  // buf = "hello"
```

---

### IPC-02: 信号基础框架
**优先级**: P0
**依赖**: P-02 (进程状态)

**描述**: 实现信号传递和处理的基础框架。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| IPC-02.1 | 信号位图 | PCB 中添加 pending/blocked 位图 |
| IPC-02.2 | kill() 系统调用 | 向进程发送信号 |
| IPC-02.3 | 默认信号处理 | SIGKILL 终止进程 |
| IPC-02.4 | 信号检查点 | 从内核返回用户态时检查 |
| IPC-02.5 | raise() 实现 | 向自己发送信号 |

**信号定义**:
```c
#define SIGHUP    1     /* 终端挂起 */
#define SIGINT    2     /* 中断 (Ctrl+C) */
#define SIGQUIT   3     /* 退出 */
#define SIGKILL   9     /* 强制终止 (不可捕获) */
#define SIGSEGV   11    /* 段错误 */
#define SIGPIPE   13    /* 管道破裂 */
#define SIGTERM   15    /* 终止请求 */
#define SIGCHLD   17    /* 子进程状态改变 */
#define SIGSTOP   19    /* 停止 (不可捕获) */
#define SIGCONT   18    /* 继续 */
```

**验证**:
```c
kill(child_pid, SIGKILL);  // 子进程被终止
```

---

### IPC-03: 用户态信号处理
**优先级**: P1
**依赖**: IPC-02

**描述**: 允许用户程序注册自定义信号处理函数。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| IPC-03.1 | signal() 系统调用 | 注册信号处理函数 |
| IPC-03.2 | sigaction() 系统调用 | 高级信号处理 |
| IPC-03.3 | 信号栈切换 | 切换到用户态执行处理函数 |
| IPC-03.4 | sigreturn() 系统调用 | 从信号处理函数返回 |
| IPC-03.5 | 信号屏蔽 | sigprocmask() |

**验证**:
```c
void handler(int sig) {
    printf("Caught signal %d\n", sig);
}
signal(SIGINT, handler);
raise(SIGINT);  // 输出 "Caught signal 2"
```

---

### IPC-04: 共享内存
**优先级**: P2
**依赖**: M-05 (用户空间内存映射)

**描述**: 实现进程间共享内存区域。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| IPC-04.1 | 共享内存对象 | shmget() 创建共享内存 |
| IPC-04.2 | 映射到进程 | shmat() 映射到地址空间 |
| IPC-04.3 | 解除映射 | shmdt() 解除映射 |
| IPC-04.4 | 引用计数 | 最后一个进程退出时释放 |

**简化实现** (可选使用 mmap):
```c
// 使用 mmap + MAP_SHARED 代替 System V 共享内存
void *ptr = mmap(NULL, 4096, PROT_READ|PROT_WRITE,
                 MAP_SHARED|MAP_ANONYMOUS, -1, 0);
```

**验证**:
```c
// 父子进程共享内存
int *shared = mmap(..., MAP_SHARED|MAP_ANONYMOUS, ...);
*shared = 0;
if (fork() == 0) {
    *shared = 42;
    exit(0);
}
wait(NULL);
assert(*shared == 42);  // 子进程修改对父进程可见
```

---

### IPC-05: Unix 域套接字 (可选)
**优先级**: P3
**依赖**: IPC-01, FS-01

**描述**: 实现本地进程间的套接字通信。

**子任务**:
| ID | 任务 | 验证方式 |
|----|------|----------|
| IPC-05.1 | socketpair() | 创建已连接的套接字对 |
| IPC-05.2 | 流式套接字 | SOCK_STREAM 支持 |
| IPC-05.3 | 数据报套接字 | SOCK_DGRAM 支持 |

**验证**:
```c
int sv[2];
socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
// 双向通信
```

---

## 开发顺序

```
IPC-01 (管道) ────────────────────┐
                                  │
IPC-02 (信号基础) ──> IPC-03 (用户态信号处理)
                                  │
IPC-04 (共享内存) ────────────────┤
                                  v
                          IPC-05 (Unix 域套接字)
```

建议顺序:
1. **IPC-01 管道** - 最常用的 IPC 机制
2. **IPC-02 信号基础** - SIGKILL/SIGCHLD 是必需的
3. **IPC-03 用户态信号** - signal() 处理
4. **IPC-04 共享内存** - 可选
5. **IPC-05 Unix 套接字** - 可选

---

## 系统调用清单

| 系统调用 | 功能 | 优先级 |
|----------|------|--------|
| pipe | 创建管道 | P0 |
| pipe2 | 创建管道 (带 flags) | P1 |
| kill | 发送信号 | P0 |
| signal | 注册信号处理 | P1 |
| sigaction | 高级信号处理 | P2 |
| sigprocmask | 信号屏蔽 | P2 |
| sigreturn | 信号处理返回 | P1 |
| shmget | 创建共享内存 | P2 |
| shmat | 映射共享内存 | P2 |
| shmdt | 解除映射 | P2 |
| socketpair | 创建套接字对 | P3 |

---

## 验证里程碑

| 编号 | 里程碑 | 验证方式 |
|------|--------|----------|
| M7.1 | 管道工作 | `ls | cat` 正常执行 |
| M7.2 | 信号传递 | `kill -9 <pid>` 终止进程 |
| M7.3 | 信号处理 | Ctrl+C 中断程序 |
| M7.4 | 共享内存 | 父子进程数据共享 |

**最终验证**:
```
$ ls /bin | cat
cat
echo
hello
init
ls
mkdir
pwd
sh

$ sleep 100 &
[1] 5
$ kill 5
[1] Terminated

$ # Ctrl+C 测试
$ cat
^C
$
```

---

## 技术注意事项

### 管道实现要点
1. 使用环形缓冲区避免数据拷贝
2. 读写需要同步 (自旋锁或禁中断)
3. 写端全关闭时，读返回 0 (EOF)
4. 读端全关闭时，写产生 SIGPIPE

### 信号实现要点
1. 信号在从内核返回用户态时检查
2. 需要保存/恢复用户态寄存器上下文
3. SIGKILL 和 SIGSTOP 不可捕获或忽略
4. 信号处理函数运行在用户栈上

### 共享内存要点
1. 物理页在所有映射进程间共享
2. 需要引用计数管理生命周期
3. COW (Copy-on-Write) fork 后需要特殊处理

---

## 交付物清单

- [x] `kernel/ipc/pipe.c` - 管道实现
- [x] `kernel/ipc/pipe.h` - 管道接口
- [x] `kernel/ipc/signal.c` - 信号实现
- [x] `kernel/ipc/signal.h` - 信号定义和接口
- [x] `kernel/ipc/shm.c` - 共享内存 (mmap实现)
- [x] `kernel/ipc/shm.h` - 共享内存接口
- [x] `libc/include/signal.h` - 用户态信号头文件
- [x] `libc/include/sys/mman.h` - mmap 头文件
- [x] `libc/src/signal.c` - libc 信号函数
- [x] `libc/src/mman.c` - libc mmap 函数
- [x] `libc/src/pipe.c` - libc pipe 函数
- [x] 更新 `kernel/proc/syscall.c` - 新增 IPC 系统调用
- [x] 更新 shell 支持管道语法 `|` 和 kill 命令

## 实现状态

| 任务 | 状态 | 说明 |
|------|------|------|
| IPC-01 管道 | ✅ 完成 | pipe(), 环形缓冲区, 阻塞读写 |
| IPC-02 信号基础 | ✅ 完成 | kill(), SIGKILL/SIGTERM |
| IPC-03 用户态信号 | ✅ 完成 | signal(), sigaction(), sigprocmask() |
| IPC-04 共享内存 | ✅ 完成 | mmap(MAP_ANONYMOUS), munmap() |
| Shell 管道 | ✅ 完成 | cmd1 \| cmd2 语法支持 |

## 构建说明

```bash
# 构建完整系统
make full

# 运行
make run

# 测试管道
$ ls | cat
$ echo hello | cat
```

---

## Shell 管道支持

需要更新 `userspace/shell/sh.c` 支持管道语法:

```c
// 解析 "cmd1 | cmd2"
// 1. 创建管道
// 2. fork 第一个子进程，stdout 重定向到管道写端
// 3. fork 第二个子进程，stdin 重定向到管道读端
// 4. 等待两个子进程完成
```

伪代码:
```c
int fd[2];
pipe(fd);

if (fork() == 0) {
    dup2(fd[1], 1);  // stdout -> pipe write
    close(fd[0]);
    close(fd[1]);
    exec(cmd1);
}

if (fork() == 0) {
    dup2(fd[0], 0);  // stdin <- pipe read
    close(fd[0]);
    close(fd[1]);
    exec(cmd2);
}

close(fd[0]);
close(fd[1]);
wait(NULL);
wait(NULL);
```
