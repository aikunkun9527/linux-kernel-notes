---
tags:
  - Linux
  - Kernel
  - 同步
  - RCU
created: 2026-03-15
status: 待学习
---

# RCU机制

> [!info] 目标
> 理解Linux内核RCU（Read-Copy-Update）同步机制的原理与应用

## 什么是RCU

**RCU (Read-Copy-Update)**：一种高效的同步机制，专门优化**读多写少**的场景

### 核心思想

```
传统锁的问题：
┌─────────────────────────────────────────────────────┐
│  读者获取读锁 → 写者等待 → 读者释放 → 写者获取写锁  │
│                                                      │
│  问题：读者阻塞写者，即使读者只是读取数据           │
└─────────────────────────────────────────────────────┘

RCU的解决方案：
┌─────────────────────────────────────────────────────┐
│  读者：无锁读取，只记录读者的存在                   │
│  写者：复制数据，修改副本，延迟替换指针            │
│                                                      │
│  优势：读者不阻塞写者，读操作几乎零开销            │
└─────────────────────────────────────────────────────┘
```

### RCU特点

| 特点 | 说明 |
|------|------|
| 读者无锁 | 读操作不需要获取任何锁 |
| 写者复制 | 写操作复制数据，修改副本 |
| 延迟释放 | 等待所有读者完成后释放旧数据 |
| 适用场景 | 读多写少 |

## RCU工作原理

```
初始状态：
┌─────────────────────────────────────────┐
│           数据结构                       │
│  ┌─────────────────────────────────┐    │
│  │         原始数据 (A)            │◄───┼─── 指针
│  └─────────────────────────────────┘    │
│         ↑            ↑                   │
│      读者1         读者2                 │
└─────────────────────────────────────────┘

更新过程：

步骤1: 写者复制数据
┌─────────────────────────────────────────┐
│  ┌─────────────────────────────────┐    │
│  │         原始数据 (A)            │◄───┼─── 旧指针
│  └─────────────────────────────────┘    │
│         ↑                               │
│      读者1 (仍在读取A)                   │
│                                          │
│  ┌─────────────────────────────────┐    │
│  │         新数据 (B)              │◄───┼─── 新指针(尚未发布)
│  └─────────────────────────────────┘    │
│         写者正在修改                     │
└─────────────────────────────────────────┘

步骤2: 发布新数据（原子替换指针）
┌─────────────────────────────────────────┐
│  ┌─────────────────────────────────┐    │
│  │         原始数据 (A)            │    │
│  └─────────────────────────────────┘    │
│         ↑                               │
│      读者1 (仍在读取A) ← 旧读者看到旧数据 │
│                                          │
│  ┌─────────────────────────────────┐    │
│  │         新数据 (B)              │◄───┼─── 新指针(已发布)
│  └─────────────────────────────────┘    │
│         ↑                               │
│      新读者看到新数据                    │
└─────────────────────────────────────────┘

步骤3: 等待宽限期结束，释放旧数据
┌─────────────────────────────────────────┐
│  ┌─────────────────────────────────┐    │
│  │         新数据 (B)              │◄───┼─── 唯一指针
│  └─────────────────────────────────┘    │
│         ↑            ↑                   │
│      新读者        新读者                │
│                                          │
│  (旧数据A已释放，内存已回收)             │
└─────────────────────────────────────────┘
```

## 宽限期 (Grace Period)

### 概念

**宽限期**：等待所有已存在的读者完成读取的时间段

```
时间线：
─────────────────────────────────────────────────────►

读者1:  ────[读取开始]──────────[读取结束]────────
读者2:  ────────[读取开始]──────[读取结束]────────
读者3:  ────────────────────────[读取开始]────[读取结束]

更新:  ────[复制修改]─[发布]─────────────────────
                              │
                              ▼ 宽限期开始
                              │
                              │ 等待读者1、2完成
                              │
                              ▼ 宽限期结束
回收:  ────────────────────────[释放旧数据]─────
```

### synchronize_rcu()

