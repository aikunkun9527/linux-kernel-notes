// 文件: exp5_slab_test.c
// 用户态Slab内存分配测试
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

#define NUM_ALLOCS 10000
#define OBJ_SIZE 256

void get_memory_usage(long *slab_reclaimable, long *slab_unreclaimable) {
    FILE *fp = fopen("/proc/vmstat", "r");
    char line[256];

    *slab_reclaimable = 0;
    *slab_unreclaimable = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "nr_slab_reclaimable", 19) == 0) {
            sscanf(line, "nr_slab_reclaimable %ld", slab_reclaimable);
        } else if (strncmp(line, "nr_slab_unreclaimable", 21) == 0) {
            sscanf(line, "nr_slab_unreclaimable %ld", slab_unreclaimable);
        }
    }
    fclose(fp);
}

void print_slab_memory() {
    FILE *fp = fopen("/proc/meminfo", "r");
    char line[256];

    printf("\n当前Slab内存使用:\n");
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "Slab:") || strstr(line, "SReclaimable:") || strstr(line, "SUnreclaim:")) {
            printf("  %s", line);
        }
    }
    fclose(fp);
}

int main() {
    void *ptrs[NUM_ALLOCS];
    long slab_r_before, slab_u_before, slab_r_after, slab_u_after;

    printf("========== Slab分配器用户态测试 ==========\n");
    printf("测试: 分配 %d 个 %d 字节对象\n", NUM_ALLOCS, OBJ_SIZE);

    print_slab_memory();
    get_memory_usage(&slab_r_before, &slab_u_before);
    printf("\n分配前 slab_reclaimable: %ld 页\n", slab_r_before);
    printf("分配前 slab_unreclaimable: %ld 页\n", slab_u_before);

    // 分配内存
    printf("\n开始分配...\n");
    for (int i = 0; i < NUM_ALLOCS; i++) {
        ptrs[i] = malloc(OBJ_SIZE);
        if (ptrs[i]) {
            memset(ptrs[i], 'x', OBJ_SIZE);  // 触发实际分配
        }
    }

    print_slab_memory();
    get_memory_usage(&slab_r_after, &slab_u_after);
    printf("\n分配后 slab_reclaimable: %ld 页 (变化: %ld)\n", slab_r_after, slab_r_after - slab_r_before);
    printf("分配后 slab_unreclaimable: %ld 页 (变化: %ld)\n", slab_u_after, slab_u_after - slab_u_before);

    // 检查kmalloc-256使用情况
    printf("\n检查 kmalloc-256 缓存:\n");
    system("cat /sys/kernel/slab/kmalloc-256/objects 2>/dev/null || echo '无法读取'");

    printf("\n按Enter释放内存...");
    getchar();

    // 释放内存
    printf("释放内存...\n");
    for (int i = 0; i < NUM_ALLOCS; i++) {
        if (ptrs[i]) free(ptrs[i]);
    }

    print_slab_memory();
    get_memory_usage(&slab_r_after, &slab_u_after);
    printf("\n释放后 slab_reclaimable: %ld 页\n", slab_r_after);
    printf("释放后 slab_unreclaimable: %ld 页\n", slab_u_after);

    printf("\n测试完成\n");
    return 0;
}
