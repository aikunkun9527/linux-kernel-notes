#!/bin/bash
# 文件: exp6_reclaim_monitor.sh
# 监控内存回收活动

LOG_FILE="reclaim_log.txt"

echo "时间,pgfault,pgmajfault,pgscan_kswapd,pgsteal_kswapd,pswpin,pswpout" > $LOG_FILE

echo "开始监控内存回收（60秒）..."
echo "请运行内存压力测试: sudo ./exp6_memory_pressure 1024"
echo ""

for i in {1..60}; do
    timestamp=$(date +%H:%M:%S)

    pgfault=$(grep pgfault /proc/vmstat | awk '{print $2}')
    pgmajfault=$(grep pgmajfault /proc/vmstat | awk '{print $2}')
    pgscan=$(grep "pgscan_kswapd" /proc/vmstat | awk '{sum+=$2} END{print sum}')
    pgsteal=$(grep "pgsteal_kswapd" /proc/vmstat | awk '{sum+=$2} END{print sum}')
    pswpin=$(grep pswpin /proc/vmstat | awk '{print $2}')
    pswpout=$(grep pswpout /proc/vmstat | awk '{print $2}')

    echo "$timestamp,$pgfault,$pgmajfault,$pgscan,$pgsteal,$pswpin,$pswpout" >> $LOG_FILE

    # 实时显示关键指标
    printf "\r[%s] pgfault: %s, pgmajfault: %s, kswapd_scan: %s" "$timestamp" "$pgfault" "$pgmajfault" "$pgscan"

    sleep 1
done

echo -e "\n\n监控完成，数据已保存到 $LOG_FILE"
