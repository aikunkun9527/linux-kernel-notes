---
tags:
  - Linux
  - Kernel
  - 进程管理
created: 2026-03-11
status: 待学习
---

# 进程描述符 task_struct

> [!info] 目标
> 深入理解Linux内核中进程的表示与管理

## 进程与线程

在Linux中：
- **进程**：拥有独立地址空间的执行实体
- **线程**：共享地址空间的轻量级进程
- **统一表示**：都用`task_struct`描述

## task_struct 结构

`task_struct`是进程描述符，定义在`include/linux/sched.h`

### 关键字段分类

```
task_struct
├── 标识信息
│   ├── pid          // 进程ID
│   ├── tgid         // 线程组ID
│   └── real_parent  // 父进程
│
├── 状态信息
│   ├── state        // 进程状态
│   └── exit_state   // 退出状态
│
├── 调度信息
│   ├── prio         // 优先级
│   ├── static_prio  // 静态优先级
│   └── rt_priority  // 实时优先级
│
├── 内存信息
│   ├── mm           // 用户空间mm_struct
│   └── active_mm    // 内核线程借用
│
├── 文件信息
│   ├── fs           // 文件系统信息
│   └── files        // 打开文件表
│
└── 信号信息
    ├── signal       // 信号处理
    └── blocked      // 阻塞信号集
```

### 核心字段详解

#### 进程标识

```c
pid_t pid;           // 进程唯一标识
pid_t tgid;          // 线程组ID（进程ID）
struct task_struct *parent;   // 父进程
struct list_head children;    // 子进程链表
struct list_head sibling;     // 兄弟进程链表
```

#### 进程状态

```c
// 进程状态定义
#define TASK_RUNNING         0  // 运行或就绪
#define TASK_INTERRUPTIBLE   1  // 可中断睡眠
#define TASK_UNINTERRUPTIBLE 2  // 不可中断睡眠
#define TASK_STOPPED         4  // 停止
#define TASK_TRACED          8  // 被追踪
#define EXIT_ZOMBIE         16  // 僵尸状态
#define EXIT_DEAD           32  // 死亡
```

#### 调度相关

```c
int prio;              // 动态优先级
int static_prio;       // 静态优先级 (100-139)
unsigned int rt_priority;  // 实时优先级 (0-99)
const struct sched_class *sched_class;  // 调度类
```

## 进程状态转换图

```
              fork()
                │
                ▼
           ┌─────────┐
           │ RUNNING │◄──────────────────┐
           └────┬────┘                   │
                │                        │
    ┌───────────┴───────────┐           │
    ▼                       ▼           │
┌─────────┐          ┌─────────┐       │
│INTERRUPTIBLE│      │UNINTERRUPTIBLE│   │
└────┬────┘          └────┬────┘       │
     │                    │            │
     │ wake_up            │ wake_up    │
     └────────────────────┴────────────┘
                              │
                              │ exit()
                              ▼
                        ┌─────────┐
                        │ ZOMBIE  │
                        └────┬────┘
                             │ wait()
                             ▼
                        ┌─────────┐
                        │  DEAD   │
                        └─────────┘
```

## 访问当前进程

```c
// 获取当前进程的task_struct
struct task_struct *current;

// 在x86上
#define current get_current()

// 使用示例
pr_info("Current PID: %d\n", current->pid);
pr_info("Current comm: %s\n", current->comm);
```

## 进程链表组织

内核使用多种数据结构组织进程：

### 1. PID哈希表
```c
// 通过PID快速查找task_struct
struct pid *find_pid_ns(int nr, struct pid_namespace *ns);
struct task_struct *pid_task(struct pid *pid, enum pid_type type);
```

### 2. 运行队列
```c
// CPU运行队列
struct rq {
    struct task_struct *curr;
    struct task_struct *idle;
    // ...
};
```

## 相关结构体

| 结构体 | 用途 |
|--------|------|
| `mm_struct` | 进程地址空间 |
| `fs_struct` | 文件系统信息 |
| `files_struct` | 打开文件表 |
| `signal_struct` | 信号处理 |

## 相关笔记
- [[进程调度器]]
- [[进程创建与销毁]]
- [[../03.内存管理/内存寻址基础|内存寻址基础]]
