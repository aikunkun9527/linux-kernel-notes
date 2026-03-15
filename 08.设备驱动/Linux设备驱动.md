---
tags:
  - MOC
  - Linux
  - Kernel
  - 驱动开发
created: 2026-03-15
status: 学习中
---

# Linux设备驱动

> [!info] 概述
> 设备驱动是内核与硬件之间的桥梁，负责管理硬件设备并提供统一的访问接口

## 🧠 核心概念

### 驱动的作用

```
┌─────────────────────────────────────────────────────────────┐
│                     用户空间应用                             │
│   open(), read(), write(), ioctl(), close()                 │
├─────────────────────────────────────────────────────────────┤
│                     系统调用层                              │
├─────────────────────────────────────────────────────────────┤
│                     VFS层                                   │
│              file_operations 接口                           │
├─────────────────────────────────────────────────────────────┤
│                     设备驱动层                              │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐           │
│  │ 字符设备驱动 │ │ 块设备驱动  │ │ 网络设备驱动 │           │
│  └─────────────┘ └─────────────┘ └─────────────┘           │
├─────────────────────────────────────────────────────────────┤
│                     设备模型                                │
│         kobject, kset, sysfs, uevent                       │
├─────────────────────────────────────────────────────────────┤
│                     硬件层                                  │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐ ┌─────┐                  │
│  │串口 │ │磁盘 │ │网卡 │ │显卡 │ │USB  │  ...              │
│  └─────┘ └─────┘ └─────┘ └─────┘ └─────┘                  │
└─────────────────────────────────────────────────────────────┘
```

### 设备分类

| 类型 | 特点 | 访问方式 | 示例 |
|------|------|----------|------|
| 字符设备 | 字节流访问 | 顺序/随机 | 串口、键盘、鼠标 |
| 块设备 | 块访问 | 随机访问 | 硬盘、SSD、U盘 |
| 网络设备 | 包传输 | 协议栈 | 网卡、WiFi |

### 设备号

```c
// 设备号 = 主设备号(12位) + 次设备号(20位)
dev_t dev = MKDEV(major, minor);

int major = MAJOR(dev);   // 标识驱动
int minor = MINOR(dev);   // 标识具体设备
```

## 📚 学习路径

### 第一层：字符设备驱动
- [[字符设备驱动]] - cdev、file_operations、ioctl
- 设备注册与自动创建

### 第二层：块设备驱动
- [[块设备驱动]] - gendisk、request_queue
- 块I/O请求处理

### 第三层：设备模型
- [[设备模型]] - kobject、kset、sysfs
- 平台设备驱动

## 设备类型详解

### 字符设备

```
特点：
- 按字节流访问
- 通常不支持seek
- 通过file_operations访问

注册流程：
alloc_chrdev_region() → cdev_init() → cdev_add()

示例设备：
- /dev/ttyS0    串口
- /dev/input/*  输入设备
- /dev/null     空设备
- /dev/random   随机数
```

### 块设备

```
特点：
- 按块(512B-4KB)访问
- 支持随机访问
- 有缓存机制

注册流程：
alloc_disk() → 初始化 → add_disk()

示例设备：
- /dev/sda     SCSI/SATA硬盘
- /dev/nvme*   NVMe SSD
- /dev/loop*   回环设备
- /dev/ram*    RAM磁盘
```

### 网络设备

```
特点：
- 按数据包传输
- 通过协议栈访问
- 没有设备节点

关键结构：
- struct net_device
- struct sk_buff

示例：
- eth0, wlan0  物理网卡
- lo          回环接口
- docker0     桥接接口
- veth*       虚拟网口
```

## file_operations核心操作

