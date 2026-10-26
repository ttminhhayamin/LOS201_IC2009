/* SMS character device, ioctl, procfs and exam counters.
 * Author: Tran Tuan Minh - SE203134 | Date: 2026-10-26
 * Linux kernel 5.15, ARMv7 vexpress-a9. Uses integers x100, never float.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/math64.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/types.h>
#include "sms_sensor.h"

#define SMS_DEVICE_NAME "sms_sensor"
#define SMS_CLASS_NAME "sms_class"
#define SMS_PROC_NAME "sms_stats"
static dev_t sms_devt;
static struct cdev sms_cdev;
static struct class *sms_class;
static struct device *sms_device;
static struct proc_dir_entry *sms_proc;
static DEFINE_MUTEX(sms_lock);
static unsigned int interval_ms = 100;
static unsigned int read_count;
static u64 total_bytes_read;
static u64 last_read_ms;
static u64 loaded_ms;
static unsigned int last_temp_x100;
static unsigned int last_humid_x100;
static unsigned int min_temp_x100, max_temp_x100;
static bool have_temperature;

static ssize_t sms_read(struct file *filp, char __user *ubuf,
                        size_t count, loff_t *ppos)
{
    char line[80];
    size_t n;
    u64 now;
    unsigned int t, h;
    (void)filp;
    if (*ppos != 0)
        return 0;
    if (!count)
        return 0;

    if (mutex_lock_interruptible(&sms_lock))
        return -ERESTARTSYS;
    now = ktime_to_ms(ktime_get());
    if (last_read_ms && now - last_read_ms < interval_ms) {
        mutex_unlock(&sms_lock);
        return -EAGAIN;
    }
    t = 1500 + get_random_u32() % 3001; /* 15.00 to 45.00 */
    h = 3000 + get_random_u32() % 6001; /* 30.00 to 90.00 */
    n = scnprintf(line, sizeof(line), "%llu,%u.%02u,%u.%02u\n",
                  (unsigned long long)now, t / 100, t % 100,
                  h / 100, h % 100);
    if (count < n)
        n = count;
    /* Successful read: update exactly the number of bytes returned.
     * mutex is sleepable; copy_to_user may sleep, so do not use spinlock.
     */
    if (copy_to_user(ubuf, line, n)) {
        mutex_unlock(&sms_lock);
        return -EFAULT;
    }
    *ppos += n;
    last_read_ms = now;
    last_temp_x100 = t;
    last_humid_x100 = h;
    read_count++;
    total_bytes_read += n;                 /* Exam R1 */
    if (!have_temperature) {              /* Exam R2 */
        min_temp_x100 = max_temp_x100 = t;
        have_temperature = true;
    } else {
        if (t < min_temp_x100)
            min_temp_x100 = t;
        if (t > max_temp_x100)
            max_temp_x100 = t;
    }
    mutex_unlock(&sms_lock);
    return n;
}

static long sms_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    unsigned int ms;
    (void)filp;
    if (cmd != SMS_SET_INTERVAL)
        return -ENOTTY;
    if (copy_from_user(&ms, (void __user *)arg, sizeof(ms)))
        return -EFAULT;
    if (ms == 0 || ms > 60000)
        return -EINVAL;
    mutex_lock(&sms_lock);
    interval_ms = ms;
    mutex_unlock(&sms_lock);
    return 0;
}

static int sms_proc_show(struct seq_file *m, void *v)
{
    u64 uptime;
    (void)v;
    mutex_lock(&sms_lock);
    uptime = div_u64(ktime_to_ms(ktime_get()) - loaded_ms, 1000);
    seq_printf(m, "read_count: %u\n", read_count);
    seq_printf(m, "total_bytes_read: %llu\n",
               (unsigned long long)total_bytes_read);
    seq_printf(m, "last_temperature: %u.%02u\n",
               last_temp_x100 / 100, last_temp_x100 % 100);
    seq_printf(m, "last_humidity: %u.%02u\n",
               last_humid_x100 / 100, last_humid_x100 % 100);
    seq_printf(m, "uptime: %llu\n", (unsigned long long)uptime);
    seq_printf(m, "interval_ms: %u\n", interval_ms);
    if (!have_temperature) {
        seq_puts(m, "min_temperature: N/A\n");
        seq_puts(m, "max_temperature: N/A\n");
    } else {
        seq_printf(m, "min_temperature: %u.%02u\n",
                   min_temp_x100 / 100, min_temp_x100 % 100);
        seq_printf(m, "max_temperature: %u.%02u\n",
                   max_temp_x100 / 100, max_temp_x100 % 100);
    }
    mutex_unlock(&sms_lock);
    return 0;
}

static int sms_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, sms_proc_show, NULL);
}

static const struct proc_ops sms_proc_ops = {
    .proc_open = sms_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static const struct file_operations sms_fops = {
    .owner = THIS_MODULE,
    .read = sms_read,
    .unlocked_ioctl = sms_ioctl,
    .llseek = no_llseek,
};

static int __init sms_init(void)
{
    int rc;
    rc = alloc_chrdev_region(&sms_devt, 0, 1, SMS_DEVICE_NAME);
    if (rc)
        return rc;
    cdev_init(&sms_cdev, &sms_fops);
    sms_cdev.owner = THIS_MODULE;
    rc = cdev_add(&sms_cdev, sms_devt, 1);
    if (rc)
        goto fail_alloc;
    sms_class = class_create(THIS_MODULE, SMS_CLASS_NAME);
    if (IS_ERR(sms_class)) {
        rc = PTR_ERR(sms_class);
        goto fail_cdev;
    }
    sms_device = device_create(sms_class, NULL, sms_devt, NULL, SMS_DEVICE_NAME);
    if (IS_ERR(sms_device)) {
        rc = PTR_ERR(sms_device);
        goto fail_class;
    }
    sms_proc = proc_create(SMS_PROC_NAME, 0444, NULL, &sms_proc_ops);
    if (!sms_proc) {
        rc = -ENOMEM;
        goto fail_device;
    }
    loaded_ms = ktime_to_ms(ktime_get());
    pr_info("sms_sensor: loaded (major=%u minor=%u)\n",
            MAJOR(sms_devt), MINOR(sms_devt));
    return 0;
fail_device:
    device_destroy(sms_class, sms_devt);
fail_class:
    class_destroy(sms_class);
fail_cdev:
    cdev_del(&sms_cdev);
fail_alloc:
    unregister_chrdev_region(sms_devt, 1);
    return rc;
}

static void __exit sms_exit(void)
{
    proc_remove(sms_proc);
    device_destroy(sms_class, sms_devt);
    class_destroy(sms_class);
    cdev_del(&sms_cdev);
    unregister_chrdev_region(sms_devt, 1);
    pr_info("sms_sensor: unloaded\n");
}

module_init(sms_init);
module_exit(sms_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tran Tuan Minh - SE203134");
MODULE_DESCRIPTION("Embedded Linux SMS sensor driver with byte and min/max stats");
MODULE_VERSION("1.0");
