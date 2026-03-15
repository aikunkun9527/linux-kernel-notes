---
tags:
  - Linux
  - Kernel
  - 网络
  - Netfilter
  - 防火墙
created: 2026-03-15
status: 学习中
---

# Netfilter框架

> [!info] 目标
> 理解Linux内核Netfilter框架的架构与使用

> [!note] 网络子系统完整笔记
> 本笔记详细介绍Netfilter，总览请参考 [[OS/Linux/09.网络子系统/Linux网络子系统|MOC]]

## Netfilter概述

### 什么是Netfilter

**Netfilter**：Linux内核中的网络包过滤框架

- 提供**钩子点**拦截数据包
- 实现防火墙、NAT、包过滤等功能
- 用户态工具：**iptables**, **nftables**

### 架构图

```
┌─────────────────────────────────────────────────────────────┐
│                     用户空间工具                            │
│        iptables, nftables, conntrack-tools                 │
├─────────────────────────────────────────────────────────────┤
│                     Netlink接口                             │
│   配置规则、查询状态、接收事件                              │
├─────────────────────────────────────────────────────────────┤
│                     Netfilter核心                           │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                  5个钩子点                          │   │
│  │  PREROUTING → INPUT → FORWARD → OUTPUT → POSTROUTING│   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐                   │
│  │iptables  │ │conntrack │ │ NAT     │  ...              │
│  │表        │ │连接跟踪  │ │         │                   │
│  └──────────┘ └──────────┘ └──────────┘                   │
├─────────────────────────────────────────────────────────────┤
│                     协议栈                                  │
│              TCP/UDP/IP处理                                │
└─────────────────────────────────────────────────────────────┘
```

## 钩子点

### 5个关键钩子点

```c
// include/uapi/linux/netfilter.h
enum nf_inet_hooks {
    NF_INET_PRE_ROUTING,    // 路由前
    NF_INET_LOCAL_IN,       // 本地输入
    NF_INET_FORWARD,        // 转发
    NF_INET_LOCAL_OUT,      // 本地输出
    NF_INET_POST_ROUTING,   // 路由后
    NF_INET_NUMHOOKS
};
```

### 数据包流程

```
                    本地进程
                       ▲
                       │
           ┌───────────┴───────────┐
           │       INPUT           │
           │  (NF_INET_LOCAL_IN)   │
           └───────────┬───────────┘
                       │
           ┌───────────┴───────────┐
           │     PREROUTING        │
           │ (NF_INET_PRE_ROUTING) │
           └───────────┬───────────┘
                       │
        接收数据包     │
           ◄───────────┤
                       │
           ┌───────────┴───────────┐
           │      Routing          │
           │       Decision        │
           └───────────┬───────────┘
                       │
           ┌───────────┴───────────┐
           │      FORWARD          │
           │  (NF_INET_FORWARD)    │───────────┐
           └───────────┬───────────┘           │
                       │                       │
                       │                       ▼
           ┌───────────┴───────────┐   ┌───────────┐
           │     POSTROUTING       │   │  OUTPUT   │
           │(NF_INET_POST_ROUTING) │   │(LOCAL_OUT)│
           └───────────┬───────────┘   └─────┬─────┘
                       │                     │
                       │     发送数据包      │
                       └─────────────────────┘
```

### 各钩子点用途

| 钩子点 | 触发时机 | 典型用途 |
|--------|----------|----------|
| PRE_ROUTING | 数据包到达，路由判断前 | DNAT、连接跟踪 |
| LOCAL_IN | 目标是本机的包 | 过滤入站流量 |
| FORWARD | 转发的包 | 过滤转发流量 |
| LOCAL_OUT | 本机发出的包 | 过滤出站流量 |
| POST_ROUTING | 数据包离开前 | SNAT |

## 钩子函数

### 注册钩子函数

