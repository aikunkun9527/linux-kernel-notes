#!/bin/bash
# 文件: exp3_page_fault_monitor.sh
# 使用/proc监控缺页中断

echo "========== 系统缺页统计 =========="
echo "从 /proc/vmstat:"
grep -E "^pgfault|^pgmajfault" /proc/vmstat

echo -e "\n========== 进程缺页统计 =========="
echo "从 /proc/self/status:"
grep -E "VmRSS|VmSize|VmData|VmStk" /proc/self/status

echo -e "\n========== 运行测试程序并观察缺页 =========="
# 记录运行前
pgfault_before=$(grep pgfault /proc/vmstat | awk '{print $2}')
pgmajfault_before=$(grep pgmajfault /proc/vmstat | awk '{print $2}')

# 运行测试程序
./exp3_page_fault

# 记录运行后
pgfault_after=$(grep pgfault /proc/vmstat | awk '{print $2}')
pgmajfault_after=$(grep pgmajfault /proc/vmstat | awk '{print $2}')

echo -e "\n========== 缺页变化 =========="
echo "Minor page faults (pgfault): $((pgfault_after - pgfault_before))"
echo "Major page faults (pgmajfault): $((pgmajfault_after - pgmajfault_before))"

echo -e "\n========== 缺页类型分析 =========="
echo "从 /proc/vmstat 详细信息:"
grep -E "^pgfault|^pgmajfault|^pgrefill|^pgsteal" /proc/vmstat
