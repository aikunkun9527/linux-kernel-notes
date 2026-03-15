---
tags:
  - Linux
  - Kernel
  - 中断
  - 软中断
created: 2026-03-11
status: 待学习
---

# 软中断与Tasklet

> [!info] 目标
> 理解Linux内核的延迟处理机制：软中断、Tasklet和工作队列

## 为什么需要延迟处理

中断处理程序的约束：
- 执行时间要短
- 不能睡眠
- 在中断上下文中执行

解决方案：将处理分为两部分
- **上半部（Top Half）**：中断处理程序，快速完成
- **下半部（Bottom Half）**：延迟处理，可以耗时

## 延迟处理机制对比

```
┌─────────────────────────────────────────────────────────┐
│                     延迟处理机制                         │
├─────────────────────────────────────────────────────────┤
│                                                         │
│   执行上下文    软中断    Tasklet    工作队列            │
│   ────────────────────────────────────────────          │
│   中断上下文      ✓         ✓          ✗               │
│   进程上下文      ✗         ✗          ✓               │
│   可睡眠          ✗         ✗          ✓               │
│   并发性        多CPU      同类型      多线程           │
│               并行        串行        并行              │
│   复杂度         高        中          低               │
│   适用场景      网络      一般设备    耗时IO            │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

## 软中断（Softirq）

### 软中断类型

```c
// include/linux/interrupt.h
enum {
    HI_SOFTIRQ = 0,         // 高优先级Tasklet
    TIMER_SOFTIRQ,          // 定时器
    NET_TX_SOFTIRQ,         // 网络发送
    NET_RX_SOFTIRQ,         // 网络接收
    BLOCK_SOFTIRQ,          // 块设备
    IRQ_POLL_SOFTIRQ,       // 中断轮询
    TASKLET_SOFTIRQ,        // 普通Tasklet
    SCHED_SOFTIRQ,          // 调度
    HRTIMER_SOFTIRQ,        // 高精度定时器
    RCU_SOFTIRQ,            // RCU处理
    NR_SOFTIRQS
};
```

### 软中断数据结构

```c
// kernel/softirq.c
struct softirq_action {
    void (*action)(struct softirq_action *);
};

static struct softirq_action softirq_vec[NR_SOFTIRQS];

// Per-CPU软中断状态
DEFINE_PER_CPU(__u32, softirq_pending);
```

### 注册软中断

```c
// 注册软中断处理函数
void open_softirq(int nr, void (*action)(struct softirq_action *))
{
    softirq_vec[nr].action = action;
}

// 示例：注册网络软中断
open_softirq(NET_TX_SOFTIRQ, net_tx_action);
open_softirq(NET_RX_SOFTIRQ, net_rx_action);
```

### 触发软中断

```c
// 触发软中断（设置pending位）
void raise_softirq(unsigned int nr)
{
    unsigned long flags;

    local_irq_save(flags);
    raise_softirq_irqoff(nr);
    local_irq_restore(flags);
}

// 在中断禁用时触发
void raise_softirq_irqoff(unsigned int nr)
{
    __raise_softirq_irqoff(nr);

    // 如果不在中断上下文，唤醒ksoftirqd
    if (!in_interrupt())
        wakeup_softirqd();
}
```

### 软中断执行

```c
// 软中断处理入口
void __do_softirq(void)
{
    struct softirq_action *h;
    __u32 pending;

    // 1. 获取pending的软中断
    pending = local_softirq_pending();

    // 2. 重置pending
    set_softirq_pending(0);

    // 3. 循环处理所有pending的软中断
    while (pending) {
        unsigned int vec_nr;

        vec_nr = __ffs(pending);
        pending &= ~(1 << vec_nr);

        h = softirq_vec + vec_nr;
        h->action(h);  // 执行处理函数
    }
}

// 在中断返回时检查并执行软中断
void irq_exit(void)
{
    if (!in_interrupt() && local_softirq_pending())
        invoke_softirq();
}
```

### ksoftirqd内核线程

```c
// 当软中断负载过高时，由内核线程处理
static int ksoftirqd(void *data)
{
    while (!kthread_should_stop()) {
        if (local_softirq_pending()) {
            __do_softirq();
            cond_resched();
        }
        schedule();
    }
    return 0;
}
```

## Tasklet

### Tasklet特点

- 基于软中断实现（HI_SOFTIRQ和TASKLET_SOFTIRQ）
- **同类型Tasklet串行执行**（不同CPU上也不会并发）
- 不同类型Tasklet可以并行
- 比软中断更易使用

### Tasklet结构

```c
// include/linux/interrupt.h
struct tasklet_struct {
    struct tasklet_struct *next;  // 链表指针
    unsigned long state;          // 状态

    atomic_t count;               // 引用计数（禁用计数）
    void (*func)(unsigned long);  // 处理函数
    unsigned long data;           // 传递给处理函数的数据
};

// 状态标志
#define TASKLET_STATE_SCHED    0  // 已调度
#define TASKLET_STATE_RUN      1  // 正在运行
```

### 定义Tasklet

```c
// 静态定义
DECLARE_TASKLET(name, func, data);
DECLARE_TASKLET_DISABLED(name, func, data);

// 动态定义
struct tasklet_struct my_tasklet;
void my_tasklet_func(unsigned long data);

tasklet_init(&my_tasklet, my_tasklet_func, data);
```

### Tasklet操作

```c
#include <linux/interrupt.h>

// 调度Tasklet
void tasklet_schedule(struct tasklet_struct *t);
void tasklet_hi_schedule(struct tasklet_struct *t);  // 高优先级

