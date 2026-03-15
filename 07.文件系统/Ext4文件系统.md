---
tags:
  - Linux
  - Kernel
  - 文件系统
  - Ext4
created: 2026-03-15
status: 待学习
---

# Ext4文件系统

> [!info] 目标
> 深入理解Ext4文件系统的磁盘布局与核心机制

> [!note] 文件系统完整笔记
> 本笔记详细介绍Ext4，总览请参考 [[Linux文件系统|MOC]]

## Ext4概述

Ext4（Fourth Extended Filesystem）是Linux原生文件系统：

| 特性 | 值 |
|------|-----|
| 最大文件大小 | 16TB（默认4KB块） |
| 最大卷大小 | 1EB |
| 最大子目录数 | 无限制 |
| 最大文件名长度 | 255字节 |
| 日志 | 支持 |

### Ext4相比Ext3的改进

- **大文件支持**：支持更大的文件和卷
- **Extents**：替代传统块映射，减少碎片
- **延迟分配**：提高性能，减少碎片
- **快速fsck**：未分配块组不检查
- **日志校验**：提高可靠性
- **多块分配**：一次分配多个块

## 磁盘布局

### 整体结构

```
┌─────────────────────────────────────────────────────────────┐
│                    Ext4文件系统布局                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                    Block Group 0                     │   │
│  │  ┌─────────┬─────────┬─────────┬─────────┬────────┐ │   │
│  │  │Superblock│GDT      │Block    │Inode    │Data    │ │   │
│  │  │         │         │Bitmap   │Bitmap   │Blocks  │ │   │
│  │  └─────────┴─────────┴─────────┴─────────┴────────┘ │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                    Block Group 1                     │   │
│  │  ┌─────────┬─────────┬─────────┬─────────┬────────┐ │   │
│  │  │(备份)   │(备份)   │Block    │Inode    │Data    │ │   │
│  │  │Superblock│GDT     │Bitmap   │Bitmap   │Blocks  │ │   │
│  │  └─────────┴─────────┴─────────┴─────────┴────────┘ │   │
│  └─────────────────────────────────────────────────────┘   │
│                            ...                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                    Block Group N                     │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 块组结构

```
Block Group结构（详细）：

┌────────────────────────────────────────────────────────┐
│ Offset        │ 内容              │ 说明              │
├────────────────────────────────────────────────────────┤
│ 0x000         │ Superblock         │ 超级块（1024字节）│
│ 0x400         │ Group Descriptors  │ 块组描述符表     │
│               │ (GDT)              │                  │
├────────────────────────────────────────────────────────┤
│               │ Reserved GDT      │ 保留GDT（扩展用） │
├────────────────────────────────────────────────────────┤
│               │ Block Bitmap       │ 块位图           │
│               │                    │ （1块）          │
├────────────────────────────────────────────────────────┤
│               │ Inode Bitmap       │ inode位图        │
│               │                    │ （1块）          │
├────────────────────────────────────────────────────────┤
│               │ Inode Table        │ inode表          │
│               │                    │ （多块）         │
├────────────────────────────────────────────────────────┤
│               │ Data Blocks        │ 数据块           │
│               │                    │ （大部分空间）    │
└────────────────────────────────────────────────────────┘
```

## 超级块 (Superblock)

### 结构定义

```c
// fs/ext4/ext4.h
struct ext4_super_block {
    __le32 s_inodes_count;        // inode总数
    __le32 s_blocks_count_lo;     // 块总数（低32位）
    __le32 s_r_blocks_count_lo;   // 保留块数（低32位）
    __le32 s_free_blocks_count_lo;// 空闲块数（低32位）
    __le32 s_free_inodes_count;   // 空闲inode数
    __le32 s_first_data_block;    // 第一个数据块
    __le32 s_log_block_size;      // 块大小 = 1024 << s_log_block_size
    __le32 s_log_cluster_size;    // 簇大小
    __le32 s_blocks_per_group;    // 每组块数
    __le32 s_clusters_per_group;  // 每组簇数
    __le32 s_inodes_per_group;    // 每组inode数
    __le32 s_mtime;               // 最后挂载时间
    __le32 s_wtime;               // 最后写入时间
    __le16 s_mnt_count;           // 挂载次数
    __le16 s_max_mnt_count;       // 最大挂载次数
    __le16 s_magic;               // 魔数：0xEF53
    __le16 s_state;               // 文件系统状态
    __le16 s_errors;              // 错误处理方式
    // ... 更多字段
    __le32 s_feature_compat;      // 兼容特性
    __le32 s_feature_incompat;    // 不兼容特性
    __le32 s_feature_ro_compat;   // 只读兼容特性
    // ...
    __u8   s_uuid[16];            // UUID
    char   s_volume_name[16];     // 卷名
    // ...
};
```

### 查看超级块信息

```bash
# 使用dumpe2fs
dumpe2fs /dev/sda1 | head -50

