// 文件: exp2_process_maps.c
// 分析进程内存映射
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

// 打印当前进程的内存映射
void print_memory_maps() {
    char buf[256];
    FILE *fp = fopen("/proc/self/maps", "r");

    printf("进程 %d 的内存映射:\n", getpid());
    printf("================================================================\n");
    printf("%-18s %-5s %-5s %-10s %-10s %s\n",
           "地址范围", "权限", "共享", "偏移", "设备", "路径");
    printf("----------------------------------------------------------------\n");

    while (fgets(buf, sizeof(buf), fp)) {
        printf("%s", buf);
    }
    fclose(fp);
}

// 打印内存统计
void print_memory_stats() {
    FILE *fp = fopen("/proc/self/status", "r");
    char line[256];

    printf("\n进程内存统计:\n");
    printf("================================\n");
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "VmSize") || strstr(line, "VmRSS") ||
            strstr(line, "VmData") || strstr(line, "VmStk") ||
            strstr(line, "VmExe") || strstr(line, "VmLib") ||
            strstr(line, "VmPTE")) {
            printf("%s", line);
        }
    }
    fclose(fp);
}

int main() {
    printf("========== 初始状态 ==========\n");
    print_memory_stats();
    print_memory_maps();

    // 分配堆内存
    printf("\n========== malloc 1MB 后 ==========\n");
    void *heap = malloc(1024 * 1024);  // 1MB
    print_memory_stats();
    printf("堆地址: %p\n", heap);

    // 创建匿名映射
    printf("\n========== mmap 匿名映射 2MB 后 ==========\n");
    void *anon = mmap(NULL, 2 * 1024 * 1024,  // 2MB
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1, 0);
    if (anon != MAP_FAILED) {
        printf("匿名映射地址: %p\n", anon);
    }
    print_memory_stats();
    print_memory_maps();

    // 创建共享映射
    printf("\n========== mmap 共享映射 1MB 后 ==========\n");
    void *shared = mmap(NULL, 1024 * 1024,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS,
                        -1, 0);
    if (shared != MAP_FAILED) {
        printf("共享映射地址: %p\n", shared);
    }
    print_memory_stats();

    // 清理
    free(heap);
    if (anon != MAP_FAILED) munmap(anon, 2 * 1024 * 1024);
    if (shared != MAP_FAILED) munmap(shared, 1024 * 1024);

    printf("\n========== 清理后 ==========\n");
    print_memory_stats();

    return 0;
}
