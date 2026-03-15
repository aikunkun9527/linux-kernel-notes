// 文件: exp7_cow.c
// 验证写时复制（Copy-on-Write）机制
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>

void print_memory_usage(const char *phase) {
    FILE *fp = fopen("/proc/self/status", "r");
    char line[256];

    printf("=== %s ===\n", phase);
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "VmRSS") || strstr(line, "VmSize") ||
            strstr(line, "VmData") || strstr(line, "VmStk") ||
            strstr(line, "VmExe") || strstr(line, "VmLib")) {
            printf("%s", line);
        }
    }
    fclose(fp);
}

void print_page_faults(const char *phase) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    printf("%s - Minor faults: %ld, Major faults: %ld\n",
           phase, usage.ru_minflt, usage.ru_majflt);
}

void print_shared_memory(const char *phase, size_t size) {
    // 读取smaps计算共享内存
    FILE *fp = fopen("/proc/self/smaps", "r");
    if (!fp) return;

    char line[256];
    unsigned long shared_clean = 0, shared_dirty = 0;
    unsigned long private_clean = 0, private_dirty = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Shared_Clean:", 13) == 0) {
            shared_clean += strtoul(line + 13, NULL, 10);
        } else if (strncmp(line, "Shared_Dirty:", 13) == 0) {
            shared_dirty += strtoul(line + 13, NULL, 10);
        } else if (strncmp(line, "Private_Clean:", 14) == 0) {
            private_clean += strtoul(line + 14, NULL, 10);
        } else if (strncmp(line, "Private_Dirty:", 14) == 0) {
            private_dirty += strtoul(line + 14, NULL, 10);
        }
    }
    fclose(fp);

    printf("%s - 共享内存: %lu KB, 私有内存: %lu KB\n",
           phase, shared_clean + shared_dirty, private_clean + private_dirty);
}

int main() {
    // 分配共享数据
    size_t size = 100 * 1024 * 1024;  // 100MB
    char *shared_data = malloc(size);

    if (!shared_data) {
        perror("malloc失败");
        return 1;
    }

    // 初始化数据
    printf("========== 写时复制（COW）验证实验 ==========\n\n");
    printf("分配 %zu MB 内存用于测试\n", size / (1024 * 1024));

    memset(shared_data, 'A', size);

    print_memory_usage("fork前 - 父进程");
    print_page_faults("fork前");
    print_shared_memory("fork前", size);

    printf("\n>>> 即将fork子进程...\n\n");

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork失败");
        free(shared_data);
        return 1;
    }

    if (pid == 0) {
        // 子进程
        printf("========== 子进程 ==========\n");

        // 等待一下让父进程先打印
        usleep(100000);

        print_memory_usage("子进程fork后（修改前）");
        print_page_faults("子进程fork后");
        print_shared_memory("子进程fork后", size);

        printf("\n子进程: 开始修改数据（触发COW）...\n");
        printf("每修改一页，内核会为子进程分配新的物理页\n\n");

        // 修改部分数据，触发COW
        size_t pages_modified = 0;
        for (size_t i = 0; i < size; i += 4096) {
            shared_data[i] = 'B';  // 每页一个字节
            pages_modified++;

            // 每10MB显示一次进度
            if (pages_modified % 2560 == 0) {
                printf("已修改 %zu MB...\n", pages_modified * 4 / 1024);
            }
        }

        printf("\n修改完成，共修改 %zu 页\n", pages_modified);

        print_memory_usage("子进程修改后（COW发生）");
        print_page_faults("子进程修改后");
        print_shared_memory("子进程修改后", size);

        // 验证数据
        printf("\n验证数据:\n");
        int diff_count = 0;
        for (size_t i = 0; i < size; i += 4096) {
            if (shared_data[i] != 'B') {
                diff_count++;
            }
        }
        printf("子进程数据检查: %s\n", diff_count == 0 ? "全部正确('B')" : "有错误");

        free(shared_data);
        printf("\n子进程退出\n");
        exit(0);
    } else {
        // 父进程
        printf("========== 父进程 ==========\n");
        printf("子进程PID: %d\n\n", pid);

        // 等待子进程完成
        usleep(200000);

        print_memory_usage("父进程（子进程修改期间）");
        print_shared_memory("父进程", size);

        // 等待子进程退出
        int status;
        waitpid(pid, &status, 0);

        printf("\n子进程已退出\n");

        print_memory_usage("父进程（子进程退出后）");
        print_page_faults("父进程最终");
        print_shared_memory("父进程最终", size);

        // 验证父进程数据未被修改
        printf("\n验证父进程数据完整性:\n");
        int diff_count = 0;
        for (size_t i = 0; i < size; i += 4096) {
            if (shared_data[i] != 'A') {
                diff_count++;
            }
        }
        printf("父进程数据检查: %s\n", diff_count == 0 ? "未被修改('A')" : "被修改了");

        free(shared_data);
        printf("\n实验完成\n");
    }

    return 0;
}
