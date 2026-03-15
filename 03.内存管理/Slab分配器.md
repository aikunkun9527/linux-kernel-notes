---
tags:
  - Linux
  - Kernel
  - 内存管理
created: 2026-03-11
status: 待学习
---

# Slab分配器

> [!info] 目标
> 理解Linux内核的小对象内存分配机制

## Slab分配器概述

伙伴系统最小分配单位是1页（4KB），但内核经常需要分配更小的对象（如`inode`、`task_struct`）。Slab分配器在伙伴系统之上提供：

1. **小对象分配**：小于1页的对象
2. **对象缓存**：频繁分配/释放的对象
3. **硬件缓存友好**：减少缓存行冲突

## 三种分配器实现

| 分配器 | 特点 | 适用场景 |
|--------|------|----------|
| **SLAB** | 最初实现，功能完整 | 通用系统 |
| **SLUB** | 当前默认，更简单高效 | 大多数系统 |
| **SLOB** | 极简实现，节省内存 | 嵌入式系统 |

## Slab架构

```
┌─────────────────────────────────────────────────────┐
│                    kmem_cache                        │
│              (对象类型描述符)                          │
├─────────────────────────────────────────────────────┤
│   per-CPU缓存                                        │
│  ┌────────┐  ┌────────┐  ┌────────┐                 │
│  │ CPU 0  │  │ CPU 1  │  │ CPU 2  │  ...           │
│  │ [obj]  │  │ [obj]  │  │ [obj]  │                │
│  │ [obj]  │  │ [obj]  │  │ [obj]  │                │
│  └────────┘  └────────┘  └────────┘                 │
├─────────────────────────────────────────────────────┤
│   Node部分 (NUMA)                                    │
│  ┌──────────────┐  ┌──────────────┐                 │
│  │    Node 0    │  │    Node 1    │                 │
│  │ ┌────┬────┐  │  │ ┌────┬────┐  │                 │
│  │ │full│partial│  │  │ │full│partial│               │
│  │ └────┴────┘  │  │ └────┴────┘  │                 │
│  └──────────────┘  └──────────────┘                 │
└─────────────────────────────────────────────────────┘
```

## 核心数据结构

### kmem_cache（缓存描述符）

```c
// mm/slab.h
struct kmem_cache {
    const char *name;           // 缓存名称
    unsigned int object_size;   // 对象大小
    unsigned int size;          // 对齐后大小

    /* SLUB特有 */
    unsigned int offset;        // freelist偏移
    unsigned int order;         // 每个slab占用的页数
    unsigned int objects;       // 每个slab的对象数

    /* Per-CPU缓存 */
    struct kmem_cache_cpu __percpu *cpu_slab;

    /* NUMA节点 */
    struct kmem_cache_node *node[MAX_NUMNODES];

    /* 构造/析构函数 */
    void (*ctor)(void *);

    /* 标志 */
    slab_flags_t flags;
    // ...
};
```

### kmem_cache_cpu（Per-CPU缓存）

```c
struct kmem_cache_cpu {
    void **freelist;        // 空闲对象链表
    struct page *page;      // 当前slab页面
    struct page *partial;   // 部分空闲的slab
    // ...
};
```

### kmem_cache_node（节点缓存）

```c
struct kmem_cache_node {
    spinlock_t list_lock;

    unsigned long nr_partial;
    struct list_head partial;   // 部分空闲的slab链表

#ifdef CONFIG_SLUB_DEBUG
    unsigned long nr_slabs;
    unsigned long total_objects;
    struct list_head full;      // 完全使用的slab链表
#endif
    // ...
};
```

## Slab状态

```
┌─────────────┐    分配对象      ┌─────────────┐
│   Partial   │ ───────────────► │    Full     │
│ (部分空闲)   │                  │  (已满)     │
└─────────────┘                  └─────────────┘
       ▲                                │
       │ 释放对象                        │ 释放对象
       │                                ▼
┌─────────────┐                  ┌─────────────┐
│    Empty    │ ◄─────────────── │   Partial   │
│   (全空)    │                  │             │
└─────────────┘                  └─────────────┘
       │
       │ 需要内存时归还伙伴系统
       ▼
   释放页面
```

## 分配API

### kmalloc / kfree

