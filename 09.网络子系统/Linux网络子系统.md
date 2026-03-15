---
tags:
  - MOC
  - Linux
  - Kernel
  - 网络
created: 2026-03-15
status: 学习中
---

# Linux网络子系统

> [!info] 概述
> Linux网络子系统实现了完整的TCP/IP协议栈，是内核中最复杂的子系统之一

## 🧠 核心概念

### 网络协议栈架构

```
┌─────────────────────────────────────────────────────────────┐
│                     用户空间应用                             │
│           socket(), bind(), listen(), accept()              │
│           connect(), send(), recv()                         │
├─────────────────────────────────────────────────────────────┤
│                     系统调用层                              │
│              sock_sendmsg(), sock_recvmsg()                 │
├─────────────────────────────────────────────────────────────┤
│                     Socket层                                │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ struct socket, struct sock                          │   │
│  │ INET, UNIX, NETLINK等协议族                         │   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│                     传输层                                  │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐                   │
│  │   TCP    │ │   UDP    │ │  SCTP    │  ...              │
│  └──────────┘ └──────────┘ └──────────┘                   │
├─────────────────────────────────────────────────────────────┤
│                     网络层                                  │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐                   │
│  │ IPv4     │ │ IPv6     │ │  ARP     │  ...              │
│  └──────────┘ └──────────┘ └──────────┘                   │
├─────────────────────────────────────────────────────────────┤
│                     数据链路层                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ 邻居子系统、流量控制、QoS                           │   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│                     设备驱动层                              │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐                   │
│  │   eth0   │ │   wlan0  │ │   lo     │  ...              │
│  └──────────┘ └──────────┘ └──────────┘                   │
├─────────────────────────────────────────────────────────────┤
│                     硬件层                                  │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐                   │
│  │   网卡   │ │   WiFi   │ │  回环    │  ...              │
│  └──────────┘ └──────────┘ └──────────┘                   │
└─────────────────────────────────────────────────────────────┘
```

### 核心数据结构关系

```
socket (用户可见)
    │
    └── sock (内核协议控制块)
           │
           ├── sk_buff (数据包缓冲区)
           │       │
           │       └── net_device (网络设备)
           │
           └── proto (协议操作)
```

## 📚 学习路径

### 第一层：协议栈概述
- [[OS/Linux/09.网络子系统/网络协议栈概述]] - 层次结构、数据流
- sk_buff与net_device

### 第二层：Socket实现
- [[OS/Linux/09.网络子系统/Socket实现]] - socket API、协议族

### 第三层：高级主题
- [[OS/Linux/09.网络子系统/Netfilter框架]] - 包过滤、NAT、防火墙
- 网络设备驱动

## 核心数据结构

### sk_buff (Socket Buffer)

```c
// 数据包的通用容器
struct sk_buff {
    // 链表指针
    struct sk_buff      *next, *prev;

    // 所属socket
    struct sock         *sk;

    // 时间戳
    ktime_t             tstamp;

    // 网络设备
    struct net_device   *dev;

    // 缓冲区指针
    unsigned char       *head;      // 缓冲区起始
    unsigned char       *data;      // 数据起始
    unsigned char       *tail;      // 数据结束
    unsigned char       *end;       // 缓冲区结束

    // 协议头偏移
    sk_buff_data_t      transport_header;  // 传输层
    sk_buff_data_t      network_header;    // 网络层
    sk_buff_data_t      mac_header;        // 链路层

    // 协议信息
    __be16              protocol;          // 协议类型
    __u16               transport_header;
    __u16               network_header;
    __u16               mac_header;

    // 长度
    unsigned int        len;        // 数据长度
    unsigned int        data_len;   // 分片数据长度
    // ...
};
```

### sk_buff布局

```
           head                data                 tail                 end
             │                   │                    │                    │
             ▼                   ▼                    ▼                    ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│  headroom  │       payload (数据)        │  tailroom  │     预留空间       │
└──────────────────────────────────────────────────────────────────────────────┘

协议头指针：
┌──────────────────────────────────────────────────────────────────────────────┐
│ MAC Header │  Network Header  │  Transport Header  │      Data              │
└──────────────────────────────────────────────────────────────────────────────┘
     ▲              ▲                     ▲
 mac_header   network_header      transport_header
```

### net_device