```c
struct file_operations {
    struct module *owner;

    // 定位
    loff_t (*llseek)(struct file *, loff_t, int);

    // 读写
    ssize_t (*read)(struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write)(struct file *, const char __user *, size_t, loff_t *);
    ssize_t (*read_iter)(struct kiocb *, struct iov_iter *);
    ssize_t (*write_iter)(struct kiocb *, struct iov_iter *);

    // 映射
    int (*mmap)(struct file *, struct vm_area_struct *);

    // 打开/关闭
    int (*open)(struct inode *, struct file *);
    int (*flush)(struct file *, fl_owner_t id);
    int (*release)(struct inode *, struct file *);

    // 同步
    int (*fsync)(struct file *, loff_t, loff_t, int datasync);
    int (*fasync)(int, struct file *, int);

    // I/O控制
    long (*unlocked_ioctl)(struct file *, unsigned int, unsigned long);
    long (*compat_ioctl)(struct file *, unsigned int, unsigned long);

    // 轮询
    __poll_t (*poll)(struct file *, struct poll_table_struct *);

    // 异步I/O
    int (*fasync)(int, struct file *, int);
};
```

## 用户空间与内核空间数据传输

### 安全拷贝函数

```c
#include <linux/uaccess.h>

// 复制到用户空间
unsigned long copy_to_user(void __user *to, const void *from, unsigned long n);

// 从用户空间复制
unsigned long copy_from_user(void *to, const void __user *from, unsigned long n);

// 单值操作
int get_user(x, const void __user *ptr);
int put_user(x, void __user *ptr);

// 安全检查
bool access_ok(const void __user *addr, size_t size);
```

### 示例

```c
static ssize_t my_read(struct file *filp, char __user *buf,
                       size_t count, loff_t *f_pos)
{
    char kbuf[128];
    size_t len;

    // 1. 检查用户空间指针
    if (!access_ok(buf, count))
        return -EFAULT;

    // 2. 准备内核数据
    len = min(count, sizeof(kbuf));
    memcpy(kbuf, "Hello from kernel", len);

    // 3. 复制到用户空间
    if (copy_to_user(buf, kbuf, len))
        return -EFAULT;

    return len;
}
```

## 并发与同步

### 驱动中的并发来源

```
1. 多进程同时访问设备
2. 中断打断进程上下文
3. SMP多CPU并发
4. 抢占调度
```

### 常用同步方法

```c
// 自旋锁（短临界区、中断上下文）
spinlock_t lock;
spin_lock(&lock);
// 临界区
spin_unlock(&lock);

// 互斥锁（长临界区、可睡眠）
struct mutex mtx;
mutex_lock(&mtx);
// 临界区
mutex_unlock(&mtx);

// 原子操作
atomic_t count = ATOMIC_INIT(0);
atomic_inc(&count);

// 完成量
struct completion comp;
init_completion(&comp);
wait_for_completion(&comp);
complete(&comp);
```

## 中断处理

### 中断注册

```c
#include <linux/interrupt.h>

// 中断处理函数
irqreturn_t my_handler(int irq, void *dev_id)
{
    // 快速处理
    return IRQ_HANDLED;
}

// 注册中断
int request_irq(unsigned int irq, irq_handler_t handler,
                unsigned long flags, const char *name, void *dev);

// 释放中断
void free_irq(unsigned int irq, void *dev);
```

### 中断下半部

```c
// Tasklet
DECLARE_TASKLET(my_tasklet, tasklet_func, data);
tasklet_schedule(&my_tasklet);

// 工作队列
struct work_struct my_work;
INIT_WORK(&my_work, work_func);
schedule_work(&my_work);

// 线程化中断
request_threaded_irq(irq, handler, thread_fn, flags, name, dev);
```

## 设备树 (Device Tree)

### 概述

```
设备树是描述硬件的数据结构：
- 替代平台代码中的硬编码
- 支持设备热插拔
- 分离硬件描述与驱动代码

文件格式：.dts (源码) → .dtb (二进制)
```

### 基本结构

