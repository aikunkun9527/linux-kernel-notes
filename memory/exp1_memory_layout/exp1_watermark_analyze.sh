#!/bin/bash
# 文件: exp1_watermark_analyze.sh
# 分析每个Zone的水位线

echo "Zone水位线分析（单位：页）"
echo "================================"

grep -E "^Node|^  (min|low|high|present|managed|free)" /proc/zoneinfo | \
awk '
/^Node/ {
    node=$2
    zone=$4
    gsub(/,/,"",zone)
}
/min / {min=$2}
/low / {low=$2}
/high / {high=$2}
/present / {present=$2}
/managed / {managed=$2}
/free / {
    free=$2
    printf "Node %s, Zone %s:\n", node, zone
    printf "  present:  %8d 页 (%6.1f MB)\n", present, present*4/1024
    printf "  managed:  %8d 页 (%6.1f MB)\n", managed, managed*4/1024
    printf "  free:     %8d 页 (%6.1f MB)\n", free, free*4/1024
    printf "  min:      %8d 页 (%6.1f MB)\n", min, min*4/1024
    printf "  low:      %8d 页 (%6.1f MB)\n", low, low*4/1024
    printf "  high:     %8d 页 (%6.1f MB)\n", high, high*4/1024

    # 判断内存状态
    if (free < min) {
        printf "  状态: [!] 低于min水位，直接回收可能触发\n"
    } else if (free < low) {
        printf "  状态: [!] 低于low水位，kswapd正在运行\n"
    } else if (free < high) {
        printf "  状态: [*] 低于high水位，内存紧张\n"
    } else {
        printf "  状态: [OK] 内存充足\n"
    }
    printf "\n"
}
'