```c
// 等待宽限期结束
void synchronize_rcu(void);

// 更新操作示例
void update_data(struct my_data *new_data)
{
    struct my_data *old_data;

    old_data = rcu_dereference(global_data);
    rcu_assign_pointer(global_data, new_data);

    // 等待所有读者完成
    synchronize_rcu();

    // 安全释放旧数据
    kfree(old_data);
}
```

## RCU API

### 读取侧

```c
#include <linux/rcupdate.h>

// 进入RCU读临界区
void rcu_read_lock(void);

// 退出RCU读临界区
void rcu_read_unlock(void);

// 安全读取RCU保护的数据
#define rcu_dereference(p) \
    ({ \
        typeof(p) _________p1 = READ_ONCE(p); \
        rcu_dereference_check(p, rcu_read_lock_held()); \
        _________p1; \
    })
```

### 更新侧

```c
// 发布新数据（带内存屏障）
#define rcu_assign_pointer(p, v)

// 等待宽限期
void synchronize_rcu(void);

// 异步等待宽限期（回调方式）
void call_rcu(struct rcu_head *head, rcu_callback_t func);

// 立即释放（使用call_rcu）
void kfree_rcu(void *ptr, struct rcu_head *rh);
```

### 使用示例

```c
// 数据结构
struct my_data {
    int value;
    char name[64];
    struct rcu_head rcu;  // 用于call_rcu
};

// 全局数据指针
struct my_data __rcu *global_data;

// 读取（无锁）
int read_value(void)
{
    struct my_data *data;
    int value;

    rcu_read_lock();
    data = rcu_dereference(global_data);
    value = data->value;  // 读取数据
    rcu_read_unlock();

    return value;
}

// 更新（同步方式）
void update_value_sync(int new_value)
{
    struct my_data *old_data, *new_data;

    // 分配新数据
    new_data = kmalloc(sizeof(*new_data), GFP_KERNEL);
    new_data->value = new_value;
    strcpy(new_data->name, "updated");

    // 替换指针
    old_data = rcu_dereference(global_data);
    rcu_assign_pointer(global_data, new_data);

    // 等待读者完成
    synchronize_rcu();

    // 释放旧数据
    kfree(old_data);
}

// 更新（异步方式）
static void free_data_rcu(struct rcu_head *rh)
{
    struct my_data *data = container_of(rh, struct my_data, rcu);
    kfree(data);
}

void update_value_async(int new_value)
{
    struct my_data *old_data, *new_data;

    new_data = kmalloc(sizeof(*new_data), GFP_KERNEL);
    new_data->value = new_value;

    old_data = rcu_dereference(global_data);
    rcu_assign_pointer(global_data, new_data);

    // 异步释放
    call_rcu(&old_data->rcu, free_data_rcu);
}

// 更简单的方式：kfree_rcu
void update_value_simple(int new_value)
{
    struct my_data *old_data, *new_data;

    new_data = kmalloc(sizeof(*new_data), GFP_KERNEL);
    new_data->value = new_value;

    old_data = rcu_dereference(global_data);
    rcu_assign_pointer(global_data, new_data);

    // 直接使用kfree_rcu
    kfree_rcu(old_data, rcu);
}
```

## RCU链表操作

### list_head RCU操作

```c
#include <linux/list.h>
#include <linux/rculist.h>

// 初始化
LIST_HEAD(my_list);

// 添加（更新侧）
void add_item_rcu(struct my_item *new)
{
    list_add_rcu(&new->list, &my_list);
}

// 删除（更新侧）
void remove_item_rcu(struct my_item *item)
{
    list_del_rcu(&item->list);
    kfree_rcu(item, rcu);
}

// 遍历（读取侧）
void traverse_items_rcu(void)
{
    struct my_item *item;

    rcu_read_lock();
    list_for_each_entry_rcu(item, &my_list, list) {
        // 访问item
        process_item(item);
    }
    rcu_read_unlock();
}
```

### hlist RCU操作

