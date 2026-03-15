#!/bin/bash
# 文件: exp7_cow_monitor.sh
# 监控COW行为

echo "========== 写时复制（COW）机制分析 =========="
echo ""

echo "1. COW基本原理"
echo "----------------------------------------"
echo "当fork()创建子进程时："
echo "  - 父子进程共享相同的物理内存页"
echo "  - 页表项标记为只读"
echo "  - 当任一进程写入时，触发页错误"
echo "  - 内核复制该页，分配新的物理页"
echo "  - 修改进程的页表指向新页"
echo ""

echo "2. COW优势"
echo "----------------------------------------"
echo "  - fork()速度快：只复制页表，不复制物理内存"
echo "  - 节省内存：未修改的页面共享"
echo "  - 按需复制：只复制实际修改的页面"
echo ""

echo "3. 当前系统缺页统计"
echo "----------------------------------------"
grep -E "^pgfault|^pgmajfault|^pgcow" /proc/vmstat

echo ""
echo "4. COW触发条件"
echo "----------------------------------------"
echo "  - fork()后子进程写入共享页面"
echo "  - 父进程写入fork()前分配的页面"
echo "  - MAP_PRIVATE的mmap区域写入"
echo ""

echo "5. 观察COW的/proc接口"
echo "----------------------------------------"
echo "  /proc/[pid]/maps  - 进程内存映射"
echo "  /proc/[pid]/smaps - 详细内存统计（含Shared/Private）"
echo "  /proc/[pid]/pagemap - 页面映射信息"
echo "  /proc/[pid]/stat  - 进程统计（含缺页次数）"
echo ""

echo "6. 运行COW验证程序"
echo "----------------------------------------"
echo "  编译: gcc -o exp7_cow exp7_cow.c"
echo "  运行: ./exp7_cow"
echo ""
