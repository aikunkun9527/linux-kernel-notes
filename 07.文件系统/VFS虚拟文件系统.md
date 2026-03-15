---
tags:
  - Linux
  - Kernel
  - 文件系统
created: 2026-03-11
status: 待学习
---

# VFS虚拟文件系统

> [!info] 目标
> 理解Linux虚拟文件系统层的架构与实现

## VFS概述

VFS（Virtual File System）提供统一的文件访问接口：
- 屏蔽底层文件系统差异
- 支持多种文件系统共存
- 提供文件缓存机制

## VFS架构

```
┌──────────────────────────────────────────┐
│             用户空间应用程序               │
├──────────────────────────────────────────┤
│           open, read, write, close        │
├──────────────────────────────────────────┤
│               VFS层                       │
│  ┌─────────┐ ┌─────────┐ ┌─────────┐     │
│  │superblock│ │ inode  │ │ dentry │     │
│  │   缓存   │ │  缓存  │ │  缓存  │     │
│  └─────────┘ └─────────┘ └─────────┘     │
├──────────────────────────────────────────┤
│     Ext4    │    XFS    │    NFS    │    │
│     Btrfs   │   FAT32   │   tmpfs   │    │
└──────────────────────────────────────────┘
```

## 核心数据结构

### 1. superblock（超级块）

```c
struct super_block {
    struct list_head    s_list;         // 超级块链表
    dev_t               s_dev;          // 设备标识
    unsigned char       s_blocksize_bits; // 块大小位数
    unsigned long       s_blocksize;    // 块大小
    loff_t              s_maxbytes;     // 最大文件大小
    struct file_system_type *s_type;    // 文件系统类型
    const struct super_operations *s_op; // 超级块操作
    // ...
};
```

### 2. inode（索引节点）

```c
struct inode {
    umode_t             i_mode;     // 文件类型和权限
    unsigned long       i_ino;      // inode号
    uid_t               i_uid;      // 用户ID
    gid_t               i_gid;      // 组ID
    loff_t              i_size;     // 文件大小
    struct timespec64   i_atime;    // 访问时间
    struct timespec64   i_mtime;    // 修改时间
    const struct inode_operations *i_op; // inode操作
    const struct file_operations *i_fop; // 文件操作
    // ...
};
```

### 3. dentry（目录项）

```c
struct dentry {
    unsigned int d_flags;       // 目录项标志
    struct qstr d_name;         // 文件名
    struct inode *d_inode;      // 关联的inode
    struct dentry *d_parent;    // 父目录项
    struct list_head d_child;   // 子目录项链表
    struct list_head d_subdirs; // 子目录链表
    const struct dentry_operations *d_op; // 目录项操作
    // ...
};
```

### 4. file（打开文件）

```c
struct file {
    struct path             f_path;     // 路径
    struct inode            *f_inode;   // 关联的inode
    const struct file_operations *f_op; // 文件操作
    unsigned int            f_flags;    // 打开标志
    fmode_t                 f_mode;     // 访问模式
    loff_t                  f_pos;      // 文件位置
    // ...
};
```

## 操作函数表

### super_operations

```c
struct super_operations {
    struct inode *(*alloc_inode)(struct super_block *sb);
    void (*destroy_inode)(struct inode *);
    void (*dirty_inode)(struct inode *, int flags);
    int (*write_inode)(struct inode *, struct writeback_control *);
    int (*drop_inode)(struct inode *);
    void (*put_super)(struct super_block *);
    int (*sync_fs)(struct super_block *, int);
    // ...
};
```

### file_operations

```c
struct file_operations {
    loff_t (*llseek)(struct file *, loff_t, int);
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    int (*open)(struct inode *, struct file *);
    int (*release)(struct inode *, struct file *);
    int (*mmap)(struct file *, struct vm_area_struct *);
    // ...
};
```

## 文件打开流程

```
open("/path/to/file", flags)
         │
         ▼
    ┌─────────┐
    │  VFS    │
    └────┬────┘
         │ 查找路径
         ▼
    ┌─────────┐
    │ dentry  │ 查找目录项缓存
    │ cache   │
    └────┬────┘
         │ 未命中则调用文件系统的lookup
         ▼
    ┌─────────┐
    │ get_inode│ 获取/创建inode
    └────┬────┘
         │
         ▼
    ┌─────────┐
    │ fdget   │ 分配文件描述符
    └─────────┘
```

## 页缓存

VFS使用页缓存加速文件访问：

```bash
# 查看页缓存信息
free -h
cat /proc/meminfo | grep -i cache

# 清除页缓存
echo 3 > /proc/sys/vm/drop_caches
```

## 挂载文件系统

```c
// 挂载API
struct dentry *mount_fs(struct file_system_type *type,
                        int flags, const char *name, void *data);
```

```bash
# 挂载命令
mount -t ext4 /dev/sda1 /mnt

# 查看挂载信息
mount
cat /proc/mounts
```

## 相关笔记
- [[Ext4文件系统]]
- [[文件系统挂载]]
