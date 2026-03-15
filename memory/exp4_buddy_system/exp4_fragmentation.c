// 文件: exp4_fragmentation.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#define NUM_BLOCKS 1000
#define BLOCK_SIZE (64 * 1024)  // 64KB

void print_buddyinfo() {
    printf("\n当前伙伴系统状态 (Normal zone):\n");
    FILE *fp = fopen("/proc/buddyinfo", "r");
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "Normal")) {
            printf("%s", line);
        }
    }
    fclose(fp);
}

void print_meminfo() {
    printf("\n内存信息:\n");
    FILE *fp = fopen("/proc/meminfo", "r");
    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), fp) && count < 10) {
        printf("%s", line);
        count++;
    }
    fclose(fp);
}

int main() {
    void *blocks[NUM_BLOCKS] = {NULL};

    print_meminfo();
    print_buddyinfo();

    printf("\n分配 %d 个 %dKB 内存块...\n", NUM_BLOCKS, BLOCK_SIZE/1024);

    // 分配
    int allocated = 0;
    for (int i = 0; i < NUM_BLOCKS; i++) {
        blocks[i] = malloc(BLOCK_SIZE);
        if (!blocks[i]) {
            printf("分配失败在第 %d 块\n", i);
            break;
        }
        // 触发页面分配
        memset(blocks[i], 'x', BLOCK_SIZE);
        allocated++;
    }

    printf("成功分配 %d 块，共 %d MB\n", allocated, allocated * BLOCK_SIZE / (1024*1024));

    print_meminfo();
    print_buddyinfo();

    printf("\n间隔释放，制造碎片...（释放偶数索引的块）\n");
    int freed = 0;
    for (int i = 0; i < allocated; i += 2) {
        free(blocks[i]);
        blocks[i] = NULL;
        freed++;
    }

    printf("释放了 %d 块\n", freed);

    print_meminfo();
    print_buddyinfo();

    // 尝试分配大块连续内存
    printf("\n尝试分配 1MB 连续内存...\n");
    void *large = malloc(1024 * 1024);
    if (large) {
        memset(large, 'y', 1024 * 1024);  // 触发实际分配
        printf("成功分配 1MB\n");
        print_buddyinfo();
        free(large);
    } else {
        printf("分配失败（碎片影响）\n");
    }

    printf("\n尝试分配 10MB 连续内存...\n");
    void *huge = malloc(10 * 1024 * 1024);
    if (huge) {
        memset(huge, 'z', 10 * 1024 * 1024);
        printf("成功分配 10MB\n");
        print_buddyinfo();
        free(huge);
    } else {
        printf("分配失败（碎片影响）\n");
    }

    // 清理
    printf("\n清理剩余内存...\n");
    for (int i = 0; i < NUM_BLOCKS; i++) {
        if (blocks[i]) free(blocks[i]);
    }

    print_meminfo();
    print_buddyinfo();

    printf("\n实验完成\n");
    return 0;
}
