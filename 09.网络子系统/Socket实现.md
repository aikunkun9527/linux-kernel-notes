---
tags:
  - Linux
  - Kernel
  - 网络
  - Socket
created: 2026-03-15
status: 学习中
---

# Socket实现

> [!info] 目标
> 理解Linux内核Socket的内部实现机制

> [!note] 网络子系统完整笔记
> 本笔记详细介绍Socket实现，总览请参考 [[Linux网络子系统|MOC]]

## Socket概述

### 用户态API

```c
// 创建socket
int socket(int domain, int type, int protocol);

// 绑定地址
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

// 监听连接
int listen(int sockfd, int backlog);

// 接受连接
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

// 发起连接
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

// 发送/接收数据
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);

// 关闭
int close(int fd);
```

### 协议族与类型

```c
// 协议族 (domain)
#define AF_UNIX     1   // Unix域套接字
#define AF_INET     2   // IPv4
#define AF_INET6    10  // IPv6
#define AF_NETLINK  16  // Netlink
#define AF_PACKET   17  // 原始套接字

// 套接字类型 (type)
#define SOCK_STREAM     1   // 面向连接 (TCP)
#define SOCK_DGRAM      2   // 数据报 (UDP)
#define SOCK_RAW        3   // 原始套接字
#define SOCK_SEQPACKET  5   // 有序数据包

// 协议 (protocol)
#define IPPROTO_TCP     6   // TCP
#define IPPROTO_UDP     17  // UDP
#define IPPROTO_RAW     255 // 原始IP
```

## 核心数据结构

### socket结构

```c
// include/linux/net.h
struct socket {
    socket_state        state;      // 套接字状态

    short               type;       // SOCK_STREAM, SOCK_DGRAM等

    unsigned long       flags;      // 套接字标志

    const struct proto_ops *ops;    // 操作函数表

    struct sock         *sk;        // 内核sock结构

    struct file         *file;      // 关联的文件结构

    struct socket_wq    wq;         // 等待队列
};

// 套接字状态
typedef enum {
    SS_UNCONNECTED,      // 未连接
    SS_CONNECTING,       // 连接中
    SS_CONNECTED,        // 已连接
    SS_DISCONNECTING     // 断开中
} socket_state;
```

### sock结构

```c
// include/net/sock.h
struct sock {
    struct sock_common  __sk_common;
#define sk_state        __sk_common.skc_state
#define sk_family       __sk_common.skc_family
#define sk_protocol     __sk_common.skc_protocol
#define sk_daddr        __sk_common.skc_daddr
#define sk_rcv_saddr    __sk_common.skc_rcv_saddr
#define sk_dport        __sk_common.skc_dport

    // 锁
    socket_lock_t       sk_lock;

    // 接收/发送队列
    struct sk_buff_head sk_receive_queue;
    struct sk_buff_head sk_write_queue;

    // 缓冲区大小
    int                 sk_rcvbuf;      // 接收缓冲区大小
    int                 sk_sndbuf;      // 发送缓冲区大小

    // 回调
    void                (*sk_state_change)(struct sock *sk);
    void                (*sk_data_ready)(struct sock *sk);
    void                (*sk_write_space)(struct sock *sk);
    void                (*sk_error_report)(struct sock *sk);

    // 协议操作
    struct proto        *sk_prot;

    // 关联的socket
    struct socket       *sk_socket;

    // 引用计数
    atomic_t            sk_refcnt;

    // ...
};
```

### proto_ops

```c
struct proto_ops {
    int     family;
    struct module *owner;

    int     (*release)(struct socket *sock);
    int     (*bind)(struct socket *sock, struct sockaddr *myaddr,
                    int sockaddr_len);
    int     (*connect)(struct socket *sock, struct sockaddr *vaddr,
                       int sockaddr_len, int flags);
    int     (*socketpair)(struct socket *sock1, struct socket *sock2);
    int     (*accept)(struct socket *sock, struct socket *newsock,
                      int flags, bool kern);
    int     (*getname)(struct socket *sock, struct sockaddr *addr,
                       int peer);
    __poll_t (*poll)(struct file *file, struct socket *sock,
                     struct poll_table_struct *wait);
    int     (*ioctl)(struct socket *sock, unsigned int cmd,
                     unsigned long arg);
    int     (*listen)(struct socket *sock, int len);
    int     (*shutdown)(struct socket *sock, int flags);
    int     (*setsockopt)(struct socket *sock, int level, int optname,
                          char __user *optval, unsigned int optlen);
    int     (*getsockopt)(struct socket *sock, int level, int optname,
                          char __user *optval, int __user *optlen);
    int     (*sendmsg)(struct socket *sock, struct msghdr *m,
                       size_t total_len);
    int     (*recvmsg)(struct socket *sock, struct msghdr *m,
                       size_t total_len, int flags);
    // ...
};
```

### proto (协议操作)