```c
struct net_device {
    char                name[IFNAMSIZ];    // 设备名 (eth0, wlan0)
    unsigned int        ifindex;           // 接口索引

    // 地址
    unsigned char       dev_addr[MAX_ADDR_LEN];  // MAC地址
    unsigned char       broadcast[MAX_ADDR_LEN]; // 广播地址

    // 配置
    unsigned int        mtu;               // 最大传输单元
    unsigned short      type;              // 接口类型
    unsigned short      hard_header_len;   // 硬件头长度

    // 标志
    unsigned int        flags;             // IFF_UP, IFF_PROMISC等

    // 操作函数
    const struct net_device_ops *netdev_ops;
    const struct ethtool_ops    *ethtool_ops;

    // 队列
    unsigned int        num_tx_queues;
    unsigned int        real_num_tx_queues;
    struct netdev_queue *_tx;

    struct netdev_queue rx_queue;

    // 统计
    struct net_device_stats stats;

    // NAPI
    struct napi_struct  napi;

    // ...
};
```

### socket与sock

```c
// 用户可见的socket结构
struct socket {
    socket_state        state;          // 状态
    short               type;           // 类型 (SOCK_STREAM等)
    unsigned long       flags;          // 标志

    const struct proto_ops *ops;        // 操作函数

    struct sock         *sk;            // 内核sock结构

    struct file         *file;          // 关联的文件
    // ...
};

// 内核协议控制块
struct sock {
    struct proto        *sk_prot;       // 协议操作
    struct socket       *sk_socket;     // 关联的socket

    int                 sk_state;       // TCP状态
    unsigned long       sk_flags;

    // 接收/发送缓冲区
    struct sk_buff_head sk_receive_queue;
    struct sk_buff_head sk_write_queue;

    // 地址
    struct sockaddr_in  sk_addr;

    // ...
};
```

## 数据包收发流程

### 接收流程详解

```
网卡接收数据包
     │
     ▼
┌─────────────────────────────────────────────────────────────┐
│ 硬件中断 (网卡中断)                                         │
│   - 网卡驱动中断处理函数                                    │
│   - napi_schedule() 调度NAPI轮询                           │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ NAPI轮询 (softirq: NET_RX_SOFTIRQ)                         │
│   - 网卡驱动的poll函数                                      │
│   - 批量处理多个数据包                                      │
│   - alloc_skb() 分配sk_buff                                │
│   - eth_type_trans() 设置协议类型                          │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ __netif_receive_skb() / netif_receive_skb()                │
│   - 数据包分发入口                                          │
│   - 处理各种packet_type (ARP, IP等)                        │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ IP层处理: ip_rcv()                                         │
│   - IP头校验                                                │
│   - Netfilter: NF_INET_PRE_ROUTING                         │
│   - 路由查找                                                │
│   - 本地交付或转发                                          │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ 传输层处理                                                  │
│   TCP: tcp_v4_rcv() → 找到sock → 放入接收队列              │
│   UDP: udp_rcv() → 找到sock → 放入接收队列                 │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ 应用层读取                                                  │
│   - 等待sock->sk_receive_queue非空                         │
│   - 复制数据到用户空间                                      │
│   - 释放sk_buff                                            │
└─────────────────────────────────────────────────────────────┘
```

### 发送流程详解

```
应用程序: send() / sendto() / write()
     │
     ▼
┌─────────────────────────────────────────────────────────────┐
│ 系统调用: sock_sendmsg()                                    │
│   - 复制用户数据到内核                                      │
│   - 分配并填充sk_buff                                       │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ Socket层处理                                                │
│   - 协议族操作 (inet_sendmsg)                               │
│   - TCP: tcp_sendmsg()                                      │
│   - UDP: udp_sendmsg()                                      │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ 传输层处理                                                  │
│   TCP: 构建TCP头、序列号、拥塞控制                         │
│   UDP: 构建UDP头                                            │
│   - Netfilter: NF_INET_LOCAL_OUT                           │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ IP层处理: ip_queue_xmit() / ip_local_out()                 │
│   - 构建IP头                                                │
│   - 路由查找                                                │
│   - 分片 (如需要)                                           │
│   - Netfilter: NF_INET_POST_ROUTING                        │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ 邻居子系统                                                  │
│   - ARP解析 (MAC地址)                                       │
│   - 构建以太网头                                            │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ dev_queue_xmit()                                            │
│   - 选择发送队列                                            │
│   - 流量控制 (QoS)                                          │
│   - 驱动ndo_start_xmit                                      │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│ 网卡驱动发送                                                │
│   - DMA传输到网卡                                           │
│   - 发送完成中断                                            │
│   - 释放sk_buff                                            │
└─────────────────────────────────────────────────────────────┘
```

## NAPI机制

### 为什么需要NAPI

