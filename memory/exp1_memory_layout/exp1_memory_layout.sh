#!/bin/bash
# 文件: exp1_memory_layout.sh
# 查看物理内存布局

echo "========== 物理内存信息 =========="
cat /proc/meminfo | head -20

echo -e "\n========== Zone信息 =========="
cat /proc/zoneinfo | grep -E "Node|zone|present|managed|free|min|low|high"

echo -e "\n========== NUMA信息 =========="
numactl --hardware 2>/dev/null || echo "UMA系统（单NUMA节点）"

echo -e "\n========== 伙伴系统状态 =========="
cat /proc/buddyinfo
