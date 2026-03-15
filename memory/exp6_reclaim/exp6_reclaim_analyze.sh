#!/bin/bash
# 文件: exp6_reclaim_analyze.sh
# 分析内存回收机制

echo "========== 内存回收机制分析 =========="
echo ""

echo "1. 页面回收类型"
echo "   - 后台回收(kswapd): 空闲内存低于low水位时触发"
echo "   - 直接回收: 空闲内存低于min水位时，分配时同步回收"
echo ""

echo "2. 当前内存状态"
echo "----------------------------------------"
grep -E "MemTotal|MemFree|MemAvailable|Buffers|Cached" /proc/meminfo

echo ""
echo "3. Zone水位线分析"
echo "----------------------------------------"
awk '
/^Node/ {
    node=$2
    zone=$4
    gsub(/,/,"",zone)
}
/min / {min=$2}
/low / {low=$2}
/high / {high=$2}
/managed / {managed=$2}
/free / {
    free=$2
    printf "Zone %s:\n", zone
    printf "  managed: %d 页 (%.1f MB)\n", managed, managed*4/1024
    printf "  free:    %d 页 (%.1f MB)\n", free, free*4/1024
    printf "  min:     %d 页 (%.1f MB)\n", min, min*4/1024
    printf "  low:     %d 页 (%.1f MB)\n", low, low*4/1024
    printf "  high:    %d 页 (%.1f MB)\n", high, high*4/1024
    if (managed > 0) {
        util = (managed - free) * 100.0 / managed
        printf "  利用率:  %.1f%%\n", util
    }
    printf "\n"
}
' /proc/zoneinfo

echo "4. 页面回收统计"
echo "----------------------------------------"
echo "kswapd后台回收:"
grep -E "^pgscan_kswapd|^pgsteal_kswapd" /proc/vmstat

echo ""
echo "直接回收:"
grep -E "^pgscan_direct|^pgsteal_direct" /proc/vmstat

echo ""
echo "5. Swap活动"
echo "----------------------------------------"
pswpin=$(grep pswpin /proc/vmstat | awk '{print $2}')
pswpout=$(grep pswpout /proc/vmstat | awk '{print $2}')
echo "页面换入: $pswpin"
echo "页面换出: $pswpout"

swap_total=$(grep SwapTotal /proc/meminfo | awk '{print $2}')
swap_free=$(grep SwapFree /proc/meminfo | awk '{print $2}')
swap_used=$((swap_total - swap_free))
echo "Swap总量: ${swap_total} KB"
echo "Swap使用: ${swap_used} KB"
echo "Swap空闲: ${swap_free} KB"

echo ""
echo "6. 内存压力评估"
echo "----------------------------------------"
mem_total=$(grep MemTotal /proc/meminfo | awk '{print $2}')
mem_free=$(grep MemFree /proc/meminfo | awk '{print $2}')
mem_avail=$(grep MemAvailable /proc/meminfo | awk '{print $2}')

if [ "$mem_avail" -gt $((mem_total * 50 / 100)) ]; then
    echo "内存压力: 低 (可用内存 > 50%)"
elif [ "$mem_avail" -gt $((mem_total * 25 / 100)) ]; then
    echo "内存压力: 中 (可用内存 25-50%)"
elif [ "$mem_avail" -gt $((mem_total * 10 / 100)) ]; then
    echo "内存压力: 高 (可用内存 10-25%)"
else
    echo "内存压力: 严重 (可用内存 < 10%)"
fi

echo ""
echo "7. 预期回收行为"
echo "----------------------------------------"
for zone in Normal; do
    free=$(awk "/^Node.*zone.*$zone/,/^$/" /proc/zoneinfo | awk '/free/{print $2}')
    low=$(awk "/^Node.*zone.*$zone/,/^$/" /proc/zoneinfo | awk '/low/{print $2}')
    min=$(awk "/^Node.*zone.*$zone/,/^$/" /proc/zoneinfo | awk '/min/{print $2}')

    echo "Zone $zone:"
    if [ "$free" -lt "$min" ]; then
        echo "  -> 直接回收将被触发!"
    elif [ "$free" -lt "$low" ]; then
        echo "  -> kswapd正在后台回收"
    else
        echo "  -> 内存充足，无需回收"
    fi
done

echo ""
echo "8. 内核参数 (vm.swappiness)"
echo "----------------------------------------"
swappiness=$(cat /proc/sys/vm/swappiness)
echo "vm.swappiness = $swappiness"
echo "  0: 尽量避免使用swap"
echo "  60: 默认值，平衡使用swap"
echo "  100: 积极使用swap"
