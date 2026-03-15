---
tags:
  - MOC
  - Linux
  - Kernel
  - 进程管理
created: 2026-03-11
status: 学习中
---

# Linux进程管理

> [!info] 概述
> 进程管理是Linux内核的核心功能，负责进程的创建、调度、终止以及进程间通信

## 🧠 核心概念

### 进程与线程
- **进程**：资源分配的基本单位，拥有独立地址空间
- **线程**：CPU调度的基本单位，共享进程资源
- **Linux统一视角**：都用`task_struct`表示，线程是共享资源的进程

### 进程管理职责
```
┌─────────────────────────────────────────────┐
│              进程管理功能                    │
├─────────────────────────────────────────────┤
│  进程创建   │  进程调度   │  进程终止        │
│  fork/clone │  schedule  │  exit/wait       │
├─────────────────────────────────────────────┤
│  进程通信   │  进程同步   │  资源管理        │
│  IPC       │  同步原语   │  内存/文件       │
└─────────────────────────────────────────────┘
```

## 📚 学习路径

### 第一层：进程表示
- [[进程描述符task_struct]] - task_struct结构、字段含义
- [[进程状态与切换]] - 状态转换、上下文切换

### 第二层：进程生命周期
- [[进程创建与销毁]] - fork/clone/exec/exit实现
- [[进程调度器]] - CFS调度、优先级、时间片

### 第三层：进程通信
- [[进程间通信IPC]] - 管道、共享内存、信号量
- [[信号机制]] - 信号发送与处理

## 🗂️ 核心数据结构

### task_struct 关键字段

```c
struct task_struct {
    // 标识
    pid_t pid;                    // 进程ID
    pid_t tgid;                   // 线程组ID
    struct task_struct *parent;   // 父进程

    // 状态
    volatile long state;          // 进程状态
    int exit_state;               // 退出状态

    // 调度
    int prio, static_prio;        // 优先级
    const struct sched_class *sched_class;

    // 内存
    struct mm_struct *mm;         // 用户空间
    struct mm_struct *active_mm;  // 内核线程

    // 文件
    struct files_struct *files;   // 打开文件
    struct fs_struct *fs;         // 文件系统信息

    // 信号
    struct signal_struct *signal;
    struct sigpending pending;

    // ...
};
```

### 进程状态

```c
#define TASK_RUNNING         0x0000  // 运行或就绪
#define TASK_INTERRUPTIBLE   0x0001  // 可中断睡眠
#define TASK_UNINTERRUPTIBLE 0x0002  // 不可中断睡眠
#define TASK_STOPPED         0x0004  // 停止
#define TASK_TRACED          0x0008  // 被追踪
#define EXIT_ZOMBIE          0x0010  // 僵尸
#define EXIT_DEAD            0x0020  // 死亡
```

## 进程状态转换

```
                    fork()
                      │
                      ▼
               ┌──────────────┐
          ┌───►│ TASK_RUNNING │◄─────────────────┐
          │    └──────┬───────┘                  │
          │           │                          │
          │    ┌──────┴──────┐                   │
          │    │             │                   │
          │    ▼             ▼                   │
          │ ┌────────────┐ ┌────────────────┐    │
          │ │INTERRUPTIBLE│ │UNINTERRUPTIBLE│    │
          │ └──────┬─────┘ └───────┬────────┘    │
          │        │               │             │
          │        │ wake_up       │ wake_up     │
          │        └───────────────┴─────────────┘
          │                                      │
          │ stop/ptrace                          │
          ▼                                      │
    ┌──────────────┐                             │
    │ TASK_STOPPED │─────────────────────────────┘
    │ TASK_TRACED  │    SIGCONT
    └──────────────┘
          │
          │ exit()
          ▼
    ┌──────────────┐
    │ EXIT_ZOMBIE  │─────► wait() ─────► EXIT_DEAD
    └──────────────┘
```

## 进程层次结构

```
进程树结构（以init/systemd为根）：

                init (PID 1)
               /     |      \
           sshd    cron    systemd-logind
           /  \      |
        bash  bash   cron job
         |
       ls (子进程)
```