```c
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>

// 钩子函数原型
typedef unsigned int nf_hookfn(void *priv,
                               struct sk_buff *skb,
                               const struct nf_hook_state *state);

// 钩子操作结构
struct nf_hook_ops {
    nf_hookfn       *hook;          // 钩子函数
    struct net_device *dev;         // 网络设备（可选）
    void            *priv;          // 私有数据
    u_int8_t        pf;             // 协议族 (PF_INET)
    unsigned int    hooknum;        // 钩子点
    int             priority;       // 优先级
};

// 注册/注销
int nf_register_net_hook(struct net *net, const struct nf_hook_ops *ops);
void nf_unregister_net_hook(struct net *net, const struct nf_hook_ops *ops);

// 批量注册
int nf_register_net_hooks(struct net *net, const struct nf_hook_ops *reg,
                          unsigned int n);
void nf_unregister_net_hooks(struct net *net, const struct nf_hook_ops *reg,
                             unsigned int n);
```

### 钩子函数返回值

```c
// 返回值决定数据包的处理
#define NF_DROP     0   // 丢弃数据包
#define NF_ACCEPT   1   // 接受，继续处理
#define NF_STOLEN   2   // 钩子函数接管，不再继续
#define NF_QUEUE    3   // 发送到用户空间队列
#define NF_REPEAT   4   // 重新调用钩子函数
#define NF_STOP     5   // 停止调用后续钩子，接受
```

### 简单钩子示例

```c
// 文件: netfilter_example.c
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/ip.h>
#include <linux/tcp.h>

static unsigned int my_hook_fn(void *priv,
                               struct sk_buff *skb,
                               const struct nf_hook_state *state)
{
    struct iphdr *iph;
    struct tcphdr *tcph;

    if (!skb)
        return NF_ACCEPT;

    // 获取IP头
    iph = ip_hdr(skb);
    if (!iph)
        return NF_ACCEPT;

    // 只处理TCP
    if (iph->protocol != IPPROTO_TCP)
        return NF_ACCEPT;

    // 获取TCP头
    tcph = tcp_hdr(skb);
    if (!tcph)
        return NF_ACCEPT;

    // 过滤目标端口为80的包
    if (ntohs(tcph->dest) == 80) {
        pr_info("Dropping TCP packet to port 80 from %pI4:%d\n",
                &iph->saddr, ntohs(tcph->source));
        return NF_DROP;
    }

    return NF_ACCEPT;
}

static const struct nf_hook_ops my_ops = {
    .hook       = my_hook_fn,
    .pf         = PF_INET,
    .hooknum    = NF_INET_LOCAL_OUT,
    .priority   = NF_IP_PRI_FIRST,
};

static int __init my_init(void)
{
    return nf_register_net_hook(&init_net, &my_ops);
}

static void __exit my_exit(void)
{
    nf_unregister_net_hook(&init_net, &my_ops);
}

module_init(my_init);
module_exit(my_exit);
MODULE_LICENSE("GPL");
```

## iptables表和链

### 表类型

| 表 | 用途 |
|-----|------|
| filter | 包过滤 |
| nat | 地址转换 |
| mangle | 包修改 |
| raw | 连接跟踪豁免 |
| security | SELinux |

### 链与钩子点对应

```
iptables链      Netfilter钩子点
────────────────────────────────
PREROUTING  →   NF_INET_PRE_ROUTING
INPUT       →   NF_INET_LOCAL_IN
FORWARD     →   NF_INET_FORWARD
OUTPUT      →   NF_INET_LOCAL_OUT
POSTROUTING →   NF_INET_POST_ROUTING
```

### 表链关系

```
filter表：
  - INPUT
  - FORWARD
  - OUTPUT

nat表：
  - PREROUTING (DNAT)
  - INPUT
  - OUTPUT
  - POSTROUTING (SNAT)

mangle表：
  - 所有链

raw表：
  - PREROUTING
  - OUTPUT
```

## 连接跟踪 (conntrack)

### 概念

**连接跟踪**：跟踪网络连接状态，用于状态检测和NAT

### 连接状态

```c
enum ip_conntrack_status {
    IPS_EXPECTED,       // 预期的连接
    IPS_SEEN_REPLY,     // 看到回复
    IPS_ASSURED,        // 确认的连接
    IPS_CONFIRMED,      // 已确认
    IPS_SRC_NAT,        // 需要SNAT
    IPS_DST_NAT,        // 需要DNAT
    IPS_NAT_MASK,       // NAT掩码
    IPS_SEQ_ADJUST,     // 序列号调整
    IPS_SRC_NAT_DONE,   // SNAT完成
    IPS_DST_NAT_DONE,   // DNAT完成
    IPS_DYING,          // 即将删除
    IPS_FIXED_TIMEOUT,  // 固定超时
};
```

