// 文件: exp7_cow_simple.c
// 简单的COW演示
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    printf("========== COW简单演示 ==========\n\n");

    // 分配并初始化100MB内存
    size_t size = 100 * 1024 * 1024;
    char *data = malloc(size);
    memset(data, 'P', size);  // Parent

    printf("父进程分配了 %zu MB 内存，填充字符 'P'\n\n", size / (1024 * 1024));

    pid_t pid = fork();

    if (pid == 0) {
        // 子进程
        printf("[子进程] 刚fork，数据应该是 'P'\n");
        printf("[子进程] 第一个字符: '%c'\n", data[0]);
        printf("[子进程] 中间字符: '%c'\n", data[size/2]);
        printf("[子进程] 最后字符: '%c'\n", data[size-1]);

        printf("\n[子进程] 修改前10页...\n");
        for (int i = 0; i < 10; i++) {
            data[i * 4096] = 'C';  // Child
        }

        printf("[子进程] 修改后，检查数据:\n");
        printf("[子进程] 第0页: '%c' (已修改)\n", data[0]);
        printf("[子进程] 第5页: '%c' (已修改)\n", data[5 * 4096]);
        printf("[子进程] 第100页: '%c' (未修改)\n", data[100 * 4096]);

        free(data);
        exit(0);
    } else {
        // 父进程
        wait(NULL);  // 等待子进程

        printf("\n[父进程] 子进程退出后，检查数据:\n");
        printf("[父进程] 第0页: '%c' (应该仍是 'P')\n", data[0]);
        printf("[父进程] 第5页: '%c' (应该仍是 'P')\n", data[5 * 4096]);
        printf("[父进程] 第100页: '%c' (应该仍是 'P')\n", data[100 * 4096]);

        printf("\n结论: COW机制保护了父进程的内存不被子进程修改\n");
        free(data);
    }

    return 0;
}