```c
struct proto {
    void       (*close)(struct sock *sk, long timeout);
    int        (*connect)(struct sock *sk, struct sockaddr *uaddr,
                          int addr_len);
    int        (*disconnect)(struct sock *sk, int flags);

    struct sock *(*accept)(struct sock *sk, int flags, int *err, bool kern);

    int        (*ioctl)(struct sock *sk, int cmd, unsigned long arg);
    int        (*init)(struct sock *sk);
    void       (*destroy)(struct sock *sk);
    int        (*setsockopt)(struct sock *sk, int level, int optname,
                             char __user *optval, unsigned int optlen);
    int        (*getsockopt)(struct sock *sk, int level, int optname,
                             char __user *optval, int __user *option);
    int        (*sendmsg)(struct sock *sk, struct msghdr *msg,
                          size_t len);
    int        (*recvmsg)(struct sock *sk, struct msghdr *msg,
                          size_t len, int noblock, int flags,
                          int *addr_len);
    int        (*bind)(struct sock *sk, struct sockaddr *uaddr,
                       int addr_len);

    // ...
    char       name[32];
    struct module *owner;
};
```

## socket系统调用流程

### socket()创建

```
socket(AF_INET, SOCK_STREAM, 0)
         │
         ▼
┌─────────────────────────────────────────────────────────────┐
│ SYSCALL_DEFINE3(socket, int, family, int, type, int, protocol)│
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ sock_create(family, type, protocol, &sock)                  │
│   - 检查参数                                                │
│   - 分配socket结构                                          │
│   - 调用协议族的create函数                                  │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ inet_create() (AF_INET协议族)                               │
│   - 查找协议 (TCP/UDP)                                      │
│   - 分配sock结构                                            │
│   - 初始化sock                                              │
│   - 关联socket和sock                                        │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ sock_map_fd(sock)                                           │
│   - 分配文件描述符                                           │
│   - 创建file结构                                            │
│   - 关联socket和file                                        │
└─────────────────────────────────────────────────────────────┘
```

### bind()绑定

```c
// net/socket.c
SYSCALL_DEFINE3(bind, int, fd, struct sockaddr __user *, umyaddr, int, addrlen)
{
    struct socket *sock;
    struct sockaddr_storage address;
    int err;

    // 根据fd查找socket
    sock = sockfd_lookup(fd, &err);
    if (!sock)
        return err;

    // 复制地址到内核
    err = move_addr_to_kernel(umyaddr, addrlen, (struct sockaddr *)&address);
    if (err < 0)
        goto out_put;

    // 调用协议族的bind
    err = sock->ops->bind(sock, (struct sockaddr *)&address, addrlen);

out_put:
    sockfd_put(sock);
    return err;
}

// inet_bind() (AF_INET)
int inet_bind(struct socket *sock, struct sockaddr *uaddr, int addr_len)
{
    struct sock *sk = sock->sk;
    struct sockaddr_in *addr = (struct sockaddr_in *)uaddr;

    // 检查地址长度
    if (addr_len < sizeof(struct sockaddr_in))
        return -EINVAL;

    // 检查是否已绑定
    if (sk->sk_state != TCP_CLOSE || inet_sk(sk)->inet_num)
        return -EINVAL;

    // 设置IP地址
    inet_sk(sk)->inet_rcv_saddr = inet_sk(sk)->inet_saddr = addr->sin_addr.s_addr;

    // 设置端口
    if (addr->sin_port) {
        // 检查端口是否可用
        err = inet_sk_get_port(sk, ntohs(addr->sin_port));
        if (err)
            return err;
    }

    return 0;
}
```

### listen()监听

```c
// inet_listen()
int inet_listen(struct socket *sock, int backlog)
{
    struct sock *sk = sock->sk;
    unsigned char old_state;
    int err;

    lock_sock(sk);

    // 检查状态
    if (sock->state != SS_UNCONNECTED || sock->type != SOCK_STREAM)
        return -EINVAL;

    old_state = sk->sk_state;
    if (!((1 << old_state) & (TCPF_CLOSE | TCPF_LISTEN)))
        return -EINVAL;

    // 如果还没在监听，转换为LISTEN状态
    if (old_state != TCP_LISTEN) {
        err = inet_csk_listen_start(sk);
        if (err)
            goto out;
    }

    // 设置backlog
    sk->sk_max_ack_backlog = backlog;

out:
    release_sock(sk);
    return 0;
}
```

### accept()接受连接

