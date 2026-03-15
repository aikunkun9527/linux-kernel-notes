#!/usr/bin/env python3
# 文件: exp2_vma_analyzer.py
# 分析进程VMA统计

import sys
import os
from collections import defaultdict

def analyze_vma(pid):
    try:
        with open(f'/proc/{pid}/maps', 'r') as f:
            lines = f.readlines()
    except Exception as e:
        print(f"无法读取进程 {pid} 的内存映射: {e}")
        return

    stats = defaultdict(lambda: {'count': 0, 'size': 0})

    for line in lines:
        parts = line.split()
        addr_range = parts[0]
        perms = parts[1]

        start, end = [int(x, 16) for x in addr_range.split('-')]
        size = end - start

        # 分类
        if len(parts) > 5:
            path = parts[5]
            if '[' in path:
                category = path
            elif '.so' in path:
                category = '共享库'
            else:
                category = '文件映射'
        elif 'r-xp' in perms:
            category = '代码段'
        elif 'rw-p' in perms:
            category = '数据段/堆'
        elif 'r--p' in perms:
            category = '只读数据'
        else:
            category = '其他'

        stats[category]['count'] += 1
        stats[category]['size'] += size

    print(f"\n进程 {pid} VMA统计:")
    print("=" * 60)
    print(f"{'类别':<20} {'数量':>6} {'大小(MB)':>12}")
    print("-" * 60)

    total_size = 0
    for cat, data in sorted(stats.items(), key=lambda x: -x[1]['size']):
        size_mb = data['size'] / (1024 * 1024)
        total_size += data['size']
        print(f"{cat:<20} {data['count']:>6} {size_mb:>12.2f}")

    print("-" * 60)
    print(f"{'总计':<20} {len(lines):>6} {total_size/(1024*1024):>12.2f}")

if __name__ == '__main__':
    if len(sys.argv) > 1:
        pid = sys.argv[1]
    else:
        pid = str(os.getpid())

    analyze_vma(pid)