```
传统中断模式：
每个数据包触发一次中断 → 中断风暴 → CPU被中断处理占用

NAPI (New API)：
首次中断 + 后续轮询 → 批量处理 → 减少中断开销
```

### NAPI使用

```c
// 初始化NAPI
struct napi_struct napi;
netif_napi_add(dev, &napi, my_poll, 64);  // weight=64
napi_enable(&napi);

// 中断处理中调度轮询
irqreturn_t my_interrupt(int irq, void *dev_id)
{
    struct net_device *dev = dev_id;
    struct my_priv *priv = netdev_priv(dev);

    // 禁用中断，切换到轮询模式
    disable_interrupts(dev);
    napi_schedule(&priv->napi);

    return IRQ_HANDLED;
}

// 轮询函数
int my_poll(struct napi_struct *napi, int budget)
{
    int work_done = 0;
    struct sk_buff *skb;

    while (work_done < budget && (skb = receive_packet())) {
        // 处理数据包
        netif_receive_skb(skb);
        work_done++;
    }

    if (work_done < budget) {
        // 预算未用完，重新启用中断
        napi_complete_done(napi, work_done);
        enable_interrupts(dev);
    }

    return work_done;
}
```

## 常用命令

```bash
# 网络接口
ip link show                 # 显示所有接口
ip addr show                 # 显示IP地址
ip link set eth0 up/down     # 启用/禁用接口

# 路由
ip route show                # 显示路由表
ip route add default via GW  # 添加默认路由

# 连接状态
ss -tuln                     # 监听端口
ss -tun                      # TCP/UDP连接
netstat -tuln

# 统计信息
cat /proc/net/dev            # 设备统计
cat /proc/net/snmp           # SNMP统计
cat /proc/net/tcp            # TCP连接

# 抓包分析
tcpdump -i eth0              # 抓包
tcpdump -i eth0 port 80      # 指定端口
tcpdump -i eth0 -w file.pcap # 保存到文件

# 防火墙
iptables -L                  # 列出规则
iptables -A INPUT -p tcp --dport 22 -j ACCEPT

# 网络诊断
ping host                    # ICMP探测
traceroute host              # 路由追踪
mtr host                     # 综合诊断
```

## 源码位置

```
net/
├── socket.c              # Socket核心
├── sysctl_net.c          # 网络sysctl
│
├── core/
│   ├── dev.c             # 设备核心
│   ├── dev.c            # 网络设备核心
│   ├── skbuff.c         # sk_buff操作
│   ├── sock.c           # sock操作
│   ├── filter.c         # BPF过滤器
│   └── datagram.c       # 数据报操作
│
├── ipv4/
│   ├── af_inet.c        # INET协议族
│   ├── tcp.c            # TCP协议
│   ├── tcp_input.c      # TCP输入
│   ├── tcp_output.c     # TCP输出
│   ├── udp.c            # UDP协议
│   ├── ip_input.c       # IP输入
│   ├── ip_output.c      # IP输出
│   ├── route.c          # 路由
│   ├── arp.c            # ARP协议
│   └── netfilter/       # Netfilter框架
│
├── ipv6/                 # IPv6实现
│
├── ethernet/             # 以太网
│   └── eth.c
│
├── packet/               # 原始套接字
│
├── unix/                 # Unix域套接字
│
└── netlink/              # Netlink套接字

include/linux/
├── net.h                 # socket结构
├── skbuff.h              # sk_buff结构
├── netdevice.h           # net_device结构
└── sock.h                # sock结构

include/net/
├── tcp.h                 # TCP相关
├── udp.h                 # UDP相关
└── ip.h                  # IP相关
```

## 🧪 实验

> [!important] 实践是检验理解的最好方式
> 详见 [[OS/Linux/09.网络子系统/网络子系统实验|实验指南]]

| 实验 | 内容 | 难度 |
|------|------|------|
| 实验一 | 网络数据包分析 | ⭐ |
| 实验二 | Socket编程 | ⭐⭐ |
| 实验三 | Netfilter模块 | ⭐⭐⭐ |
| 实验四 | 网络设备驱动 | ⭐⭐⭐ |

## 🔗 相关链接

- [[../04.中断与异常/Linux中断与异常|中断与异常]] - NAPI、软中断
- [[../08.设备驱动/Linux设备驱动|设备驱动]] - 网络设备驱动
- [[OS/Linux/09.网络子系统/Socket实现]] - Socket API实现
- [[OS/Linux/09.网络子系统/Netfilter框架]] - 包过滤与NAT