// 禁用/启用Tasklet
void tasklet_disable(struct tasklet_struct *t);
void tasklet_enable(struct tasklet_struct *t);

// 杀死Tasklet（确保不会执行）
void tasklet_kill(struct tasklet_struct *t);
```

### Tasklet示例

```c
#include <linux/module.h>
#include <linux/interrupt.h>

struct my_device {
    int irq;
    struct tasklet_struct my_tasklet;
    // ...
};

// Tasklet处理函数
static void my_tasklet_handler(unsigned long data)
{
    struct my_device *dev = (struct my_device *)data;

    // 执行耗时操作
    process_data(dev);
}

// 中断处理程序（上半部）
static irqreturn_t my_interrupt(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;

    // 快速处理
    acknowledge_interrupt(dev);

    // 调度Tasklet（下半部）
    tasklet_schedule(&dev->my_tasklet);

    return IRQ_HANDLED;
}

static int my_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    struct my_device *dev;

    // 初始化Tasklet
    tasklet_init(&dev->my_tasklet, my_tasklet_handler, (unsigned long)dev);

    // 注册中断
    request_irq(dev->irq, my_interrupt, IRQF_SHARED, "mydev", dev);

    return 0;
}

static void my_remove(struct pci_dev *pdev)
{
    struct my_device *dev = pci_get_drvdata(pdev);

    // 杀死Tasklet
    tasklet_kill(&dev->my_tasklet);

    // 释放中断
    free_irq(dev->irq, dev);
}
```

## 工作队列（Workqueue）

### 工作队列特点

- 在**进程上下文**中执行
- **可以睡眠**
- 可以执行耗时操作

### 工作队列类型

```c
// 系统默认工作队列
events/0, events/1, ...  // 每CPU一个

// 自定义工作队列
struct workqueue_struct *create_workqueue(const char *name);
void destroy_workqueue(struct workqueue_struct *wq);
```

### Work结构

```c
// include/linux/workqueue.h
struct work_struct {
    atomic_long_t data;
    struct list_head entry;
    work_func_t func;    // 处理函数
};

// 延迟工作
struct delayed_work {
    struct work_struct work;
    struct timer_list timer;
};
```

### 使用工作队列

```c
#include <linux/workqueue.h>

// 定义work
DECLARE_WORK(my_work, my_work_handler);
DECLARE_DELAYED_WORK(my_delayed_work, my_handler);

// 动态初始化
INIT_WORK(&my_work, my_handler);
INIT_DELAYED_WORK(&my_delayed_work, my_handler);

// 调度work
void schedule_work(struct work_struct *work);
void schedule_delayed_work(struct delayed_work *dwork, unsigned long delay);

// 使用自定义工作队列
void queue_work(struct workqueue_struct *wq, struct work_struct *work);

// 等待work完成
void flush_work(struct work_struct *work);
void flush_workqueue(struct workqueue_struct *wq);

// 取消work
bool cancel_work_sync(struct work_struct *work);
bool cancel_delayed_work_sync(struct delayed_work *dwork);
```

### 工作队列示例

```c
#include <linux/module.h>
#include <linux/workqueue.h>

struct my_device {
    struct work_struct my_work;
    // ...
};

static void my_work_handler(struct work_struct *work)
{
    struct my_device *dev = container_of(work, struct my_device, my_work);

    // 可以睡眠的操作
    msleep(100);

    // 执行耗时操作
    process_large_data(dev);
}

static irqreturn_t my_interrupt(int irq, void *dev_id)
{
    struct my_device *dev = dev_id;

    // 调度work
    schedule_work(&dev->my_work);

    return IRQ_HANDLED;
}
```

## Threaded IRQ

现代内核推荐的另一种方式：

```c
// 注册线程化中断
int request_threaded_irq(unsigned int irq,
                         irq_handler_t handler,      // 上半部
                         irq_handler_t thread_fn,    // 线程函数
                         unsigned long flags,
                         const char *name,
                         void *dev);

// 示例
static irqreturn_t my_handler(int irq, void *dev_id)
{
    // 上半部：快速处理
    return IRQ_WAKE_THREAD;  // 唤醒线程
}

static irqreturn_t my_thread_fn(int irq, void *dev_id)
{
    // 下半部：在线程中执行，可以睡眠
    process_data(dev_id);
    return IRQ_HANDLED;
}

request_threaded_irq(irq, my_handler, my_thread_fn,
                     IRQF_SHARED, "mydev", dev);
```

## 选择指南

```
需要延迟处理？
    │
    ▼
需要睡眠？
    │
    ├─ 是 → 工作队列 或 Threaded IRQ
    │
    └─ 否 → 需要高精度时间？
              │
              ├─ 是 → HRTIMER_SOFTIRQ
              │
              └─ 否 → 需要高并发性能？
                        │
                        ├─ 是 → 软中断（如网络处理）
                        │
                        └─ 否 → Tasklet（更简单）
```

## 查看软中断状态

```bash
# 查看软中断统计
cat /proc/softirqs

# 输出示例
                    CPU0       CPU1       CPU2       CPU3
          HI:          0          0          0          0
       TIMER:     123456     123456     123456     123456
      NET_TX:       123        456        789       1011
      NET_RX:      4567      5678      6789      7890
       BLOCK:        12         34         56         78
 IRQ_POLL:          0          0          0          0
    TASKLET:        12         34         56         78
      SCHED:      1234      2345      3456      4567
    HRTIMER:         0          0          0          0
         RCU:      1234      1234      1234      1234
```

## 相关笔记

- [[中断处理机制]]
- [[Linux中断与异常]]
- [[../08.设备驱动/字符设备驱动|字符设备驱动]]
