---
tags:
  - MOC
  - Linux
  - Kernel
  - 同步机制
created: 2026-03-15
status: 学习中
---

# Linux内核同步机制

> [!info] 概述
> 内核同步机制用于保护共享数据，防止并发访问导致的数据竞争和不一致性

## 🧠 核心概念

### 为什么需要同步

```
并发访问问题：
┌─────────────────────────────────────────────────────────┐
│  CPU 0                      CPU 1                       │
│  ──────                     ──────                      │
│  read  count (0)                                       │
│         │                   read  count (0)            │
│         │                      │                       │
│  count++                     count++                   │
│  write count (1)               │                       │
│         │                   write count (1)            │
│         ▼                      ▼                       │
│  期望结果: count = 2                                    │
│  实际结果: count = 1  ❌ 数据竞争！                     │
└─────────────────────────────────────────────────────────┘
```

### 并发来源

| 来源 | 说明 | 示例 |
|------|------|------|
| 多CPU | 多核同时执行 | SMP系统 |
| 中断 | 打断当前执行 | 时钟中断、设备中断 |
| 软中断 | 延迟处理 | tasklet、softirq |
| 抢占 | 内核抢占调度 | CONFIG_PREEMPT |
| 睡眠唤醒 | 进程调度 | 等待队列 |

### 同步机制分类

```
内核同步机制
├── 原子操作
│   ├── atomic_t
│   ├── atomic64_t
│   └── 原子位操作
│
├── 自旋锁
│   ├── spinlock_t
│   ├── 读写自旋锁 (rwlock_t)
│   └── 序列锁 (seqlock_t)
│
├── 睡眠锁
│   ├── 互斥锁 (mutex)
│   ├── 信号量 (semaphore)
│   └── 读写信号量 (rw_semaphore)
│
├── 完成量
│   └── completion
│
├── RCU
│   └── Read-Copy-Update
│
└── 其他
    ├── 禁用中断
    ├── 禁用抢占
    └── 内存屏障
```

## 📚 学习路径

### 第一层：基础同步
- [[原子操作]] - 原子变量、位操作
- [[自旋锁]] - 忙等待锁

### 第二层：睡眠锁
- [[互斥锁与信号量]] - 可睡眠锁
- [[完成量]] - 任务完成通知

### 第三层：高级机制
- [[RCU机制]] - 读多写少优化
- [[内存屏障]] - 内存可见性

## 同步机制对比

### 按特性分类

| 机制 | 等待方式 | 可睡眠 | 适用场景 | 开销 |
|------|----------|--------|----------|------|
| 原子操作 | 无等待 | - | 简单计数 | 最低 |
| 自旋锁 | 忙等待 | ❌ | 短临界区 | 低 |
| 互斥锁 | 睡眠 | ✓ | 长临界区 | 中 |
| 信号量 | 睡眠 | ✓ | 资源计数 | 中 |
| RCU | 无锁 | ✓ | 读多写少 | 特殊 |

### 选择决策树

```
需要同步保护？
    │
    ├─ 否 → 不需要
    │
    └─ 是 → 操作是否原子可完成？
              │
              ├─ 是 → 原子操作
              │
              └─ 否 → 临界区时间？
                        │
                        ├─ 很短（< 几微秒）→ 自旋锁
                        │
                        └─ 较长或可能睡眠？
                              │
                              ├─ 读写有明显区分？
                              │     │
                              │     ├─ 是，读多写少 → RCU
                              │     │
                              │     └─ 是，读写相当 → 读写信号量
                              │
                              └─ 需要资源计数？
                                    │
                                    ├─ 是 → 信号量
                                    │
                                    └─ 否 → 互斥锁
```

## 临界区保护原则

### 基本规则

> [!important] 同步原则
> 1. **最小化临界区**：只保护必须保护的代码
> 2. **避免嵌套锁**：防止死锁
> 3. **注意锁顺序**：统一获取顺序
> 4. **不要在锁中睡眠**：自旋锁中禁止睡眠

### 死锁场景

```
死锁示例：

CPU 0                          CPU 1
─────                          ─────
lock(A)
         │                     lock(B)
         │                              │
lock(B)  │                     lock(A) │
   ↓     │                        ↓    │
等待B    │                     等待A    │
         │                              │
         └──────── 相互等待 ────────────┘
                    死锁！
```

### 避免死锁

```c
// 方法1: 固定锁顺序
lock(A);  // 总是先获取A
lock(B);  // 再获取B

// 方法2: 使用lockdep检测
CONFIG_LOCKDEP=y  // 启用锁依赖检测

// 方法3: trylock
if (mutex_trylock(&A)) {
    if (mutex_trylock(&B)) {
        // 成功获取两把锁
    } else {
        mutex_unlock(&A);
    }
}
```

