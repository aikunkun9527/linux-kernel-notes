// 文件: exp5_slab_module/slab_stat.c
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define MY_CACHE_NAME "my_test_cache"
#define MY_OBJECT_SIZE 256

static struct kmem_cache *my_cache;
static void *objects[10];
static int obj_count = 0;

// 对象构造函数
static void my_ctor(void *obj)
{
    memset(obj, 0, MY_OBJECT_SIZE);
}

// /proc 文件显示slab统计
static int slab_stat_show(struct seq_file *m, void *v)
{
    struct kmem_cache *cache;
    int i;

    seq_printf(m, "========== 自定义Slab缓存信息 ==========\n");
    seq_printf(m, "缓存名称: %s\n", MY_CACHE_NAME);
    seq_printf(m, "对象大小: %d 字节\n", MY_OBJECT_SIZE);
    seq_printf(m, "已分配对象数: %d\n", obj_count);

    if (my_cache) {
        seq_printf(m, "缓存指针: %p\n", my_cache);
    }

    seq_printf(m, "\n========== 已分配的对象地址 ==========\n");
    for (i = 0; i < obj_count; i++) {
        if (objects[i])
            seq_printf(m, "对象 %d: %p\n", i, objects[i]);
    }

    return 0;
}

static int slab_stat_open(struct inode *inode, struct file *file)
{
    return single_open(file, slab_stat_show, NULL);
}

static const struct proc_ops slab_stat_proc_ops = {
    .proc_open = slab_stat_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static struct proc_dir_entry *proc_entry;

static int __init slab_stat_init(void)
{
    int i;

    pr_info("Slab统计模块加载\n");

    // 创建自定义缓存
    my_cache = kmem_cache_create(MY_CACHE_NAME,
                                 MY_OBJECT_SIZE, 0,
                                 SLAB_HWCACHE_ALIGN | SLAB_POISON,
                                 my_ctor);
    if (!my_cache) {
        pr_err("创建缓存失败\n");
        return -ENOMEM;
    }

    pr_info("创建缓存: %s, 对象大小: %d\n", MY_CACHE_NAME, MY_OBJECT_SIZE);
    pr_info("查看缓存: cat /proc/slabinfo | grep %s\n", MY_CACHE_NAME);

    // 分配多个对象
    for (i = 0; i < 5; i++) {
        objects[obj_count] = kmem_cache_alloc(my_cache, GFP_KERNEL);
        if (objects[obj_count]) {
            pr_info("分配对象 %d: %p\n", obj_count, objects[obj_count]);
            obj_count++;
        }
    }

    pr_info("分配了 %d 个对象\n", obj_count);

    // 释放部分对象（保留2个）
    for (i = 0; i < obj_count - 2; i++) {
        if (objects[i]) {
            kmem_cache_free(my_cache, objects[i]);
            objects[i] = NULL;
            pr_info("释放对象 %d\n", i);
        }
    }

    // 创建/proc接口
    proc_entry = proc_create("my_slab_stat", 0444, NULL, &slab_stat_proc_ops);
    if (!proc_entry) {
        pr_err("创建/proc条目失败\n");
    }

    pr_info("查看统计: cat /proc/my_slab_stat\n");
    pr_info("模块加载完成\n");

    return 0;
}

static void __exit slab_stat_exit(void)
{
    int i;

    pr_info("Slab统计模块卸载\n");

    // 释放剩余对象
    for (i = 0; i < 10; i++) {
        if (objects[i]) {
            kmem_cache_free(my_cache, objects[i]);
            pr_info("释放对象 %d\n", i);
        }
    }

    // 销毁缓存
    if (my_cache) {
        kmem_cache_destroy(my_cache);
        pr_info("销毁缓存: %s\n", MY_CACHE_NAME);
    }

    // 移除/proc接口
    if (proc_entry) {
        remove_proc_entry("my_slab_stat", NULL);
    }

    pr_info("模块卸载完成\n");
}

module_init(slab_stat_init);
module_exit(slab_stat_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kernel Lab");
MODULE_DESCRIPTION("Slab分配器演示模块");
