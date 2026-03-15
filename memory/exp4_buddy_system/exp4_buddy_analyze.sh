#!/bin/bash
# 文件: exp4_buddy_analyze.sh
# 分析伙伴系统状态

echo "========== 伙伴系统详解 =========="
echo ""
echo "伙伴系统按2的幂次方组织内存块（Order）:"
echo "  Order 0: 1页  = 4KB"
echo "  Order 1: 2页  = 8KB"
echo "  Order 2: 4页  = 16KB"
echo "  Order 3: 8页  = 32KB"
echo "  Order 4: 16页 = 64KB"
echo "  Order 5: 32页 = 128KB"
echo "  ..."

echo -e "\n========== 当前伙伴系统状态 =========="
cat /proc/buddyinfo

echo -e "\n========== 详细分析 =========="
# 解析 buddyinfo
awk '
/^Node/ {
    node = $2
    zone = $4
    gsub(/,/,"",zone)
    printf "Node %s, Zone %s:\n", node, zone
    for (i=5; i<=NF; i++) {
        order = i-5
        pages = 2^order * 4  # KB
        if (pages < 1024)
            printf "  Order %2d: %5d 块 × %dKB = %dKB\n", order, $i, pages, $i * pages
        else
            printf "  Order %2d: %5d 块 × %dMB\n", order, $i, pages/1024
    }
    total = 0
    for (i=5; i<=NF; i++) {
        order = i-5
        pages_kb = 2^order * 4
        total += $i * pages_kb
    }
    printf "  总计: %.2f MB\n\n", total/1024
}
' /proc/buddyinfo

echo "========== 外部碎片分析 =========="
echo "外部碎片：物理内存总量足够，但无法分配连续的大块内存"
echo ""
echo "判断碎片化程度："
echo "  - 高Order块少 = 碎片化严重"
echo "  - 低Order块多，高Order块少 = 中等碎片化"
echo "  - 各Order块分布均匀 = 碎片化较轻"
echo ""

# 计算碎片化指数
echo "碎片化指数计算:"
awk '
/^Node/ && /Normal/ {
    low = $5 + $6 + $7  # Order 0-2
    mid = $8 + $9 + $10  # Order 3-5
    high = 0
    for (i=11; i<=NF; i++) high += $i
    total = low + mid + high
    if (total > 0) {
        frag_index = (low * 1.0 / total) * 100
        printf "  低阶块(0-2): %d (%.1f%%)\n", low, low*100.0/total
        printf "  中阶块(3-5): %d (%.1f%%)\n", mid, mid*100.0/total
        printf "  高阶块(6+):  %d (%.1f%%)\n", high, high*100.0/total
        printf "  碎片化指数: %.1f (越高表示碎片越严重)\n", frag_index
    }
}
' /proc/buddyinfo

echo -e "\n========== Zone信息 =========="
grep -E "^Node|^  (min|low|high|present|managed|free)" /proc/zoneinfo | \
awk '/^Node/{node=$2; zone=$4}
     /min/{printf "Node %s %s: min=%d ", node, zone, $2}
     /low/{printf "low=%d ", $2}
     /high/{printf "high=%d ", $2}
     /managed/{printf "managed=%d页 (%.1fMB)\n", $2, $2*4/1024}'
