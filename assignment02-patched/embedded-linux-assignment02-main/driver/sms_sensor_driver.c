

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/mutex.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/errno.h>

#include "sms_sensor.h"

/* ============================================================
 * 1. DRIVER CONFIGURATION
 * ============================================================
 */

#define DRIVER_NAME       "sms_sensor_driver"
#define DEVICE_NAME       "sms_sensor"
#define DEVICE_CLASS_NAME "sms_sensor_class"

#define SMS_MIN_INTERVAL_MS 1U
#define SMS_MAX_INTERVAL_MS 60000U

#define SMS_LINE_SIZE 128

/*
 * Measurements are stored as integers in tenths.
 *
 * Example:
 *   253 represents 25.3 Celsius.
 *   678 represents 67.8 percent humidity.
 *
 * This avoids floating-point arithmetic in kernel space.
 */

/* ============================================================
 * 2. DRIVER STATE
 * ============================================================
 */

/* Character device resources. */
static dev_t sms_dev_number;
static struct cdev sms_cdev;

static struct class *sms_class;
static struct device *sms_device;

/* Procfs resource. */
static struct proc_dir_entry *sms_proc_entry;

/*
 * Protect shared sensor configuration and statistics.
 */
static DEFINE_MUTEX(sms_lock);

/* Minimum interval between successful sensor reads. */
static u32 sms_interval_ms = SMS_DEFAULT_INTERVAL_MS;

/* Sensor statistics. */
static u64 sms_read_count;
static u64 sms_start_time_s;
static u64 sms_last_read_ms;

/* Last measurements, stored in tenths. */
static u32 sms_last_temperature_tenths;
static u32 sms_last_humidity_tenths;

/* Req 1: tong so byte da tra ve cho user qua read() (64-bit). */
static u64 sms_total_bytes_read;

/*
 * Req 2: nhiet do min/max cua moi mau da sinh ra tu luc nap module,
 * luu dang so nguyen x100 (vd 2530 = 25.30). Khong dung so thuc trong kernel.
 * Dung co sms_have_temperature thay vi gia tri khoi tao "ma thuat":
 * mau dau tien luon gan thang vao min va max, nen luon dung.
 */
static u32 sms_min_temperature_x100;
static u32 sms_max_temperature_x100;
static bool sms_have_temperature;

/* ============================================================
 * 3. CHARACTER DEVICE OPERATIONS
 * ============================================================
 */

/*
 * sms_open - Open the sensor device.
 */
static int sms_open(struct inode *inode, struct file *file)
{
    (void)inode;
    (void)file;

    pr_debug("%s: device opened\n", DRIVER_NAME);

    return 0;
}

/*
 * sms_release - Close the sensor device.
 */
static int sms_release(struct inode *inode, struct file *file)
{
    (void)inode;
    (void)file;

    pr_debug("%s: device closed\n", DRIVER_NAME);

    return 0;
}

/*
 * sms_read - Generate and return one sensor sample.
 *
 * CSV format:
 *   timestamp_ms,temperature,humidity\n
 *
 * Example:
 *   123456789,25.3,67.8
 *
 * The minimum interval between successful reads is controlled
 * by sms_interval_ms.
 *
 * Return:
 *   > 0       Number of bytes returned
 *   0         No bytes requested
 *   -EINVAL   User buffer is too small
 *   -EFAULT   Failed to copy data to userspace
 *   -ERESTARTSYS if interrupted while acquiring the mutex
 */
