---
tags:
  - MOC
  - Linux
  - Kernel
  - 文件系统
created: 2026-03-15
status: 学习中
---

# Linux文件系统

> [!info] 概述
> Linux文件系统通过VFS（虚拟文件系统）提供统一的文件访问接口，支持多种文件系统共存

## 🧠 核心概念

### VFS的作用

```
┌─────────────────────────────────────────────────────────────┐
│                     用户空间应用                             │
│   cat, ls, vim, grep, 程序...                               │
├─────────────────────────────────────────────────────────────┤
│                    系统调用层                                │
│              open, read, write, close...                    │
├─────────────────────────────────────────────────────────────┤
│                     VFS层                                   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  superblock缓存  │  inode缓存  │  dentry缓存  │     │   │
│  └─────────────────────────────────────────────────────┘   │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  page cache (页缓存)                                 │   │
│  └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│               具体文件系统实现                              │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐          │
│  │  Ext4   │ │   XFS   │ │  Btrfs  │ │   NFS   │          │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘          │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐          │
│  │  FAT32  │ │  NTFS   │ │  tmpfs  │ │ procfs  │          │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘          │
└─────────────────────────────────────────────────────────────┘
```

### 文件系统分类

```
Linux文件系统
├── 磁盘文件系统
│   ├── Ext2/Ext3/Ext4    # Linux原生
│   ├── XFS                # SGI开发，高性能
│   ├── Btrfs              # CoW，快照支持
│   ├── JFS                # IBM开发
│   └── FAT32/NTFS         # Windows兼容
│
├── 网络文件系统
│   ├── NFS                # Network File System
│   ├── CIFS/SMB           # Windows共享
│   └── CephFS             # 分布式存储
│
├── 内存文件系统
│   ├── tmpfs              # 内存临时文件
│   ├── ramfs              # 简单内存文件系统
│   └── hugetlbfs          # 大页文件系统
│
└── 伪文件系统
    ├── procfs             # 进程信息
    ├── sysfs              # 设备模型
    ├── debugfs            # 调试信息
    └── devtmpfs           # 设备文件
```

## 📚 学习路径

### 第一层：VFS框架
- [[VFS虚拟文件系统]] - 核心数据结构与操作
- [[文件系统挂载]] - 挂载流程与机制

### 第二层：具体文件系统
- [[Ext4文件系统]] - Ext4磁盘布局与特性

### 第三层：高级主题
- 页缓存机制
- 文件锁
- 日志与恢复

## VFS核心对象

### 对象关系图

```
┌─────────────────────────────────────────────────────────────┐
│                       superblock                            │
│                    (文件系统实例)                            │
│                          │                                  │
│            ┌─────────────┼─────────────┐                   │
│            │             │             │                    │
│            ▼             ▼             ▼                    │
│      ┌─────────┐   ┌─────────┐   ┌─────────┐              │
│      │ inode   │   │ inode   │   │ inode   │  ...         │
│      │ (文件1) │   │ (文件2) │   │ (目录1) │              │
│      └────┬────┘   └────┬────┘   └────┬────┘              │
│           │             │             │                    │
│           ▼             ▼             ▼                    │
│      ┌─────────┐   ┌─────────┐   ┌─────────┐              │
│      │ dentry  │   │ dentry  │   │ dentry  │  ...         │
│      │(路径组件)│   │(路径组件)│   │(路径组件)│              │
│      └────┬────┘   └────┬────┘   └────┬────┘              │
│           │             │             │                    │
│           ▼             ▼             ▼                    │
│      ┌─────────┐   ┌─────────┐   ┌─────────┐              │
│      │  file   │   │  file   │   │  file   │  ...         │
│      │(打开实例)│   │(打开实例)│   │(打开实例)│              │
│      └─────────┘   └─────────┘   └─────────┘              │
└─────────────────────────────────────────────────────────────┘
```

### 四大核心对象

| 对象 | 说明 | 主要内容 |
|------|------|----------|
| superblock | 文件系统实例 | 块大小、inode数、操作函数表 |
| inode | 文件元数据 | 权限、大小、时间、数据块指针 |
| dentry | 目录项 | 文件名、父子关系、inode指针 |
| file | 打开的文件 | 文件位置、打开标志、操作函数 |

### 操作函数表

| 操作表 | 所属对象 | 主要操作 |
|--------|----------|----------|
| super_operations | superblock | alloc_inode, write_inode, put_super |
| inode_operations | inode | create, link, unlink, mkdir, lookup |
| dentry_operations | dentry | d_compare, d_hash, d_delete |
| file_operations | file | read, write, mmap, open, release |

## 文件系统注册与挂载

### 注册文件系统

```c
// 注册文件系统类型
struct file_system_type {
    const char *name;           // 文件系统名称
    int fs_flags;               // 标志
    struct dentry *(*mount)(struct file_system_type *, int,
                            const char *, void *);
    void (*kill_sb)(struct super_block *);
    struct module *owner;
    // ...
};

// 注册
int register_filesystem(struct file_system_type *fs);
void unregister_filesystem(struct file_system_type *fs);
```

