#!/bin/bash
# 快速版本：监控伙伴系统变化（20秒）

LOG_FILE="buddy_log.txt"

echo "时间,Order0,Order1,Order2,Order3,Order4" > $LOG_FILE

echo "开始监控伙伴系统（20秒）..."
for i in {1..20}; do
    timestamp=$(date +%H:%M:%S)

    # 获取Normal zone的空闲页数（各order）
    counts=$(cat /proc/buddyinfo | grep Normal | awk '{for(j=4;j<=8;j++) printf "%d,",$j}')

    echo "$timestamp,$counts" >> $LOG_FILE
    sleep 1
done

echo "监控完成，数据已保存到 $LOG_FILE"

# 绘图分析
python3 << 'EOF'
import matplotlib.pyplot as plt
import csv

times = []
orders = [[], [], [], [], []]

with open('buddy_log.txt', 'r') as f:
    reader = csv.reader(f)
    next(reader)  # skip header
    for row in reader:
        times.append(row[0])
        for i in range(5):
            orders[i].append(int(row[i+1]))

plt.figure(figsize=(12, 6))
for i, order in enumerate(orders):
    plt.plot(times, order, marker='o', label=f'Order {i}')

plt.xlabel('Time')
plt.ylabel('Free Blocks')
plt.title('Buddy System Free Blocks Over Time')
plt.legend()
plt.xticks(rotation=45)
plt.tight_layout()
plt.savefig('buddy_analysis.png')
print("图表已保存到 buddy_analysis.png")
EOF
