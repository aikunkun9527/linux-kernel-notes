#!/bin/bash
# 文件: exp6_kswapd_monitor.sh
# 监控kswapd内核线程状态

echo "========== kswapd内核线程信息 =========="
echo ""

# 查看kswapd进程
echo "kswapd进程:"
ps -eo pid,comm,stat,pmem,vsz,rss | grep kswapd

echo ""
echo "========== kswapd运行状态 =========="

# 检查kswapd是否在运行
kswapd_running=$(ps -eo comm,stat | grep "kswapd0" | grep -c "R")
if [ "$kswapd_running" -gt 0 ]; then
    echo "状态: 正在运行 (R)"
else
    echo "状态: 睡眠中 (S)"
fi

echo ""
echo "========== kswapd统计 =========="

# 从/proc/vmstat获取kswapd统计
echo "页面扫描:"
grep pgscan_kswapd /proc/vmstat

echo ""
echo "页面回收:"
grep pgsteal_kswapd /proc/vmstat

echo ""
echo "kswapd唤醒次数:"
grep pgrefill /proc/vmstat

echo ""
echo "========== Zone水位线状态 =========="
echo "当空闲内存 < low水位时，kswapd开始后台回收"
echo "当空闲内存 < min水位时，触发直接回收"
echo ""

awk '
/^Node/ {node=$2; zone=$4; gsub(/,/,"",zone)}
/min / {min=$2}
/low / {low=$2}
/high / {high=$2}
/free / {
    free=$2
    printf "Node %s %s: free=%d, min=%d, low=%d, high=%d\n", node, zone, free, min, low, high
    if (free < min) printf "  [!] 低于min水位，触发直接回收!\n"
    else if (free < low) printf "  [!] 低于low水位，kswapd已激活\n"
    else if (free < high) printf "  [*] 低于high水位，kswapd可能运行\n"
    else printf "  [OK] 水位正常\n"
}
' /proc/zoneinfo

echo ""
echo "========== Swap使用情况 =========="
grep -E "SwapTotal|SwapFree|SwapCached" /proc/meminfo
