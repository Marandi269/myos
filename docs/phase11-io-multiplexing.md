# Phase 11: I/O 多路复用 (I/O Multiplexing)

## 目标
实现 select/poll 机制，支持单线程同时监控多个文件描述符。

## 前置依赖
- ✅ Phase 5: 文件系统 (VFS, 文件描述符)
- ✅ Phase 7: IPC (管道)
- ✅ Phase 8: 网络栈 (Socket)

---

## 设计思路

### 核心概念

I/O 多路复用允许进程同时等待多个 fd 就绪，而不是阻塞在单个 fd 上。

```
传统阻塞 I/O:
  read(fd1) → 阻塞 → 返回 → read(fd2) → 阻塞 → ...

I/O 多路复用:
  select([fd1, fd2, fd3]) → 等待任意就绪 → 处理就绪的 fd
```

### 实现层次

```
┌─────────────────────────────────────────┐
│           用户态 API                     │
│   select() / poll() / epoll_*()         │
├─────────────────────────────────────────┤
│           系统调用层                     │
│   sys_select / sys_poll / sys_epoll_*   │
├─────────────────────────────────────────┤
│           等待队列机制                   │
│   wait_queue_t + poll_table             │
├─────────────────────────────────────────┤
│        VFS poll 回调                    │
│   file->f_op->poll()                    │
├─────────────────────────────────────────┤
│        各设备/Socket 实现               │
│   pipe_poll, socket_poll, tty_poll      │
└─────────────────────────────────────────┘
```

---

## 任务列表

### IO-01: 等待队列基础设施
**优先级**: P0

**描述**: 实现内核等待队列，用于进程睡眠和唤醒。

**数据结构**:
```c
/* 等待队列头 */
typedef struct wait_queue_head {
    struct list_head task_list;
} wait_queue_head_t;

/* 等待队列项 (每个等待的进程一个) */
typedef struct wait_queue_entry {
    struct process *task;
    struct list_head list;
} wait_queue_entry_t;

/* 初始化等待队列 */
void init_waitqueue_head(wait_queue_head_t *wq);

/* 将当前进程加入等待队列并睡眠 */
void wait_event(wait_queue_head_t *wq, int condition);

/* 唤醒等待队列中的进程 */
void wake_up(wait_queue_head_t *wq);
```

**文件**:
- `kernel/proc/wait_queue.c/h`

---

### IO-02: poll 表机制
**优先级**: P0

**描述**: 实现 poll_table，用于收集多个 fd 的等待队列。

**数据结构**:
```c
/* poll 表 - 收集所有需要等待的队列 */
typedef struct poll_table {
    /* 将等待队列加入 poll 表的回调 */
    void (*queue_proc)(struct poll_table *pt, wait_queue_head_t *wq);
    /* 内部数据 */
    void *private;
} poll_table_t;

/* poll 返回的事件掩码 */
#define POLLIN      0x0001   /* 可读 */
#define POLLOUT     0x0004   /* 可写 */
#define POLLERR     0x0008   /* 错误 */
#define POLLHUP     0x0010   /* 挂起 */
#define POLLNVAL    0x0020   /* 无效 fd */

/* VFS 层的 poll 接口 */
typedef unsigned int (*poll_fn_t)(struct file *file, poll_table_t *pt);
```

**核心逻辑**:
```c
/* select/poll 的核心循环 */
int do_poll(struct pollfd *fds, int nfds, int timeout) {
    poll_table_t pt;
    int count = 0;

    while (1) {
        /* 遍历所有 fd，调用其 poll 方法 */
        for (int i = 0; i < nfds; i++) {
            struct file *file = get_file(fds[i].fd);
            if (file && file->f_op->poll) {
                int events = file->f_op->poll(file, &pt);
                if (events & fds[i].events) {
                    fds[i].revents = events;
                    count++;
                }
            }
        }

        /* 有就绪的 fd，返回 */
        if (count > 0) break;

        /* 超时，返回 */
        if (timeout == 0) break;

        /* 睡眠等待唤醒 */
        schedule_timeout(timeout);
    }

    return count;
}
```

---

### IO-03: select 系统调用
**优先级**: P1

**描述**: 实现 POSIX select() 系统调用。

**接口**:
```c
int select(int nfds, fd_set *readfds, fd_set *writefds,
           fd_set *exceptfds, struct timeval *timeout);

/* fd_set 操作宏 */
void FD_ZERO(fd_set *set);
void FD_SET(int fd, fd_set *set);
void FD_CLR(int fd, fd_set *set);
int  FD_ISSET(int fd, fd_set *set);
```

