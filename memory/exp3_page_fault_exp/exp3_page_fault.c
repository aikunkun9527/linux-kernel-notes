// 文件: exp3_page_fault.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>

// 获取缺页次数
void get_page_faults(long *minor, long *major) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    *minor = usage.ru_minflt;
    *major = usage.ru_majflt;
}

int main() {
    long minor_before, major_before, minor_after, major_after;

    get_page_faults(&minor_before, &major_before);

    // 触发缺页的几种方式

    // 方式1: 访问malloc的内存（首次访问）
    printf("方式1: 访问malloc内存\n");
    char *heap = malloc(10 * 1024 * 1024);  // 10MB
    for (int i = 0; i < 10 * 1024 * 1024; i += 4096) {
        heap[i] = 'x';  // 触发缺页
    }

    get_page_faults(&minor_after, &major_after);
    printf("  Minor faults: %ld\n", minor_after - minor_before);
    printf("  Major faults: %ld\n", major_after - major_before);

    minor_before = minor_after;
    major_before = major_after;

    // 方式2: mmap匿名映射
    printf("\n方式2: mmap匿名映射\n");
    char *mapped = mmap(NULL, 10 * 1024 * 1024,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    for (int i = 0; i < 10 * 1024 * 1024; i += 4096) {
        mapped[i] = 'x';
    }

    get_page_faults(&minor_after, &major_after);
    printf("  Minor faults: %ld\n", minor_after - minor_before);
    printf("  Major faults: %ld\n", major_after - major_before);

    minor_before = minor_after;
    major_before = major_after;

    // 方式3: 文件映射
    printf("\n方式3: 文件映射\n");
    FILE *fp = fopen("/bin/ls", "r");
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *file_map = mmap(NULL, fsize, PROT_READ, MAP_PRIVATE, fileno(fp), 0);
    volatile char sum = 0;
    for (long i = 0; i < fsize; i += 4096) {
        sum += file_map[i];
    }

    get_page_faults(&minor_after, &major_after);
    printf("  Minor faults: %ld\n", minor_after - minor_before);
    printf("  Major faults: %ld\n", major_after - major_before);
    printf("  (checksum: %d, 防止编译器优化)\n", sum);

    // 清理
    free(heap);
    munmap(mapped, 10 * 1024 * 1024);
    munmap(file_map, fsize);
    fclose(fp);

    return 0;
}