```c
// inet_accept()
int inet_accept(struct socket *sock, struct socket *newsock, int flags, bool kern)
{
    struct sock *sk1 = sock->sk;
    struct sock *sk2;
    int err;

    // 从监听队列取出新连接
    sk2 = sk1->sk_prot->accept(sk1, flags, &err, kern);
    if (!sk2)
        return err;

    // 关联新socket和新sock
    sock_graft(sk2, newsock);
    newsock->state = SS_CONNECTED;

    return 0;
}

// inet_csk_accept() (TCP的accept)
struct sock *inet_csk_accept(struct sock *sk, int flags, int *err, bool kern)
{
    struct inet_connection_sock *icsk = inet_csk(sk);
    struct request_sock_queue *queue = &icsk->icsk_accept_queue;
    struct request_sock *req;
    struct sock *newsk;

    lock_sock(sk);

    // 等待新连接
    if (reqsk_queue_empty(queue)) {
        long timeo = sock_rcvtimeo(sk, flags & O_NONBLOCK);
        if (!timeo) {
            *err = -EAGAIN;
            goto out_err;
        }

        *err = inet_csk_wait_for_connect(sk, timeo);
        if (*err)
            goto out_err;
    }

    // 从队列取出请求
    req = reqsk_queue_remove(queue, sk);
    newsk = req->sk;

    release_sock(sk);
    return newsk;
}
```

### connect()发起连接

```c
// inet_stream_connect()
int inet_stream_connect(struct socket *sock, struct sockaddr *uaddr,
                        int addr_len, int flags)
{
    struct sock *sk = sock->sk;
    int err;

    lock_sock(sk);

    switch (sock->state) {
    case SS_UNCONNECTED:
        err = -EISCONN;
        if (sk->sk_state != TCP_CLOSE)
            goto out;

        err = sk->sk_prot->connect(sk, uaddr, addr_len);
        if (err < 0)
            goto out;

        sock->state = SS_CONNECTING;
        err = -EINPROGRESS;
        break;

    case SS_CONNECTING:
        err = -EALREADY;
        break;

    case SS_CONNECTED:
        err = -EISCONN;
        break;

    default:
        err = -EINVAL;
        break;
    }

out:
    release_sock(sk);
    return err;
}
```

### sendmsg/recvmsg

```c
// 发送数据
int sock_sendmsg(struct socket *sock, struct msghdr *msg)
{
    struct sockaddr_storage *address = NULL;
    int ret;

    // 处理目标地址
    if (msg->msg_name) {
        address = (struct sockaddr_storage *)msg->msg_name;
    }

    // 安全检查
    ret = security_socket_sendmsg(sock, msg, msg_data_left(msg));
    if (ret)
        return ret;

    // 调用协议的sendmsg
    return sock->ops->sendmsg(sock, msg, msg_data_left(msg));
}

// 接收数据
int sock_recvmsg(struct socket *sock, struct msghdr *msg, int flags)
{
    int ret;

    // 安全检查
    ret = security_socket_recvmsg(sock, msg, msg_data_left(msg), flags);
    if (ret)
        return ret;

    // 调用协议的recvmsg
    return sock->ops->recvmsg(sock, msg, msg_data_left(msg), flags);
}
```

## TCP状态机

```
TCP连接状态转换图：

                              +---------+
                ----------->| CLOSED  |
                |           +---------+
                |               | application: active open
                |               | send: SYN
                |               V
                |           +---------+
                |           | SYN_SENT|-------------------+
                |           +---------+                   |
                |               |                         |
                |               | recv: SYN+ACK           |
                |               | send: ACK               |
                |               V                         |
                |           +---------+                   |
      application:   +----->|ESTABLISHED|<-------------+
      passive open   |      +---------+                |
      send: SYN,ACK  |           |                     |
                |    |           | application: close  |
                |    |           | send: FIN           |
                |    |           V                     |
                |    |      +---------+                |
                |    |      |FIN_WAIT1|                |
                |    |      +---------+                |
                |    |           |                     |
                |    |           | recv: ACK           |
                |    |           V                     |
                |    |      +---------+                |
                |    |      |FIN_WAIT2|                |
                |    |      +---------+                |
                |    |           | recv: FIN           |
                |    |           | send: ACK           |
                |    |           V                     |
                |    |      +---------+                |
                |    +------| TIME_WAIT|--------------+
                |           +---------+               |
                |               | 2MSL timeout         |
                |               V                      |
                |           +---------+               |
                +---------->| CLOSED  |<--------------+
                            +---------+
```

## 常用查看命令

```bash
# 查看socket统计
cat /proc/net/sockstat
# sockets: used 1234
# TCP: inuse 56 orphan 0 tw 12 alloc 78 mem 5
# UDP: inuse 34 mem 2

# 查看TCP连接
cat /proc/net/tcp
ss -tan

# 查看UDP连接
cat /proc/net/udp
ss -uan

# 查看Unix域套接字
cat /proc/net/unix

# 查看网络参数
sysctl -a | grep net

# socket缓冲区
cat /proc/sys/net/ipv4/tcp_rmem
cat /proc/sys/net/ipv4/tcp_wmem
```

## 相关笔记

- [[网络协议栈概述]]
- [[Netfilter框架]]
- [[Linux网络子系统]]
