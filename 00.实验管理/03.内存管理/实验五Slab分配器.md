## 实验五：Slab分配器分析

### 观察结果

#### 5.1 Slab信息概览

**系统Slab内存统计：**
```
Slab:           405,256 KB
SReclaimable:   191,684 KB (可回收)
SUnreclaim:     213,572 KB (不可回收)
```

**活跃对象TOP10：**

| 缓存名称 | 活跃对象 | 总对象 | 对象大小 | 用途 |
|---------|---------|-------|---------|------|
| dentry | 181,649 | 192,822 | 192B | 目录项缓存 |
| trace_event_file | 122,905 | 123,732 | 96B | 跟踪事件 |
| buffer_head | 90,055 | 119,652 | 104B | 块设备缓冲 |
| ext4_inode_cache | 79,179 | 80,127 | 1128B | ext4 inode |
| kernfs_node_cache | 76,347 | 76,440 | 136B | kernfs节点 |
| extent_status | 55,642 | 60,282 | 40B | ext4范围状态 |
| dmaengine-unmap-2 | 50,276 | 50,432 | 64B | DMA引擎 |
| radix_tree_node | 36,690 | 39,340 | 584B | 基数树节点 |
| vm_area_struct | 37,333 | 39,102 | 192B | VMA结构 |
| kmalloc-rnd-15-512 | 30,786 | 31,984 | 512B | 通用分配 |

**重要内核结构缓存：**

| 缓存名称 | 对象数 | 对象大小 |
|---------|-------|---------|
| task_struct | 904 | 10,688 B |
| dentry | 181,635 | 192 B |
| inode_cache | 14,943 | 624 B |
| buffer_head | 90,055 | 104 B |
| mm_struct | 252 | 1,520 B |

#### 5.2 kmalloc缓存分析

系统提供了多种大小的kmalloc缓存：

```
kmalloc-8, kmalloc-16, kmalloc-32, kmalloc-64, kmalloc-96
kmalloc-128, kmalloc-192, kmalloc-256, kmalloc-512
kmalloc-1k, kmalloc-2k, kmalloc-4k, kmalloc-8k
```

**kmalloc缓存详情：**

| 缓存 | 对象数 | 对象大小 | slab大小 |
|------|-------|---------|----------|
| kmalloc-64 | 256 | 64B | 64 |
| kmalloc-128 | 128 | 128B | 128 |
| kmalloc-256 | 160 | 256B | 256 |
| kmalloc-512 | 176 | 512B | 512 |

#### 5.3 自定义Slab缓存实验

**内核模块创建自定义缓存：**
- 缓存名称: `my_test_cache`
- 对象大小: 256 字节
- 对齐方式: SLAB_HWCACHE_ALIGN (硬件缓存对齐)

**/proc/slabinfo输出：**
```
my_test_cache    2    25    320   25    2
```
- 活跃对象: 2
- 总对象: 25
- 实际对象大小: 320B (包含元数据)
- 每slab对象数: 25
- 每slab页数: 2

**/proc/my_slab_stat输出：**
```
缓存名称: my_test_cache
对象大小: 256 字节
已分配对象数: 5
缓存指针: 00000000c3c885ca

已分配的对象地址:
对象 3: 000000002e3af01b
对象 4: 000000008f95ad80
```

#### 5.4 perf Slab事件追踪

```
Performance counter stats for 'system wide':

        12,435      kmem:kmem_cache_alloc
        13,419      kmem:kmem_cache_free

   2.011563600 seconds time elapsed
```

每秒约6,200次slab分配和6,700次slab释放。

#### 5.5 Slab分配器工作原理

**SLUB分配器（当前使用）：**

1. **结构**：
   - 每个CPU维护本地per-CPU slabs
   - 减少锁竞争
   - 更好的NUMA性能

2. **对象分配**：
   ```
   kmem_cache_alloc(cache, GFP_KERNEL)
   -> 从per-CPU slab获取空闲对象
   -> 若无空闲，从伙伴系统申请新slab
   ```

3. **对象释放**：
   ```
   kmem_cache_free(cache, object)
   -> 将对象放回per-CPU slab
   -> slab全空时归还伙伴系统
   ```

4. **优势**：
   - 减少内存碎片
   - 快速分配（O(1)）
   - 对象缓存友好

### 遇到的问题与解决

1. **/proc/slabinfo权限**
   - 问题：普通用户无法读取
   - 解决：使用sudo读取

2. **perf tracepoint权限**
   - 问题：无法访问kmem tracepoints
   - 解决：`sudo mount -o remount,mode=755 /sys/kernel/tracing/`

3. **内核模块编译警告**
   - 警告：未使用的变量
   - 影响：无，编译成功

### 思考题

1. **为什么需要Slab分配器？**
   - 伙伴系统最小分配单位是一页(4KB)
   - 内核大量使用小对象（如task_struct、inode）
   - Slab提供高效的小对象管理

2. **kmalloc和kmem_cache_alloc的区别？**
   - kmalloc: 通用分配，按大小选择缓存
   - kmem_cache_alloc: 专用缓存，固定大小，更高效

3. **SLAB vs SLUB vs SLOB？**
   - SLAB: 最初实现，复杂但高效
   - SLUB: 简化版，更好的NUMA性能（当前使用）
   - SLOB: 极简版，用于嵌入式系统

### 关键发现

1. **dentry缓存最大**: 181,649个对象，用于目录项缓存加速文件查找
2. **task_struct很大**: 10,688字节，每个进程约占用10KB slab内存
3. **自定义缓存开销**: 请求256字节，实际320字节（25%开销用于元数据）

### 文件列表

```
exp5_slab/
├── exp5_slab_info.sh         # Slab信息查看脚本
├── exp5_slab_test.c          # 用户态测试程序
├── exp5_slab_test            # 编译后可执行文件
├── exp5_slab_module/
│   ├── slab_stat.c           # 内核模块源码
│   ├── Makefile              # 编译配置
│   └── slab_stat.ko          # 编译后模块
└── 实验五报告.md              # 本报告
```

### 进一步实验

- [ ] 分析slab内存泄漏
- [ ] 使用slub_debug调试
- [ ] 比较不同slab分配器性能