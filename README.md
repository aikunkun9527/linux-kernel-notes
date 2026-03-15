# Linux内核学习笔记

> 系统学习Linux内核的学习笔记，涵盖核心子系统的原理与实践

## 📚 内容概览

### 阶段一：基础准备
- 开发环境搭建
- 内核源码获取与编译
- 内核调试技术

### 阶段二：核心子系统

#### 进程管理
- 进程描述符 task_struct
- 进程调度器 (CFS)
- 进程创建与销毁
- 进程状态与切换

#### 内存管理
- 内存寻址基础
- 内存区域与节点 (Node/Zone)
- 页面分配器 (Buddy System)
- Slab分配器
- 进程地址空间
- 内存映射 (mmap)
- 页面错误处理
- 内存回收

#### 中断与异常
- 中断处理机制
- 异常处理
- 软中断与Tasklet

#### 系统调用
- 系统调用机制
- 系统调用实现
- 系统调用入口详解

#### 同步机制
- 原子操作
- 自旋锁
- 互斥锁与信号量
- RCU机制

### 阶段三：高级主题
- 文件系统 (VFS, Ext4)
- 设备驱动
- 网络子系统

## 🧪 实验代码

每个子系统配套实验代码，位于 `00.实验管理/` 目录下。

## 🔗 参考资源

- [Linux内核源码](https://github.com/torvalds/linux)
- [Linux内核文档](https://www.kernel.org/doc/)
- [0voice/linux_kernel_wiki](https://github.com/0voice/linux_kernel_wiki)

## 📖 学习资源

推荐书籍：
- 《深入理解Linux内核》
- 《Linux内核设计与实现》
- 《深入Linux内核架构》

## 📜 License

MIT License

---

⭐ 如果觉得有帮助，欢迎 Star！