**实现要点**:
```c
/* fd_set 实现 (位图) */
#define FD_SETSIZE 64

typedef struct fd_set {
    unsigned long bits[FD_SETSIZE / (8 * sizeof(unsigned long))];
} fd_set;

/* sys_select 实现 */
int64_t sys_select(int nfds, fd_set *readfds, fd_set *writefds,
                   fd_set *exceptfds, struct timeval *timeout) {
    struct pollfd pfd[FD_SETSIZE];
    int npfd = 0;

    /* 将 fd_set 转换为 pollfd 数组 */
    for (int fd = 0; fd < nfds; fd++) {
        int events = 0;
        if (readfds && FD_ISSET(fd, readfds))   events |= POLLIN;
        if (writefds && FD_ISSET(fd, writefds)) events |= POLLOUT;
        if (events) {
            pfd[npfd].fd = fd;
            pfd[npfd].events = events;
            npfd++;
        }
    }

    /* 调用内部 poll */
    int ret = do_poll(pfd, npfd, timeout_ms);

    /* 将结果转换回 fd_set */
    if (readfds)  FD_ZERO(readfds);
    if (writefds) FD_ZERO(writefds);
    for (int i = 0; i < npfd; i++) {
        if (pfd[i].revents & POLLIN)  FD_SET(pfd[i].fd, readfds);
        if (pfd[i].revents & POLLOUT) FD_SET(pfd[i].fd, writefds);
    }

    return ret;
}
```

---

### IO-04: poll 系统调用
**优先级**: P1

**描述**: 实现 POSIX poll() 系统调用。

**接口**:
```c
struct pollfd {
    int   fd;       /* 文件描述符 */
    short events;   /* 请求的事件 */
    short revents;  /* 返回的事件 */
};

int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```

**实现**:
```c
int64_t sys_poll(struct pollfd *fds, uint64_t nfds, int timeout) {
    /* 直接调用 do_poll */
    return do_poll(fds, nfds, timeout);
}
```

---

### IO-05: 各设备 poll 实现
**优先级**: P1

**描述**: 为管道、Socket、控制台等实现 poll 回调。

**管道 poll**:
```c
unsigned int pipe_poll(struct file *file, poll_table_t *pt) {
    pipe_t *pipe = file->private_data;
    unsigned int mask = 0;

    /* 注册等待队列 */
    poll_wait(file, &pipe->wait_queue, pt);

    /* 检查状态 */
    if (file->f_mode & FMODE_READ) {
        if (pipe->count > 0 || pipe->writers == 0)
            mask |= POLLIN;   /* 可读或写端已关闭 */
    }
    if (file->f_mode & FMODE_WRITE) {
        if (pipe->count < PIPE_BUF_SIZE)
            mask |= POLLOUT;  /* 可写 */
    }

    return mask;
}
```

**Socket poll**:
```c
unsigned int socket_poll(struct file *file, poll_table_t *pt) {
    socket_t *sock = file->private_data;
    unsigned int mask = 0;

    poll_wait(file, &sock->wait_queue, pt);

    /* TCP: 检查接收缓冲区 */
    if (sock->recv_buf_len > 0)
        mask |= POLLIN;

    /* TCP: 检查发送缓冲区 */
    if (sock->send_buf_len < SOCK_BUF_SIZE)
        mask |= POLLOUT;

    /* TCP: 检查连接状态 */
    if (sock->state == TCP_CLOSE)
        mask |= POLLHUP;

    return mask;
}
```

**控制台 poll**:
```c
unsigned int console_poll(struct file *file, poll_table_t *pt) {
    unsigned int mask = POLLOUT;  /* 总是可写 */

    poll_wait(file, &keyboard_wait_queue, pt);

    if (keyboard_buffer_count > 0)
        mask |= POLLIN;  /* 有键盘输入 */

    return mask;
}
```

---

### IO-06: epoll (可选，高级)
**优先级**: P2

**描述**: 实现 Linux epoll 接口，适合大量连接。

**接口**:
```c
int epoll_create(int size);
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

struct epoll_event {
    uint32_t events;   /* EPOLLIN, EPOLLOUT, etc. */
    epoll_data_t data; /* 用户数据 */
};

/* 操作类型 */
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3
```