```c
// 哈希链表的RCU操作
void hlist_add_rcu(struct hlist_node *node, struct hlist_head *head);
void hlist_del_rcu(struct hlist_node *node);

#define hlist_for_each_entry_rcu(pos, head, member)
```

## RCU变体

| 变体 | 说明 | 适用场景 |
|------|------|----------|
| RCU | 标准RCU | 一般内核代码 |
| SRCU | 可睡眠RCU | 读临界区可能睡眠 |
| RCU-sched | 调度RCU | 不关心睡眠 |
| RCU-bh | 底半部RCU | 网络等软中断场景 |

### SRCU (Sleepable RCU)

```c
// 定义SRCU
DEFINE_SRCU(my_srcu);
// 或
struct srcu_struct my_srcu;
init_srcu_struct(&my_srcu);

// 读取
int idx = srcu_read_lock(&my_srcu);
// 可以睡眠！
srcu_read_unlock(&my_srcu, idx);

// 更新
synchronize_srcu(&my_srcu);
```

## RCU vs 其他同步机制

```
读操作开销对比：

          RCU         rwlock      mutex
          ────        ──────      ─────
读开销    极低        中等        中等
         (无锁)      (原子操作)   (原子操作)

写操作开销：

          RCU         rwlock      mutex
          ────        ──────      ─────
写开销    较高        中等        中等
         (复制+等待) (等待读者)

适用场景：
- 读远多于写 → RCU
- 读写相当 → rwlock
- 写为主或临界区长 → mutex
```

## 实际应用示例

### 1. 网络路由表

```c
// 路由表项
struct route_entry {
    __be32 daddr;
    __be32 gw;
    struct rcu_head rcu;
    struct hlist_node node;
};

// 查找路由（高频读）
struct route_entry *lookup_route(__be32 daddr)
{
    struct route_entry *entry;

    rcu_read_lock();
    hlist_for_each_entry_rcu(entry, &route_table[daddr & mask], node) {
        if (entry->daddr == daddr) {
            rcu_read_unlock();
            return entry;
        }
    }
    rcu_read_unlock();
    return NULL;
}

// 更新路由（低频写）
void update_route(__be32 daddr, __be32 gw)
{
    struct route_entry *new, *old;

    new = kmalloc(sizeof(*new), GFP_KERNEL);
    new->daddr = daddr;
    new->gw = gw;

    old = find_and_remove_old(daddr);
    hlist_add_head_rcu(&new->node, &route_table[hash(daddr)]);

    if (old)
        kfree_rcu(old, rcu);
}
```

### 2. 配置更新

```c
// 全局配置
struct config __rcu *global_config;

// 读取配置
int get_config_value(void)
{
    struct config *cfg;
    int value;

    rcu_read_lock();
    cfg = rcu_dereference(global_config);
    value = cfg->some_value;
    rcu_read_unlock();

    return value;
}

// 更新配置（通过/proc或sysfs）
ssize_t set_config(struct file *file, const char __user *buf,
                   size_t count, loff_t *ppos)
{
    struct config *new_cfg, *old_cfg;

    new_cfg = kmalloc(sizeof(*new_cfg), GFP_KERNEL);
    // 解析用户输入...

    old_cfg = rcu_dereference(global_config);
    *new_cfg = *old_cfg;  // 复制
    // 修改...

    rcu_assign_pointer(global_config, new_cfg);
    kfree_rcu(old_cfg, rcu);

    return count;
}
```

## 注意事项

> [!warning] RCU使用规则
> 1. **读临界区不能阻塞**：`rcu_read_lock()` 和 `rcu_read_unlock()` 之间不能睡眠（除非使用SRCU）
> 2. **写者必须提供同步**：多个写者需要额外同步
> 3. **数据必须是可复制的**：指针替换，数据复制
> 4. **延迟释放**：使用 `synchronize_rcu()` 或 `call_rcu()`

## 相关笔记

- [[自旋锁]]
- [[互斥锁与信号量]]
- [[Linux内核同步机制]]