static ssize_t sms_read(struct file *file,
                        char __user *user_buffer,
                        size_t count,
                        loff_t *position)
{
    char line[SMS_LINE_SIZE];

    u32 temperature_tenths;
    u32 humidity_tenths;

    u64 now_ms;
    u64 elapsed_ms;
    u64 remaining_ms;

    int line_length;
    unsigned long not_copied;

    (void)file;
    (void)position;

    if (count == 0)
        return 0;

retry_interval:

    /*
     * Acquire the mutex before examining the previous sample
     * timestamp and the configured minimum interval.
     */
    if (mutex_lock_interruptible(&sms_lock))
        return -ERESTARTSYS;

    now_ms = ktime_to_ms(ktime_get());

    /*
     * Enforce the minimum interval between successful reads.
     *
     * Release the mutex before sleeping so other operations,
     * including ioctl and procfs statistics, can proceed.
     */
    if (sms_last_read_ms != 0) {
        elapsed_ms = now_ms - sms_last_read_ms;

        if (elapsed_ms < sms_interval_ms) {
            remaining_ms = sms_interval_ms - elapsed_ms;

            mutex_unlock(&sms_lock);

            msleep((unsigned int)remaining_ms);

            /*
             * Recheck the interval after sleeping.
             * A loop avoids recursive calls and stack growth.
             */
            goto retry_interval;
        }
    }

    /*
     * Generate temperature:
     *
     * 150 ... 450 tenths
     * = 15.0 ... 45.0 Celsius
     */
    temperature_tenths =
        150U + (get_random_u32() % 301U);

    /*
     * Generate humidity:
     *
     * 300 ... 900 tenths
     * = 30.0 ... 90.0 percent
     */
    humidity_tenths =
        300U + (get_random_u32() % 601U);

    /*
     * Capture the timestamp for this sample.
     */
    now_ms = ktime_to_ms(ktime_get());

    /*
     * Format CSV without floating-point operations.
     *
     * Example:
     *   123456789,25.3,67.8\n
     */
    line_length = scnprintf(
        line,
        sizeof(line),
        "%llu,%u.%u,%u.%u\n",
        (unsigned long long)now_ms,
        temperature_tenths / 10U,
        temperature_tenths % 10U,
        humidity_tenths / 10U,
        humidity_tenths % 10U
    );

    /*
     * Return a complete record only.
     * Do not silently truncate a CSV sample.
     */
    if (count < (size_t)line_length) {
        mutex_unlock(&sms_lock);
        return -EINVAL;
    }

    /*
     * Transfer the CSV record to userspace.
     */
    not_copied = copy_to_user(
        user_buffer,
        line,
        (size_t)line_length
    );

    if (not_copied != 0) {
        mutex_unlock(&sms_lock);
        return -EFAULT;
    }

    /*
     * Update statistics only after a successful transfer.
     */
    sms_read_count++;

    sms_last_temperature_tenths = temperature_tenths;
    sms_last_humidity_tenths = humidity_tenths;

    /*
     * Req 1: cong SO BYTE THUC SU tra ve (line_length), khong phai count.
     * Dong nay nam SAU copy_to_user() thanh cong nen neu copy loi
     * (return -EFAULT o tren) thi khong cong gi.
     */
    sms_total_bytes_read += (u64)line_length;

    /* Req 2: cap nhat min/max (x100) duoi cung mutex voi cac thong ke khac. */
    {
        u32 temperature_x100 = temperature_tenths * 10U;

        if (!sms_have_temperature) {
            sms_min_temperature_x100 = temperature_x100;
            sms_max_temperature_x100 = temperature_x100;
            sms_have_temperature = true;
        } else {
            if (temperature_x100 < sms_min_temperature_x100)
                sms_min_temperature_x100 = temperature_x100;
            if (temperature_x100 > sms_max_temperature_x100)
                sms_max_temperature_x100 = temperature_x100;
        }
    }

    sms_last_read_ms = ktime_to_ms(ktime_get());

    mutex_unlock(&sms_lock);

    return line_length;
}

/* ============================================================
 * 4. IOCTL INTERFACE
 * ============================================================
 */

/*
 * sms_ioctl - Configure the minimum sensor read interval.
 *
 * Command:
 *   SMS_SET_INTERVAL
 *
 * Argument:
 *   Pointer to a u32 value containing milliseconds.
 *
 * Valid range:
 *   1 ... 60000 ms
 */
static long sms_ioctl(struct file *file,
                      unsigned int command,
                      unsigned long argument)
{
    u32 new_interval_ms;

    (void)file;

    switch (command) {
    case SMS_SET_INTERVAL:

        /*
         * Copy the interval from userspace.
         */
        if (copy_from_user(
                &new_interval_ms,
                (void __user *)argument,
                sizeof(new_interval_ms))) {
            return -EFAULT;
        }

        /*
         * Validate the requested interval.
         */
        if (new_interval_ms < SMS_MIN_INTERVAL_MS ||
            new_interval_ms > SMS_MAX_INTERVAL_MS) {
            return -EINVAL;
        }

        /*
         * Update the shared configuration safely.
         */
        mutex_lock(&sms_lock);

        sms_interval_ms = new_interval_ms;

        mutex_unlock(&sms_lock);

        pr_info(
            "%s: interval configured to %u ms\n",
            DRIVER_NAME,
            new_interval_ms
        );

        return 0;

    default:
        return -ENOTTY;
    }
}