# 使用tune2fs
tune2fs -l /dev/sda1

# 关键信息
Filesystem volume name:   <none>
Last mounted on:          /
Filesystem UUID:          xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
Filesystem magic number:  0xEF53
Filesystem revision #:    1 (dynamic)
Filesystem features:      has_journal, ext_attr, resize_inode, dir_index, ...
Default mount options:    user_xattr acl
Filesystem state:         clean
Errors behavior:          Continue
Filesystem OS type:       Linux
Inode count:              6553600
Block count:              26214400
Reserved block count:     1310720
Free blocks:              20000000
Free inodes:              6500000
First block:              0
Block size:               4096
```

## Inode结构

### Inode内容

```c
struct ext4_inode {
    __le16 i_mode;            // 文件类型和权限
    __le16 i_uid;             // 用户ID（低16位）
    __le32 i_size_lo;         // 文件大小（低32位）
    __le32 i_atime;           // 访问时间
    __le32 i_ctime;           // 状态改变时间
    __le32 i_mtime;           // 修改时间
    __le32 i_dtime;           // 删除时间
    __le16 i_gid;             // 组ID（低16位）
    __le16 i_links_count;     // 硬链接数
    __le32 i_blocks_lo;       // 块数（512字节块）
    __le32 i_flags;           // 文件标志
    // ...
    union {
        struct {
            __le32 l_i_version;   // 版本
        } linux1;
        // ...
    } osd1;
    __le32 i_block[EXT4_N_BLOCKS];  // 块指针或extent
    // ...
    __le32 i_generation;      // 文件版本（NFS用）
    __le32 i_file_acl_lo;     // 扩展属性块
    __le32 i_size_high;       // 文件大小（高32位）
    // ...
};
```

### i_block数组

```c
#define EXT4_N_BLOCKS  15

// 传统块映射（无extent特性时）
// i_block[0-11]  : 直接块指针（12个）
// i_block[12]    : 一级间接块
// i_block[13]    : 二级间接块
// i_block[14]    : 三级间接块

// Extent方式（现代Ext4默认）
// i_block存储extent树信息
```

## Extent机制

### 为什么使用Extent

```
传统块映射问题：
- 大文件需要大量块指针
- 1TB文件需要约1GB的块指针
- 碎片化严重

Extent解决方案：
- 一个extent描述一段连续的块
- 一个extent可表示多达128MB连续空间
- 减少元数据开销
```

### Extent结构

```c
// Extent头部
struct ext4_extent_header {
    __le16 eh_magic;      // 魔数：0xF30A
    __le16 eh_entries;    // 条目数
    __le16 eh_max;        // 最大条目数
    __le16 eh_depth;      // 树深度（0为叶子）
    __le32 eh_generation; // 生成号
};

// Extent条目（叶子节点）
struct ext4_extent {
    __le32 ee_block;      // 逻辑块号
    __le16 ee_len;        // 长度（块数）
    __le16 ee_start_hi;   // 物理块号（高16位）
    __le32 ee_start_lo;   // 物理块号（低32位）
};