## 上下文与锁选择

```
┌─────────────────────────────────────────────────────────┐
│                    执行上下文                            │
├─────────────────────────────────────────────────────────┤
│                                                         │
│   进程上下文          可以睡眠 → mutex/semaphore        │
│        │               不能睡眠 → spinlock              │
│        │                                                │
│        ├────────── 软中断上下文                         │
│        │               spin_lock_bh()                   │
│        │                                                │
│        ├────────── 硬件中断上下文                       │
│        │               spin_lock_irqsave()              │
│        │                                                │
│        └────────── NMI上下文                            │
│                        raw_spin_lock()                  │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

## 常用API速查

### 原子操作

```c
// 整数原子操作
atomic_t v = ATOMIC_INIT(0);
atomic_set(&v, 1);
atomic_read(&v);
atomic_add(1, &v);
atomic_sub(1, &v);
atomic_inc(&v);
atomic_dec(&v);
atomic_xchg(&v, new);      // 交换
atomic_cmpxchg(&v, old, new);  // 比较交换

// 位原子操作
set_bit(nr, addr);
clear_bit(nr, addr);
change_bit(nr, addr);
test_bit(nr, addr);
test_and_set_bit(nr, addr);
```

### 自旋锁

```c
spinlock_t lock;
spin_lock_init(&lock);

spin_lock(&lock);
spin_unlock(&lock);

spin_lock_irqsave(&lock, flags);
spin_unlock_irqrestore(&lock, flags);

spin_lock_bh(&lock);
spin_unlock_bh(&lock);
```

### 互斥锁

```c
DEFINE_MUTEX(my_mutex);
// 或
struct mutex my_mutex;
mutex_init(&my_mutex);

mutex_lock(&my_mutex);
mutex_unlock(&my_mutex);

if (mutex_trylock(&my_mutex)) {
    // 获取成功
}
```

### 信号量

```c
DEFINE_SEMAPHORE(my_sem, 1);
// 或
struct semaphore my_sem;
sema_init(&my_sem, count);

down(&my_sem);    // 获取，可能睡眠
up(&my_sem);      // 释放

down_interruptible(&my_sem);  // 可中断
down_trylock(&my_sem);        // 非阻塞
```

### 完成量

```c
DECLARE_COMPLETION(my_comp);
// 或
struct completion my_comp;
init_completion(&my_comp);

wait_for_completion(&my_comp);  // 等待
complete(&my_comp);              // 唤醒一个
complete_all(&my_comp);          // 唤醒所有
```

## 调试工具

### 内核配置

```
CONFIG_DEBUG_SPINLOCK      # 自旋锁调试
CONFIG_DEBUG_MUTEXES       # 互斥锁调试
CONFIG_DEBUG_LOCK_ALLOC    # 锁分配调试
CONFIG_LOCKDEP             # 锁依赖检测
CONFIG_PROVE_LOCKING       # 锁正确性验证
```

### 运行时检测

```bash
# 查看锁统计
cat /proc/lockdep_stats

# 查看锁依赖
cat /proc/lockdep

# 查看锁冲突
cat /proc/lock_stat
```

## 源码位置

```
kernel/
├── locking/
│   ├── spinlock.c        # 自旋锁
│   ├── mutex.c           # 互斥锁
│   ├── semaphore.c       # 信号量
│   ├── rwsem.c           # 读写信号量
│   ├── completion.c      # 完成量
│   └── rtmutex.c         # 实时互斥锁

include/linux/
├── spinlock.h            # 自旋锁API
├── mutex.h               # 互斥锁API
├── semaphore.h           # 信号量API
├── completion.h          # 完成量API
├── atomic.h              # 原子操作
└── rcupdate.h            # RCU

arch/x86/include/asm/
├── atomic.h              # x86原子操作实现
├── spinlock.h            # x86自旋锁实现
└── barrier.h             # 内存屏障
```

## 🧪 实验

> [!important] 实践是检验理解的最好方式
> 详见 [[同步机制实验|实验指南]]

| 实验 | 内容 | 难度 |
|------|------|------|
| 实验一 | 原子操作与竞态条件 | ⭐ |
| 实验二 | 自旋锁性能测试 | ⭐⭐ |
| 实验三 | 互斥锁与信号量对比 | ⭐⭐ |
| 实验四 | RCU机制验证 | ⭐⭐⭐ |

## 🔗 相关链接

- [[../02.进程管理/Linux进程管理|进程管理]] - 调度与并发
- [[../04.中断与异常/Linux中断与异常|中断与异常]] - 中断上下文
- [[../08.设备驱动/字符设备驱动|驱动开发]] - 并发设备访问
