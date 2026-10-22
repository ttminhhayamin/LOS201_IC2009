#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tran Tuan Minh - SE203134");
MODULE_DESCRIPTION("Assignment 01 - Misc Counter Device Driver");
MODULE_VERSION("1.0");

/* Counter and statistics */
static atomic_t counter_value = ATOMIC_INIT(0);
static atomic_t total_reads = ATOMIC_INIT(0);
static atomic_t total_writes = ATOMIC_INIT(0);

static DEFINE_MUTEX(counter_mutex);

static struct proc_dir_entry *counter_proc;

/*
 * Store a read snapshot for each opened file.
 * This supports partial reads without incrementing twice.
 */
struct counter_reader {
    char text[32];
    size_t length;
    bool initialized;
};

/*
 * OPEN: Allocate per-file reading state.
 * Return 0 or -ENOMEM.
 */
static int counter_open(struct inode *inode,
                        struct file *file)
{
    struct counter_reader *state;

    state = kzalloc(sizeof(*state), GFP_KERNEL);

    if (!state)
        return -ENOMEM;

    file->private_data = state;

    return 0;
}

/*
 * RELEASE: Free the reading state.
 * Return 0.
 */
static int counter_release(struct inode *inode,
                           struct file *file)
{
    kfree(file->private_data);
    file->private_data = NULL;

    return 0;
}

/*
 * READ: Return current value followed by newline.
 * Increment the counter once per successful read.
 *
 * Return bytes copied, 0 at EOF, or negative error.
 */
static ssize_t counter_read(struct file *file,
                            char __user *buf,
                            size_t len,
                            loff_t *ppos)
{
    struct counter_reader *state = file->private_data;
    ssize_t ret;
    int value;

    if (len == 0)
        return 0;

    mutex_lock(&counter_mutex);

    if (!state->initialized) {
        value = atomic_read(&counter_value);

        state->length = scnprintf(
            state->text,
            sizeof(state->text),
            "%d\n",
            value
        );
    }

    ret = simple_read_from_buffer(
        buf,
        len,
        ppos,
        state->text,
        state->length
    );

    if (ret > 0 && !state->initialized) {
        state->initialized = true;

        atomic_inc(&counter_value);
        atomic_inc(&total_reads);

        pr_info("counter_driver: read value %d\n",
                value);
    }

    mutex_unlock(&counter_mutex);

    return ret;
}

/*
 * WRITE: Receive a non-negative ASCII integer.
 * Set counter to the supplied value.
 *
 * Return bytes written or -EINVAL/-EFAULT.
 */
static ssize_t counter_write(struct file *file,
                             const char __user *buf,
                             size_t len,
                             loff_t *ppos)
{
    char temp[32];
    int value;

    if (len == 0 || len >= sizeof(temp))
        return -EINVAL;

    if (copy_from_user(temp, buf, len))
        return -EFAULT;

    temp[len] = '\0';

    if (kstrtoint(temp, 10, &value))
        return -EINVAL;

    if (value < 0)
        return -EINVAL;

    mutex_lock(&counter_mutex);

    atomic_set(&counter_value, value);
    atomic_inc(&total_writes);

    mutex_unlock(&counter_mutex);

    pr_info("counter_driver: counter set to %d\n",
            value);

    return len;
}

/*
 * PROCFS: Display current counter statistics.
 *
 * Return 0.
 */
static int counter_proc_show(struct seq_file *m,
                             void *v)
{
    mutex_lock(&counter_mutex);

    seq_printf(m,
        "=== ASSIGNMENT 01 COUNTER INFO ===\n");

    seq_printf(m,
        "Current counter : %d\n",
        atomic_read(&counter_value));

    seq_printf(m,
        "Total reads     : %d\n",
        atomic_read(&total_reads));

    seq_printf(m,
        "Total writes    : %d\n",
        atomic_read(&total_writes));

    mutex_unlock(&counter_mutex);

    return 0;
}

/* Open procfs statistics using seq_file. */
static int counter_proc_open(struct inode *inode,
                             struct file *file)
{
    return single_open(file, counter_proc_show, NULL);
}

static const struct proc_ops counter_proc_ops = {
    .proc_open = counter_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

/* Device operations */
static const struct file_operations counter_fops = {
    .owner = THIS_MODULE,
    .open = counter_open,
    .read = counter_read,
    .write = counter_write,
    .release = counter_release,
};

/* Misc device registration */
static struct miscdevice counter_miscdev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "counter",
    .fops = &counter_fops,
    .mode = 0666,
};

/*
 * INIT: Register misc device and procfs entry.
 * Return 0 or a negative error.
 */
static int __init counter_init(void)
{
    int ret;

    ret = misc_register(&counter_miscdev);

    if (ret) {
        pr_err("counter_driver: misc_register failed\n");
        return ret;
    }

    counter_proc = proc_create(
        "counter_info",
        0444,
        NULL,
        &counter_proc_ops
    );

    if (!counter_proc) {
        misc_deregister(&counter_miscdev);
        return -ENOMEM;
    }

    pr_info("counter_driver: loaded successfully\n");

    return 0;
}

/*
 * EXIT: Remove procfs and unregister misc device.
 */
static void __exit counter_exit(void)
{
    proc_remove(counter_proc);

    misc_deregister(&counter_miscdev);

    pr_info("counter_driver: unloaded successfully\n");
}

module_init(counter_init);
module_exit(counter_exit);