/*
 * File operations exposed by the character device.
 */
static const struct file_operations sms_fops = {
    .owner          = THIS_MODULE,
    .open           = sms_open,
    .release        = sms_release,
    .read           = sms_read,
    .unlocked_ioctl = sms_ioctl,
    .llseek         = no_llseek,
};

/* ============================================================
 * 5. PROCFS STATISTICS
 * ============================================================
 */

/*
 * sms_stats_show - Display sensor driver statistics.
 *
 * Interface:
 *   /proc/sms_stats
 */
static int sms_stats_show(struct seq_file *seq, void *unused)
{
    u64 read_count;
    u64 uptime_s;
    u64 last_read_ms;

    u64 total_bytes_read;
    u32 min_temperature_x100;
    u32 max_temperature_x100;
    bool have_temperature;

    u32 temperature_tenths;
    u32 humidity_tenths;
    u32 interval_ms;

    (void)unused;

    /*
     * Copy shared state while holding the mutex.
     */
    mutex_lock(&sms_lock);

    read_count = sms_read_count;
    last_read_ms = sms_last_read_ms;

    temperature_tenths = sms_last_temperature_tenths;
    humidity_tenths = sms_last_humidity_tenths;

    interval_ms = sms_interval_ms;

    total_bytes_read = sms_total_bytes_read;
    min_temperature_x100 = sms_min_temperature_x100;
    max_temperature_x100 = sms_max_temperature_x100;
    have_temperature = sms_have_temperature;

    uptime_s = ktime_get_seconds() - sms_start_time_s;

    mutex_unlock(&sms_lock);

    /*
     * Format output after releasing the mutex.
     */
    seq_printf(
        seq,
        "read_count: %llu\n",
        (unsigned long long)read_count
    );

    seq_printf(
        seq,
        "last_temperature: %u.%u\n",
        temperature_tenths / 10U,
        temperature_tenths % 10U
    );

    seq_printf(
        seq,
        "last_humidity: %u.%u\n",
        humidity_tenths / 10U,
        humidity_tenths % 10U
    );

    seq_printf(
        seq,
        "uptime_seconds: %llu\n",
        (unsigned long long)uptime_s
    );

    seq_printf(
        seq,
        "interval_ms: %u\n",
        interval_ms
    );

    seq_printf(
        seq,
        "last_read_ms: %llu\n",
        (unsigned long long)last_read_ms
    );

    /* Req 1 */
    seq_printf(
        seq,
        "total_bytes_read: %llu\n",
        (unsigned long long)total_bytes_read
    );

    /* Req 2: N/A truoc lan doc dau tien, sau do in dang xx.xx (x100). */
    if (!have_temperature) {
        seq_puts(seq, "min_temperature: N/A\n");
        seq_puts(seq, "max_temperature: N/A\n");
    } else {
        seq_printf(seq, "min_temperature: %u.%02u\n",
                   min_temperature_x100 / 100U,
                   min_temperature_x100 % 100U);
        seq_printf(seq, "max_temperature: %u.%02u\n",
                   max_temperature_x100 / 100U,
                   max_temperature_x100 % 100U);
    }

    return 0;
}

/*
 * sms_stats_open - Open the procfs statistics interface.
 */
static int sms_stats_open(struct inode *inode, struct file *file)
{
    (void)inode;

    return single_open(file, sms_stats_show, NULL);
}

/*
 * Linux 5.15 procfs operations.
 */
