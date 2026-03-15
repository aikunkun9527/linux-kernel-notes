#!/bin/bash
# 文件: exp6_realtime_monitor.sh
# 实时监控内存回收活动

echo "========== 实时内存回收监控 =========="
echo "每秒刷新，按Ctrl+C退出"
echo ""

# 显示表头
printf "%-10s %12s %12s %12s %12s %12s\n" "时间" "MemFree" "MemAvail" "SwapFree" "kswapd_scan" "kswapd_steal"
printf "%-10s %12s %12s %12s %12s %12s\n" "----" "-------" "--------" "--------" "-----------" "-----------"

# 记录上次的值
last_scan=0
last_steal=0

while true; do
    # 获取时间
    time=$(date +%H:%M:%S)

    # 获取内存信息
    mem_free=$(grep MemFree /proc/meminfo | awk '{print $2}')
    mem_avail=$(grep MemAvailable /proc/meminfo | awk '{print $2}')
    swap_free=$(grep SwapFree /proc/meminfo | awk '{print $2}')

    # 获取vmstat
    scan=$(grep pgscan_kswapd /proc/vmstat | awk '{print $2}')
    steal=$(grep pgsteal_kswapd /proc/vmstat | awk '{print $2}')

    # 计算增量
    scan_diff=$((scan - last_scan))
    steal_diff=$((steal - last_steal))

    # 转换为MB
    mem_free_mb=$((mem_free / 1024))
    mem_avail_mb=$((mem_avail / 1024))
    swap_free_mb=$((swap_free / 1024))

    printf "%-10s %10dM %10dM %10dM %12d %12d\n" \
        "$time" "$mem_free_mb" "$mem_avail_mb" "$swap_free_mb" "$scan_diff" "$steal_diff"

    last_scan=$scan
    last_steal=$steal

    sleep 1
done