### 挂载流程

```
mount -t ext4 /dev/sda1 /mnt
            │
            ▼
    ┌───────────────────┐
    │ sys_mount()       │
    └─────────┬─────────┘
              │
              ▼
    ┌───────────────────┐
    │ 查找file_system   │
    │ _type (ext4)      │
    └─────────┬─────────┘
              │
              ▼
    ┌───────────────────┐
    │ 调用mount回调     │
    │ ext4_mount()      │
    └─────────┬─────────┘
              │
              ▼
    ┌───────────────────┐
    │ 读取超级块        │
    │ 建立VFS结构       │
    └─────────┬─────────┘
              │
              ▼
    ┌───────────────────┐
    │ 添加到挂载命名空间│
    └───────────────────┘
```

## 页缓存

### 地址空间

```c
// 每个inode关联一个地址空间
struct address_space {
    struct inode *host;         // 所属inode
    struct radix_tree_root i_pages; // 页缓存树
    unsigned long nrpages;      // 页数
    const struct address_space_operations *a_ops;
    // ...
};

struct address_space_operations {
    int (*writepage)(struct page *page, struct writeback_control *wbc);
    int (*readpage)(struct file *, struct page *);
    int (*writepages)(struct address_space *, struct writeback_control *);
    int (*set_page_dirty)(struct page *page);
    // ...
};
```

### 读写流程

```
read() 系统调用
     │
     ▼
┌─────────────────┐
│ 查找页缓存      │
│ (radix tree)    │
└────────┬────────┘
         │
    ┌────┴────┐
    │命中     │未命中
    ▼         ▼
返回数据  ┌─────────────────┐
         │ 分配新页         │
         │ 加入页缓存       │
         └────────┬────────┘
                  │
                  ▼
         ┌─────────────────┐
         │ 调用readpage    │
         │ 从磁盘读取       │
         └────────┬────────┘
                  │
                  ▼
              返回数据
```

## 常用命令

```bash
# 查看文件系统
df -h                      # 磁盘使用
df -i                      # inode使用
du -sh /path               # 目录大小

# 查看挂载
mount                      # 所有挂载
cat /proc/mounts           # 挂载信息
cat /proc/filesystems      # 支持的文件系统

# 文件系统操作
mkfs.ext4 /dev/sda1        # 创建Ext4
fsck.ext4 /dev/sda1        # 检查修复
tune2fs -l /dev/sda1       # 查看参数

# 挂载/卸载
mount -t ext4 /dev/sda1 /mnt
umount /mnt

# 查看inode信息
stat filename
ls -li                     # 显示inode号

# 页缓存
free -h
cat /proc/meminfo | grep -i cache
sync                       # 刷新缓存到磁盘
```

## 文件系统特性对比

| 特性 | Ext4 | XFS | Btrfs |
|------|------|-----|-------|
| 最大文件 | 16TB | 8EB | 16EB |
| 最大卷 | 1EB | 8EB | 16EB |
| 日志 | ✓ | ✓ | ✓ |
| 快照 | ✗ | ✓ | ✓ |
| 压缩 | ✗ | ✗ | ✓ |
| 去重 | ✗ | ✗ | ✓ |
| 在线扩展 | ✓ | ✓ | ✓ |
| 在线收缩 | ✓ | ✗ | ✓ |

## 源码位置

```
fs/
├── open.c              # open系统调用
├── read_write.c        # read/write系统调用
├── dcache.c            # dentry缓存
├── inode.c             # inode操作
├── super.c             # superblock操作
├── namei.c             # 路径查找
├── block_dev.c         # 块设备文件
├── char_dev.c          # 字符设备文件
├── pipe.c              # 管道
├── eventpoll.c         # epoll
├── ext4/               # Ext4文件系统
├── xfs/                # XFS文件系统
├── btrfs/              # Btrfs文件系统
├── nfs/                # NFS
├── proc/               # procfs
├── sysfs/              # sysfs
└── tmpfs.c             # tmpfs

include/linux/
├── fs.h                # 文件系统核心定义
├── file.h              # file结构
├── dcache.h            # dentry相关
└── pagemap.h           # 页缓存
```

## 🧪 实验

> [!important] 实践是检验理解的最好方式
> 详见 [[文件系统实验|实验指南]]

| 实验 | 内容 | 难度 |
|------|------|------|
| 实验一 | VFS对象分析 | ⭐ |
| 实验二 | Ext4磁盘结构 | ⭐⭐ |
| 实验三 | 文件系统挂载 | ⭐⭐ |
| 实验四 | 页缓存观察 | ⭐⭐ |

## 🔗 相关链接

- [[../03.内存管理/Linux内存管理|内存管理]] - 页缓存、内存映射
- [[../08.设备驱动/字符设备驱动|字符设备驱动]] - 设备文件
- [[../08.设备驱动/块设备驱动|块设备驱动]] - 块设备与文件系统