```c
#include <linux/slab.h>

// 动态分配
void *kmalloc(size_t size, gfp_t flags);
void kfree(const void *objp);

// 零初始化分配
void *kzalloc(size_t size, gfp_t flags);

// 重分配
void *krealloc(const void *objp, size_t new_size, gfp_t flags);

// 示例
struct my_struct *p;
p = kmalloc(sizeof(*p), GFP_KERNEL);
if (!p)
    return -ENOMEM;

// 使用p...
p->field = value;

kfree(p);
```

### 创建专用缓存

```c
// 创建缓存
struct kmem_cache *kmem_cache_create(
    const char *name,        // 缓存名称
    unsigned int size,       // 对象大小
    unsigned int align,      // 对齐要求
    slab_flags_t flags,      // 标志
    void (*ctor)(void *)     // 构造函数
);

// 从缓存分配
void *kmem_cache_alloc(struct kmem_cache *s, gfp_t gfpflags);

// 释放到缓存
void kmem_cache_free(struct kmem_cache *s, void *obj);

// 销毁缓存
void kmem_cache_destroy(struct kmem_cache *s);
```

### 专用缓存示例

```c
// 定义缓存指针
static struct kmem_cache *task_struct_cachep;

// 初始化时创建缓存
void __init fork_init(void)
{
    task_struct_cachep = kmem_cache_create(
        "task_struct",           // 名称
        sizeof(struct task_struct), // 大小
        ARCH_MIN_TASKALIGN,      // 对齐
        SLAB_PANIC | SLAB_NOTRACK, // 标志
        NULL                     // 无构造函数
    );
}

// 分配task_struct
struct task_struct *tsk;
tsk = kmem_cache_alloc(task_struct_cachep, GFP_KERNEL);

// 释放
kmem_cache_free(task_struct_cachep, tsk);
```

## 分配标志

```c
// 常用标志
SLAB_HWCACHE_ALIGN   // 按缓存行对齐
SLAB_PANIC           // 分配失败时panic
SLAB_RECLAIM_ACCOUNT // 可被回收
SLAB_CACHE_DMA       // 使用DMA内存
SLAB_NOTRACK         // 不追踪
```

## kmalloc大小

kmalloc提供一组预定义大小的缓存：

```c
// kmalloc大小系列（SLUB）
8, 16, 32, 64, 96, 128, 192, 256, 512, 1024,
2048, 4096, 8192, 16384, 32768, 65536, 131072, ...

// 查看系统kmalloc缓存
ls /sys/kernel/slab/kmalloc-*
```

## SLUB内部实现

### 对象布局

```
每个Slab页面：
┌────────────────────────────────────────────────────┐
│ struct page │ Object 0 │ Object 1 │ ... │ Object N │
│   元数据     │          │          │     │          │
└────────────────────────────────────────────────────┘

Object内部布局：
┌──────────────────┬───────────────────┐
│  用户数据        │  freelist指针     │
│  (object_size)   │  (若in-object)    │
└──────────────────┴───────────────────┘
```

### 快速路径分配

```c
// 简化的SLUB分配逻辑
void *kmem_cache_alloc(struct kmem_cache *s, gfp_t gfpflags)
{
    // 1. 获取当前CPU的缓存
    struct kmem_cache_cpu *c = this_cpu_ptr(s->cpu_slab);

    // 2. 从freelist取对象
    void *object = c->freelist;
    if (likely(object)) {
        c->freelist = get_freepointer(s, object);
        return object;
    }

    // 3. freelist为空，从partial链表获取
    // 4. partial也为空，从伙伴系统分配新页面
    return __slab_alloc(s, gfpflags);
}
```

## 调试与统计

```bash
# 查看Slab信息
cat /proc/slabinfo

# SLUB统计
cat /sys/kernel/slab/alloc/fraction
cat /sys/kernel/slab/task_struct/objects

# 查看kmalloc缓存大小
cat /proc/slabinfo | grep kmalloc
```

### slabinfo输出示例

```
# name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab>
task_struct       256          256        9472     4            8
mm_struct         128          128        1024     32           8
inode_cache       1024         1024       688      23           4
dentry            2048         2048       192      42           2
```

## 性能优化

### 对象预取

```c
// 分配多个对象时预取
void *obj = kmem_cache_alloc(cache, GFP_KERNEL);
prefetch(obj + cache->object_size);  // 预取下一个可能的对象
```

### Per-CPU缓存优势

1. **无锁分配**：从当前CPU缓存分配无需加锁
2. **缓存热度**：对象可能还在CPU缓存中
3. **减少缓存行 bouncing**：避免多核争用

## 相关笔记
- [[页面分配器]]
- [[进程地址空间]]
- [[内存区域与节点]]