// Extent索引（内部节点）
struct ext4_extent_idx {
    __le32 ei_block;      // 逻辑块号
    __le32 ei_leaf_lo;    // 子节点块号（低32位）
    __le16 ei_leaf_hi;    // 子节点块号（高16位）
    __u16 ei_unused;
};
```

### Extent树

```
                    ┌───────────────┐
                    │    Header     │
                    │  depth=2      │
                    │  entries=2    │
                    └───────┬───────┘
              ┌─────────────┴─────────────┐
              ▼                           ▼
     ┌───────────────┐           ┌───────────────┐
     │    Index      │           │    Index      │
     │  block=0-999  │           │ block=1000+   │
     └───────┬───────┘           └───────┬───────┘
             │                           │
             ▼                           ▼
    ┌───────────────┐           ┌───────────────┐
    │    Header     │           │    Header     │
    │  depth=1      │           │  depth=1      │
    └───────┬───────┘           └───────┬───────┘
            │                           │
            ▼                           ▼
    ┌───────────────┐           ┌───────────────┐
    │   Extent      │           │   Extent      │
    │ block=0       │           │ block=1000    │
    │ len=100       │           │ len=200       │
    │ start=5000    │           │ start=8000    │
    └───────────────┘           └───────────────┘
```

## 目录结构

### 目录项格式

```c
// 线性目录（传统）
struct ext4_dir_entry {
    __le32 inode;          // inode号
    __le16 rec_len;        // 目录项长度
    __le16 name_len;       // 文件名长度
    char   name[EXT4_NAME_LEN];  // 文件名
};

// 哈希树目录（大目录）
// 使用HTree索引加速查找
```

### 目录查找

```
查找 /home/user/file.txt

1. 读取根目录inode（通常为2）
2. 在根目录数据块中查找"home"
3. 获取home的inode号
4. 读取home目录数据
5. 查找"user"
6. 获取user的inode号
7. 在user目录中查找"file.txt"
8. 获取file.txt的inode号
9. 访问文件数据
```

## 日志机制

### 日志模式

| 模式 | 说明 | 性能 | 可靠性 |
|------|------|------|--------|
| journal | 记录所有数据+元数据 | 最低 | 最高 |
| ordered | 只记录元数据，数据先写 | 中等 | 高 |
| writeback | 只记录元数据，数据顺序不保证 | 最高 | 最低 |

### 日志结构

```
┌─────────────────────────────────────────────────────────┐
│                    Journal                               │
├─────────────────────────────────────────────────────────┤
│  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐       │
│  │Header   │ │Trans 1  │ │Trans 2  │ │Trans 3  │ ...   │
│  │(描述符) │ │(事务)   │ │(事务)   │ │(事务)   │       │
│  └─────────┘ └─────────┘ └─────────┘ └─────────┘       │
│                                                         │
│  每个事务包含：                                          │
│  - 块描述符                                             │
│  - 块数据                                               │
│  - 提交记录                                             │
└─────────────────────────────────────────────────────────┘
```

## 延迟分配 (Delalloc)

### 工作原理

```
传统分配：
write() → 立即分配块 → 写入数据

延迟分配：
write() → 只更新页缓存 →（延迟）
          ↓
sync/close → 批量分配块 → 写入数据

优势：
- 减少碎片（知道完整写入模式后再分配）
- 提高性能（批量分配）
- 更好的空间利用
```

## 常用工具

```bash
# 创建Ext4文件系统
mkfs.ext4 /dev/sda1
mkfs.ext4 -b 4096 -I 256 /dev/sda1  # 指定块大小和inode大小

# 查看信息
dumpe2fs /dev/sda1
tune2fs -l /dev/sda1

# 修改参数
tune2fs -c 30 /dev/sda1          # 30次挂载后检查
tune2fs -i 30d /dev/sda1         # 30天后检查
tune2fs -j /dev/sda1             # 添加日志（Ext2→Ext3）
tune2fs -O extents /dev/sda1     # 启用extent特性

# 检查修复
e2fsck /dev/sda1
e2fsck -f /dev/sda1              # 强制检查

# 调整大小
resize2fs /dev/sda1              # 扩展到最大
resize2fs /dev/sda1 10G          # 调整到10G

# 查看inode使用
df -i /mount/point
```

## 相关笔记

- [[VFS虚拟文件系统]]
- [[文件系统挂载]]
- [[Linux文件系统]]