```dts
/ {
    compatible = "vendor,board";

    cpus {
        cpu@0 {
            compatible = "arm,cortex-a53";
            reg = <0x0>;
        };
    };

    memory@40000000 {
        reg = <0x40000000 0x20000000>;
    };

    uart@101f0000 {
        compatible = "vendor,uart";
        reg = <0x101f0000 0x1000>;
        interrupts = <12>;
    };
};
```

### 驱动中解析设备树

```c
// 匹配表
static const struct of_device_id my_of_match[] = {
    { .compatible = "vendor,mydevice", },
    { }
};
MODULE_DEVICE_TABLE(of, my_of_match);

// probe函数中获取资源
static int my_probe(struct platform_device *pdev)
{
    struct resource *res;
    void __iomem *base;
    int irq;

    // 获取内存资源
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    base = devm_ioremap_resource(&pdev->dev, res);

    // 获取中断资源
    irq = platform_get_irq(pdev, 0);

    // 获取设备树属性
    u32 value;
    of_property_read_u32(pdev->dev.of_node, "my-property", &value);

    return 0;
}
```

## 常用调试方法

### 内核日志

```c
// 日志级别
pr_emerg("Emergency\n");    // 最高优先级
pr_alert("Alert\n");
pr_crit("Critical\n");
pr_err("Error\n");
pr_warn("Warning\n");
pr_notice("Notice\n");
pr_info("Information\n");
pr_debug("Debug\n");        // 最低优先级

// 动态调试
pr_debug("Dynamic debug\n");  // CONFIG_DYNAMIC_DEBUG
```

```bash
# 查看日志
dmesg
dmesg | tail -20
cat /proc/kmsg

# 控制日志级别
cat /proc/sys/kernel/printk
echo "8 4 1 7" > /proc/sys/kernel/printk
```

### 动态调试

```bash
# 启用动态调试
echo "module mydriver +p" > /sys/kernel/debug/dynamic_debug/control

# 启用文件中所有调试
echo "file mydriver.c +p" > /sys/kernel/debug/dynamic_debug/control

# 启用函数调试
echo "func my_probe +p" > /sys/kernel/debug/dynamic_debug/control
```

### /proc和sysfs

```c
// 创建/proc接口
#include <linux/proc_fs.h>

struct proc_dir_entry *entry;
entry = proc_create("mydriver", 0444, NULL, &proc_fops);

// 创建sysfs属性
#include <linux/sysfs.h>

static DEVICE_ATTR(myattr, 0644, my_show, my_store);
device_create_file(dev, &dev_attr_myattr);
```

## 源码位置

```
drivers/
├── char/           # 字符设备驱动
├── block/          # 块设备驱动
├── net/            # 网络设备驱动
├── gpio/           # GPIO驱动
├── i2c/            # I2C驱动
├── spi/            # SPI驱动
├── usb/            # USB驱动
├── pci/            # PCI驱动
├── platform/       # 平台驱动
└── base/           # 设备模型核心

include/linux/
├── fs.h            # file_operations
├── cdev.h          # 字符设备
├── blkdev.h        # 块设备
├── device.h        # 设备模型
├── platform_device.h  # 平台设备
└── of.h            # 设备树
```

## 🧪 实验

> [!important] 实践是检验理解的最好方式
> 详见 [[设备驱动实验|实验指南]]

| 实验 | 内容 | 难度 |
|------|------|------|
| 实验一 | 字符设备驱动开发 | ⭐⭐ |
| 实验二 | ioctl与poll实现 | ⭐⭐ |
| 实验三 | 中断处理 | ⭐⭐⭐ |
| 实验四 | 平台设备驱动 | ⭐⭐⭐ |

## 🔗 相关链接

- [[../06.同步机制/Linux内核同步机制|同步机制]] - 并发保护
- [[../04.中断与异常/Linux中断与异常|中断与异常]] - 中断处理
- [[../07.文件系统/Linux文件系统|文件系统]] - VFS接口
