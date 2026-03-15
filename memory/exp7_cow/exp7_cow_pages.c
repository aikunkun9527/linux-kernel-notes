// 文件: exp7_cow_pages.c
// 逐页分析COW行为
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>

#define PAGE_SIZE 4096
#define NUM_PAGES 100

// 获取页面在物理内存中的状态
void print_page_info(void *addr, size_t len) {
    FILE *fp = fopen("/proc/self/pagemap", "rb");
    if (!fp) {
        perror("无法打开pagemap");
        return;
    }

    unsigned long vaddr = (unsigned long)addr;
    unsigned long page_count = (len + PAGE_SIZE - 1) / PAGE_SIZE;

    printf("页面分析 (虚拟地址: 0x%lx, 页数: %lu):\n", vaddr, page_count);

    // 计算pagemap中的偏移
    unsigned long offset = vaddr / PAGE_SIZE * sizeof(unsigned long);

    if (fseek(fp, offset, SEEK_SET) != 0) {
        perror("fseek失败");
        fclose(fp);
        return;
    }

    int present_count = 0;
    int dirty_count = 0;

    for (unsigned long i = 0; i < page_count && i < 10; i++) {
        unsigned long entry;
        if (fread(&entry, sizeof(entry), 1, fp) != 1) {
            break;
        }

        int present = (entry >> 63) & 1;
        int dirty = (entry >> 55) & 1;
        unsigned long pfn = entry & ((1UL << 55) - 1);

        if (present) {
            present_count++;
            printf("  页 %lu: present=%d, dirty=%d, pfn=0x%lx\n",
                   i, present, dirty, pfn);
        }
    }

    fclose(fp);
    printf("  ... (共 %d 页在内存中)\n", present_count);
}

int main() {
    printf("========== COW逐页分析实验 ==========\n\n");

    // 使用mmap分配内存，更容易观察
    size_t size = NUM_PAGES * PAGE_SIZE;  // 100页 = 400KB
    char *data = mmap(NULL, size,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS,
                       -1, 0);

    if (data == MAP_FAILED) {
        perror("mmap失败");
        return 1;
    }

    printf("分配了 %zu 字节 (%d 页)\n\n", size, NUM_PAGES);

    // 初始化数据
    memset(data, 'X', size);
    printf("初始化完成\n");

    // 触发页面分配
    for (int i = 0; i < NUM_PAGES; i++) {
        data[i * PAGE_SIZE] = 'X';
    }

    printf("\n=== Fork前 ===\n");
    print_page_info(data, size);

    pid_t pid = fork();

    if (pid == 0) {
        // 子进程
        printf("\n=== 子进程: Fork后，修改前 ===\n");
        print_page_info(data, size);

        printf("\n修改前10页...\n");
        for (int i = 0; i < 10; i++) {
            data[i * PAGE_SIZE] = 'C';  // Child
        }

        printf("\n=== 子进程: 修改10页后 ===\n");
        print_page_info(data, size);

        // 修改更多页面
        printf("\n修改剩余所有页面...\n");
        for (int i = 10; i < NUM_PAGES; i++) {
            data[i * PAGE_SIZE] = 'C';
        }

        printf("\n=== 子进程: 修改所有页后 ===\n");
        print_page_info(data, size);

        munmap(data, size);
        exit(0);
    } else {
        // 父进程
        usleep(100000);  // 等待子进程打印

        printf("\n=== 父进程: 子进程修改期间 ===\n");
        print_page_info(data, size);

        int status;
        wait(&status);

        printf("\n=== 父进程: 子进程退出后 ===\n");
        print_page_info(data, size);

        // 验证数据
        int unchanged = 1;
        for (int i = 0; i < NUM_PAGES; i++) {
            if (data[i * PAGE_SIZE] != 'X') {
                unchanged = 0;
                break;
            }
        }
        printf("\n父进程数据验证: %s\n",
               unchanged ? "未被修改(COW保护)" : "被修改了(异常!)");

        munmap(data, size);
    }

    return 0;
}