## 进程调度概览

```
调度类优先级（高→低）：
┌─────────────────────────────────────────┐
│ stop_sched_class   │ CPU停止任务        │
├─────────────────────────────────────────┤
│ dl_sched_class     │ 截止时间调度       │
├─────────────────────────────────────────┤
│ rt_sched_class     │ 实时调度 (FIFO/RR) │
├─────────────────────────────────────────┤
│ fair_sched_class   │ 完全公平调度 (CFS) │
├─────────────────────────────────────────┤
│ idle_sched_class   │ 空闲任务           │
└─────────────────────────────────────────┘
```

## 进程创建流程

```
fork/clone()
     │
     ▼
┌─────────────┐
│ 分配PID     │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ 复制task_   │
│ struct      │
└──────┬──────┘
       │
       ├──────────────┬──────────────┐
       ▼              ▼              ▼
┌─────────────┐ ┌─────────────┐ ┌─────────────┐
│ 复制/共享   │ │ 复制/共享   │ │ 复制/共享   │
│ mm_struct   │ │ files_struct│ │ fs_struct   │
└─────────────┘ └─────────────┘ └─────────────┘
       │
       ▼
┌─────────────┐
│ 加入调度器  │
│ 运行队列    │
└─────────────┘
```

## 进程上下文切换

```c
// 上下文切换核心函数
context_switch(prev, next) {
    // 1. 切换地址空间
    switch_mm(prev->mm, next->mm);

    // 2. 切换内核栈和寄存器
    switch_to(prev, next, prev);
}
```

## 常用进程操作

### 用户态

```c
// 进程创建
pid_t fork(void);          // 创建子进程
pid_t vfork(void);         // 共享地址空间的fork
int clone(int (*fn)(void*), void *stack, int flags, void *arg);

// 进程执行
int execve(const char *pathname, char *const argv[], char *const envp[]);

// 进程终止
void exit(int status);
pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);

// 进程控制
pid_t getpid(void);        // 获取进程ID
pid_t getppid(void);       // 获取父进程ID
int nice(int inc);         // 调整优先级
```

### 内核态

```c
// 进程创建
struct task_struct *copy_process(unsigned long flags, ...);

// 进程终止
void do_exit(long code);
void do_group_exit(int exit_code);

// 调度相关
void schedule(void);
void yield(void);
```

## 进程查看命令

```bash
# 查看进程
ps aux                    # 所有进程
ps -ef                    # 完整格式
ps -eLf                   # 显示线程

# 进程树
pstree -p                 # 树形显示

# 动态监控
top                       # 实时监控
htop                      # 增强版top

# 进程信息
cat /proc/<pid>/status    # 进程状态
cat /proc/<pid>/stat      # 统计信息
cat /proc/<pid>/maps      # 内存映射
cat /proc/<pid>/fd        # 文件描述符
```

## 源码位置

```
kernel/
├── fork.c           # 进程创建
├── exit.c           # 进程终止
├── sched/           # 调度器
│   ├── core.c       # 调度核心
│   ├── fair.c       # CFS调度器
│   ├── rt.c         # 实时调度器
│   └── deadline.c   # 截止时间调度器
├── signal.c         # 信号处理
├── pid.c            # PID管理
└── sys.c            # 系统调用

include/linux/
├── sched.h          # task_struct定义
├── pid.h            # PID相关
└── sched/           # 调度相关头文件
```

## 🧪 实验

> [!important] 实践是检验理解的最好方式
> 详见 [[实验指南-进程与中断|实验指南]]

| 实验 | 内容 | 难度 |
|------|------|------|
| 实验一 | 进程状态观察 | ⭐ |
| 实验二 | 进程调度分析 | ⭐⭐ |
| 实验三 | 上下文切换分析 | ⭐⭐ |

## 🔗 相关链接

- [[../03.内存管理/Linux内存管理|内存管理]] - mm_struct
- [[../05.系统调用/系统调用机制|系统调用]] - fork/exec实现
- [[../04.中断与异常/Linux中断与异常|中断与异常]] - 进程切换
