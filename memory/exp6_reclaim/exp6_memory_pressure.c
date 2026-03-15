// 文件: exp6_memory_pressure.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

void get_vmstat(const char *name, unsigned long *value) {
    FILE *fp = fopen("/proc/vmstat", "r");
    char line[256];

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, name, strlen(name)) == 0) {
            sscanf(line, "%*s %lu", value);
            break;
        }
    }
    fclose(fp);
}

void print_memory_info() {
    FILE *fp = fopen("/proc/meminfo", "r");
    char line[256];
    int count = 0;

    printf("\n========== 内存信息 ==========\n");
    while (fgets(line, sizeof(line), fp) && count < 15) {
        if (strstr(line, "MemTotal") || strstr(line, "MemFree") ||
            strstr(line, "MemAvailable") || strstr(line, "Buffers") ||
            strstr(line, "Cached") || strstr(line, "SwapCached") ||
            strstr(line, "SwapTotal") || strstr(line, "SwapFree") ||
            strstr(line, "Active") || strstr(line, "Inactive") ||
            strstr(line, "Slab") || strstr(line, "Dirty") ||
            strstr(line, "Writeback")) {
            printf("%s", line);
            count++;
        }
    }
    fclose(fp);
}

void print_zone_watermarks() {
    printf("\n========== Zone水位线 ==========\n");
    FILE *fp = fopen("/proc/zoneinfo", "r");
    char line[256];
    int in_normal = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "zone   Normal")) {
            in_normal = 1;
            printf("%s", line);
        } else if (in_normal) {
            if (strstr(line, "zone")) {
                break;  // 下一个zone，退出
            }
            if (strstr(line, "min ") || strstr(line, "low ") ||
                strstr(line, "high ") || strstr(line, "present") ||
                strstr(line, "managed") || strstr(line, "free ")) {
                printf("%s", line);
            }
        }
    }
    fclose(fp);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("用法: %s <内存MB>\n", argv[0]);
        printf("示例: %s 1024  # 分配1GB内存\n", argv[0]);
        return 1;
    }

    long mb = atol(argv[1]);
    long size = mb * 1024 * 1024;

    unsigned long pgscan_before, pgscan_after;
    unsigned long pgsteal_before, pgsteal_after;
    unsigned long pswpin_before, pswpin_after;
    unsigned long pswpout_before, pswpout_after;

    print_memory_info();
    print_zone_watermarks();

    get_vmstat("pgscan_kswapd", &pgscan_before);
    get_vmstat("pgsteal_kswapd", &pgsteal_before);
    get_vmstat("pswpin", &pswpin_before);
    get_vmstat("pswpout", &pswpout_before);

    printf("\n尝试分配 %ld MB 内存...\n", mb);

    char *mem = malloc(size);
    if (!mem) {
        perror("malloc失败");
        return 1;
    }

    printf("分配成功，正在写入...\n");
    // 逐页写入，触发实际物理页分配
    for (long i = 0; i < size; i += 4096) {
        mem[i] = 'x';

        // 每100MB显示一次进度
        if (i % (100 * 1024 * 1024) == 0) {
            printf("已写入 %ld MB...\n", i / (1024 * 1024));
        }
    }

    printf("写入完成\n");

    get_vmstat("pgscan_kswapd", &pgscan_after);
    get_vmstat("pgsteal_kswapd", &pgsteal_after);
    get_vmstat("pswpin", &pswpin_after);
    get_vmstat("pswpout", &pswpout_after);

    print_memory_info();
    print_zone_watermarks();

    printf("\n========== 内存回收统计 ==========\n");
    printf("kswapd扫描页面: %lu\n", pgscan_after - pgscan_before);
    printf("kswapd回收页面: %lu\n", pgsteal_after - pgsteal_before);
    printf("页面换入(swap in): %lu\n", pswpin_after - pswpin_before);
    printf("页面换出(swap out): %lu\n", pswpout_after - pswpout_before);

    printf("\n按Enter释放内存...");
    getchar();

    free(mem);
    printf("内存已释放\n");

    print_memory_info();

    return 0;
}
