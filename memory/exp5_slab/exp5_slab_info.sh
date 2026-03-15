#!/bin/bash
# 文件: exp5_slab_info.sh
# 查看Slab信息

echo "========== Slab信息概览 =========="
echo "Slab分配器用于小对象分配，减少内存碎片"
echo ""

echo "========== /proc/slabinfo 头部 =========="
sudo head -1 /proc/slabinfo
echo ""
echo "说明: name <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab>"
echo ""

echo "========== 活跃对象TOP15 =========="
sudo cat /proc/slabinfo | sort -k3 -rn | head -16

echo -e "\n========== 按对象大小排序 (大对象TOP10) =========="
sudo cat /proc/slabinfo | sort -k5 -rn | head -11

echo -e "\n========== kmalloc缓存列表 =========="
ls /sys/kernel/slab/ | grep -E "^kmalloc" | sort -V

echo -e "\n========== kmalloc缓存详情 =========="
for size in 64 128 256 512 1024 2048; do
    cache="kmalloc-$size"
    if [ -d "/sys/kernel/slab/$cache" ]; then
        objects=$(cat /sys/kernel/slab/$cache/objects 2>/dev/null || echo "N/A")
        obj_size=$(cat /sys/kernel/slab/$cache/object_size 2>/dev/null || echo "N/A")
        slab_size=$(cat /sys/kernel/slab/$cache/slab_size 2>/dev/null || echo "N/A")
        printf "%-15s: objects=%-10s obj_size=%-8s slab_size=%s\n" "$cache" "$objects" "$obj_size" "$slab_size"
    fi
done

echo -e "\n========== 重要内核结构缓存 =========="
for cache in task_struct dentry inode_cache buffer_head mm_struct files_struct; do
    if [ -d "/sys/kernel/slab/$cache" ]; then
        objects=$(cat /sys/kernel/slab/$cache/objects 2>/dev/null || echo "N/A")
        obj_size=$(cat /sys/kernel/slab/$cache/object_size 2>/dev/null || echo "N/A")
        active=$(echo $objects | cut -d'/' -f1)
        total=$(echo $objects | cut -d'/' -f2)
        printf "%-20s: %s (obj_size=%s bytes)\n" "$cache" "$objects" "$obj_size"
    fi
done

echo -e "\n========== Slab内存统计 =========="
echo "从 /proc/meminfo:"
grep -E "^Slab:" /proc/meminfo
grep -E "^SReclaimable:" /proc/meminfo
grep -E "^SUnreclaim:" /proc/meminfo

echo -e "\n========== vmstat slab统计 =========="
grep -E "^nr_slab" /proc/vmstat

echo -e "\n========== Slab分配器类型 =========="
if [ -f /sys/kernel/slab/kmalloc-64/ctor ]; then
    echo "当前系统使用: SLUB 分配器 (默认)"
fi