### 状态检测

```bash
# 查看连接跟踪表
cat /proc/net/nf_conntrack

# 连接跟踪统计
cat /proc/sys/net/netfilter/nf_conntrack_count
cat /proc/sys/net/netfilter/nf_conntrack_max

# 使用conntrack工具
conntrack -L           # 列出连接
conntrack -E           # 监控事件
```

### iptables状态匹配

```bash
# 允许已建立的连接
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT

# 状态类型
# NEW          - 新连接
# ESTABLISHED  - 已建立的连接
# RELATED      - 相关连接（如FTP数据连接）
# INVALID      - 无效包
# UNTRACKED    - 未跟踪
```

## NAT实现

### SNAT (源地址转换)

```bash
# 修改源IP，用于共享上网
iptables -t nat -A POSTROUTING -s 192.168.1.0/24 -o eth0 -j SNAT --to-source 1.2.3.4

# MASQUERADE (动态SNAT)
iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
```

### DNAT (目标地址转换)

```bash
# 修改目标IP，用于端口转发
iptables -t nat -A PREROUTING -d 1.2.3.4 -p tcp --dport 80 -j DNAT --to-destination 192.168.1.100:8080
```

### NAT流程

```
入站DNAT:
PREROUTING → DNAT → 路由判断 → LOCAL_IN/FORWARD

出站SNAT:
LOCAL_OUT/POSTROUTING → SNAT → 发送
```

## nftables

### 为什么需要nftables

- **统一的语法**：替代iptables, ip6tables, arptables, ebtables
- **更高效**：减少内核态处理
- **更灵活**：支持更多匹配和目标

### nftables基本使用

```bash
# 创建表
nft add table inet filter

# 创建链
nft add chain inet filter input { type filter hook input priority 0 \; }

# 添加规则
nft add rule inet filter input tcp dport 22 accept
nft add rule inet filter input tcp dport 80 drop

# 查看规则
nft list table inet filter

# 删除规则
nft delete rule inet filter input handle 2
```

## 常用命令

```bash
# iptables基本操作
iptables -L                    # 列出规则
iptables -L -n -v             # 详细显示
iptables -L -t nat            # NAT表
iptables -F                   # 清空规则
iptables -X                   # 删除自定义链

# 添加规则
iptables -A INPUT -p tcp --dport 22 -j ACCEPT
iptables -A INPUT -s 192.168.1.0/24 -j ACCEPT
iptables -A INPUT -j DROP

# 删除规则
iptables -D INPUT 1           # 删除第1条
iptables -D INPUT -p tcp --dport 22 -j ACCEPT

# 保存规则
iptables-save > /etc/iptables/rules.v4
iptables-restore < /etc/iptables/rules.v4

# 查看规则匹配
iptables -L -v -n

# 监控
watch -n 1 iptables -L -v
```

## 源码位置

```
net/netfilter/
├── core.c               # Netfilter核心
├── nf_hook.c            # 钩子管理
├── nf_queue.c           # 队列处理
├── nf_log.c             # 日志
│
├── nf_tables_api.c      # nftables API
├── nf_tables_core.c     # nftables核心
├── nft_*.c              # nftables表达式
│
├── nf_conntrack_core.c  # 连接跟踪核心
├── nf_conntrack_standalone.c
├── nf_conntrack_*.c     # 连接跟踪协议
│
├── nf_nat_core.c        # NAT核心
├── nf_nat_*.c           # NAT实现
│
├── iptable_filter.c     # filter表
├── iptable_nat.c        # nat表
├── iptable_mangle.c     # mangle表
├── iptable_raw.c        # raw表
│
├── ip_tables.c          # iptables核心
└── arp_tables.c         # ARP表

include/linux/
├── netfilter.h          # Netfilter核心定义
├── netfilter/
│   └── nf_conntrack*.h  # 连接跟踪
└── uapi/linux/netfilter/
    └── *.h              # 用户态头文件
```

## 相关笔记

- [[OS/Linux/09.网络子系统/网络协议栈概述]]
- [[OS/Linux/09.网络子系统/Socket实现]]
- [[OS/Linux/09.网络子系统/Linux网络子系统]]