static const struct proc_ops sms_proc_ops = {
    .proc_open    = sms_stats_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* ============================================================
 * 6. MODULE INITIALIZATION
 * ============================================================
 */

/*
 * sms_sensor_init - Initialize the sensor driver.
 */
static int __init sms_sensor_init(void)
{
    int ret;

    /*
     * Initialize the driver state.
     */
    sms_interval_ms = SMS_DEFAULT_INTERVAL_MS;

    sms_read_count = 0;
    sms_last_read_ms = 0;

    sms_last_temperature_tenths = 0;
    sms_last_humidity_tenths = 0;

    sms_total_bytes_read = 0;
    sms_min_temperature_x100 = 0;
    sms_max_temperature_x100 = 0;
    sms_have_temperature = false;

    sms_start_time_s = ktime_get_seconds();

    /*
     * Allocate a dynamic character device number.
     */
    ret = alloc_chrdev_region(
        &sms_dev_number,
        0,
        1,
        DRIVER_NAME
    );

    if (ret < 0) {
        pr_err(
            "%s: failed to allocate device number\n",
            DRIVER_NAME
        );

        return ret;
    }

    /*
     * Initialize and register the character device.
     */
    cdev_init(&sms_cdev, &sms_fops);

    sms_cdev.owner = THIS_MODULE;

    ret = cdev_add(&sms_cdev, sms_dev_number, 1);

    if (ret < 0) {
        pr_err("%s: cdev_add failed\n", DRIVER_NAME);

        goto err_unregister_region;
    }

    /*
     * Create the device class.
     *
     * Linux 5.15 uses:
     * class_create(THIS_MODULE, class_name)
     */
    sms_class = class_create(
        THIS_MODULE,
        DEVICE_CLASS_NAME
    );

    if (IS_ERR(sms_class)) {
        ret = PTR_ERR(sms_class);
        sms_class = NULL;

        pr_err("%s: class_create failed\n", DRIVER_NAME);

        goto err_cdev_del;
    }

    /*
     * Create /dev/sms_sensor.
     */
    sms_device = device_create(
        sms_class,
        NULL,
        sms_dev_number,
        NULL,
        "%s",
        DEVICE_NAME
    );

    if (IS_ERR(sms_device)) {
        ret = PTR_ERR(sms_device);
        sms_device = NULL;

        pr_err("%s: device_create failed\n", DRIVER_NAME);

        goto err_class_destroy;
    }

    /*
     * Create /proc/sms_stats.
     */
    sms_proc_entry = proc_create(
        SMS_PROC_NAME,
        0444,
        NULL,
        &sms_proc_ops
    );

    if (sms_proc_entry == NULL) {
        ret = -ENOMEM;

        pr_err("%s: proc_create failed\n", DRIVER_NAME);

        goto err_device_destroy;
    }

    pr_info(
        "%s: loaded successfully\n",
        DRIVER_NAME
    );

    pr_info(
        "%s: device=%s, major=%d, minor=%d\n",
        DRIVER_NAME,
        DEVICE_NAME,
        MAJOR(sms_dev_number),
        MINOR(sms_dev_number)
    );

    pr_info(
        "%s: default interval=%u ms\n",
        DRIVER_NAME,
        sms_interval_ms
    );

    pr_info(
        "%s: proc interface=/%s\n",
        DRIVER_NAME,
        SMS_PROC_NAME
    );

    return 0;

/*
 * Error handling:
 * Release resources in reverse initialization order.
 */

err_device_destroy:
    device_destroy(sms_class, sms_dev_number);

err_class_destroy:
    class_destroy(sms_class);

err_cdev_del:
    cdev_del(&sms_cdev);

err_unregister_region:
    unregister_chrdev_region(sms_dev_number, 1);

    return ret;
}

/* ============================================================
 * 7. MODULE CLEANUP
 * ============================================================
 */

/*
 * sms_sensor_exit - Remove the driver and release resources.
 */
static void __exit sms_sensor_exit(void)
{
    /*
     * Remove procfs and device interfaces first.
     */
    proc_remove(sms_proc_entry);

    device_destroy(sms_class, sms_dev_number);

    class_destroy(sms_class);

    /*
     * Unregister the character device.
     */
    cdev_del(&sms_cdev);

    unregister_chrdev_region(sms_dev_number, 1);

    pr_info(
        "%s: unloaded, total successful reads=%llu\n",
        DRIVER_NAME,
        (unsigned long long)sms_read_count
    );
}

/* ============================================================
 * 8. MODULE REGISTRATION
 * ============================================================
 */

module_init(sms_sensor_init);
module_exit(sms_sensor_exit);

MODULE_LICENSE("GPL");

MODULE_AUTHOR("Hoang Trung Hai - SS204779");

MODULE_DESCRIPTION(
    "Simulated temperature and humidity sensor driver "
    "for Embedded Linux Assignment 02"
);

MODULE_VERSION("1.0");