**简化实现思路**:
```c
/* epoll 实例 */
typedef struct epoll {
    struct list_head fd_list;    /* 监控的 fd 链表 */
    wait_queue_head_t wait;      /* 等待队列 */
} epoll_t;

/* 监控的 fd 项 */
typedef struct epoll_item {
    int fd;
    uint32_t events;
    epoll_data_t data;
    struct list_head list;
} epoll_item_t;
```

**epoll vs select/poll 优势**:
- select/poll 每次调用需要遍历所有 fd
- epoll 使用回调机制，只返回就绪的 fd
- 适合监控大量 fd (如网络服务器)

---

## 开发顺序

```
IO-01 (等待队列) → IO-02 (poll 表)
                        │
            ┌───────────┼───────────┐
            v           v           v
      IO-03 (select) IO-04 (poll) IO-05 (设备 poll)
                        │
                        v
                  IO-06 (epoll)
```

**建议实现顺序**:
1. IO-01 + IO-02: 基础设施
2. IO-04: poll (比 select 接口更简洁)
3. IO-05: 管道和 Socket 的 poll
4. IO-03: select (基于 poll 实现)
5. IO-06: epoll (可选)

---

## 验证测试

### 测试 1: 管道 + select
```c
int pipefd[2];
pipe(pipefd);

if (fork() == 0) {
    sleep(1);
    write(pipefd[1], "hello", 5);
    exit(0);
}

fd_set readfds;
FD_ZERO(&readfds);
FD_SET(pipefd[0], &readfds);

printf("Waiting for data...\n");
int ret = select(pipefd[0] + 1, &readfds, NULL, NULL, NULL);
printf("select returned %d\n", ret);

if (FD_ISSET(pipefd[0], &readfds)) {
    char buf[10];
    read(pipefd[0], buf, 5);
    printf("Read: %s\n", buf);
}
```

### 测试 2: 多 Socket + poll
```c
struct pollfd fds[2];
fds[0].fd = sock1;
fds[0].events = POLLIN;
fds[1].fd = sock2;
fds[1].events = POLLIN;

int ret = poll(fds, 2, 5000);  /* 5秒超时 */

for (int i = 0; i < 2; i++) {
    if (fds[i].revents & POLLIN) {
        printf("Socket %d ready\n", fds[i].fd);
        recv(fds[i].fd, buf, sizeof(buf), 0);
    }
}
```

---

## 交付物清单

- [ ] `kernel/proc/wait_queue.c/h` - 等待队列
- [ ] `kernel/fs/poll.c/h` - poll 核心实现
- [ ] `kernel/fs/select.c` - select 实现
- [ ] 更新 `kernel/ipc/pipe.c` - 添加 pipe_poll
- [ ] 更新 `kernel/net/socket.c` - 添加 socket_poll
- [ ] 更新 `kernel/proc/syscall.c` - 新增系统调用
- [ ] `libc/include/sys/select.h` - select 头文件
- [ ] `libc/include/poll.h` - poll 头文件
- [ ] `libc/src/select.c` - libc select
- [ ] `libc/src/poll.c` - libc poll

---

## 简化版实现 (最小可用)

如果只需要基本功能，可以实现简化版：

```c
/* 简化版 poll - 轮询实现 */
int simple_poll(struct pollfd *fds, int nfds, int timeout) {
    uint64_t deadline = get_ticks() + timeout * TICKS_PER_MS;
    int count = 0;

    while (1) {
        count = 0;
        for (int i = 0; i < nfds; i++) {
            struct file *f = get_file(fds[i].fd);
            if (!f) {
                fds[i].revents = POLLNVAL;
                continue;
            }

            fds[i].revents = 0;

            /* 检查管道 */
            if (f->type == FILE_PIPE) {
                pipe_t *p = f->private;
                if (p->count > 0) fds[i].revents |= POLLIN;
                if (p->count < PIPE_BUF_SIZE) fds[i].revents |= POLLOUT;
            }

            /* 检查 socket */
            if (f->type == FILE_SOCKET) {
                socket_t *s = f->private;
                if (s->recv_len > 0) fds[i].revents |= POLLIN;
                if (s->send_len < BUF_SIZE) fds[i].revents |= POLLOUT;
            }

            if (fds[i].revents & fds[i].events) count++;
        }

        if (count > 0) return count;
        if (timeout == 0) return 0;
        if (get_ticks() >= deadline) return 0;

        yield();  /* 让出 CPU */
    }
}
```

这个简化版使用轮询而非睡眠/唤醒，效率较低但实现简单，适合初期验证。
