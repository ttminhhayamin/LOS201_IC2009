#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/timekeeping.h>
#include <linux/ioctl.h>
#include <linux/string.h>
#include <linux/err.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Lab");
MODULE_DESCRIPTION("LAB-02 Extended Practice Character Device Driver");
MODULE_VERSION("1.0");

#define DEVICE_NAME  "lab2"
#define CLASS_NAME   "lab2_class"
#define BUFFER_SIZE  1024
#define DRIVER_MAJOR 240
#define DRIVER_VERSION "1.0"

/* ioctl commands */
#define LAB2_IOC_MAGIC  'L'
#define GET_BUFFER_LEN  _IOR(LAB2_IOC_MAGIC, 1, int)
#define CLEAR_BUFFER    _IO(LAB2_IOC_MAGIC, 2)

/* Device data */
static char device_buffer[BUFFER_SIZE];
static size_t buffer_len;

/* Statistics */
static int open_count;
static int read_count;
static int write_count;

static unsigned long total_bytes_read;
static unsigned long total_bytes_written;

static size_t last_write_size;
static time64_t last_write_time;

/*
 * Question 4:
 * Default maximum write size = 128 bytes.
 *
 * Question 16:
 * User may change this through sysfs.
 */
static unsigned int max_size = 128;

/* Question 19: one-shot read state */
static bool data_available;

static DEFINE_MUTEX(lab2_mutex);

static struct class *lab2_class;
static struct device *lab2_device;
static struct proc_dir_entry *lab2_proc;


/* =========================================================
 * OPEN
 * ========================================================= */
static int lab2_open(struct inode *inode, struct file *file)
{
    mutex_lock(&lab2_mutex);

    /* Count successful opens safely */
    open_count++;

    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: device opened (count=%d)\n",
            open_count);

    return 0;
}


/* =========================================================
 * RELEASE
 * ========================================================= */
static int lab2_release(struct inode *inode, struct file *file)
{
    pr_info("lab2_driver: device closed\n");
    return 0;
}


/* =========================================================
 * READ
 * ========================================================= */
static ssize_t lab2_read(struct file *file,
                         char __user *buf,
                         size_t len,
                         loff_t *offset)
{
    size_t bytes_to_read;

    mutex_lock(&lab2_mutex);

    /* Question 1 */
    read_count++;

    /* Question 19: no new data -> EOF */
    if (!data_available) {
        mutex_unlock(&lab2_mutex);
        return 0;
    }

    if (*offset >= buffer_len) {
        data_available = false;
        mutex_unlock(&lab2_mutex);
        return 0;
    }

    bytes_to_read = buffer_len - *offset;

    if (len < bytes_to_read)
        bytes_to_read = len;

    /*
     * copy_to_user() must be used because destination is
     * userspace memory. Direct memcpy() is unsafe here.
     */
    if (copy_to_user(buf,
                     device_buffer + *offset,
                     bytes_to_read)) {
        mutex_unlock(&lab2_mutex);
        return -EFAULT;
    }

    *offset += bytes_to_read;

    /* Question 7 */
    total_bytes_read += bytes_to_read;

    /*
     * Once all current data is consumed, future reads
     * return EOF until another successful write occurs.
     */
    if (*offset >= buffer_len)
        data_available = false;

    pr_info("lab2_driver: sent %zu bytes to user\n",
            bytes_to_read);

    mutex_unlock(&lab2_mutex);

    return bytes_to_read;
}


/* Record statistics for a successful device write */
static void record_successful_write(size_t len)
{
    write_count++;
    total_bytes_written += len;
    last_write_size = len;

    /* Question 9 */
    last_write_time = ktime_get_real_seconds();
}


/* =========================================================
 * WRITE
 * ========================================================= */
static ssize_t lab2_write(struct file *file,
                          const char __user *buf,
                          size_t len,
                          loff_t *offset)
{
    size_t available;
    char command[7] = { 0 };

    /*
     * Questions 3 and 15:
     * Recognize both:
     *
     *   clear
     *   clear\n
     */
    if (len == 5 || len == 6) {
        if (copy_from_user(command, buf, len))
            return -EFAULT;

        if ((len == 5 &&
             memcmp(command, "clear", 5) == 0) ||
            (len == 6 &&
             memcmp(command, "clear\n", 6) == 0)) {

            mutex_lock(&lab2_mutex);

            device_buffer[0] = '\0';
            buffer_len = 0;
            data_available = false;

            record_successful_write(len);

            mutex_unlock(&lab2_mutex);

            pr_info("lab2_driver: buffer cleared\n");

            return len;
        }
    }

    mutex_lock(&lab2_mutex);

    /*
     * Questions 4 + 16.
     * max_size starts at 128 but can be modified via sysfs.
     */
    if (len > max_size)
        len = max_size;

    /* Question 14: append instead of overwrite */
    available = (BUFFER_SIZE - 1) - buffer_len;

    if (available == 0) {
        mutex_unlock(&lab2_mutex);
        return -ENOSPC;
    }

    if (len > available)
        len = available;

    /*
     * copy_from_user() is required because the source
     * pointer belongs to userspace.
     */
    if (copy_from_user(device_buffer + buffer_len,
                       buf,
                       len)) {
        mutex_unlock(&lab2_mutex);
        return -EFAULT;
    }

    buffer_len += len;
    device_buffer[buffer_len] = '\0';

    /* New data can now be read */
    data_available = true;

    /* Questions 2, 8, 9, 17 */
    record_successful_write(len);

    pr_info("lab2_driver: received %zu bytes\n", len);

    mutex_unlock(&lab2_mutex);

    return len;
}


/* =========================================================
 * IOCTL - Question 20
 * ========================================================= */
static long lab2_ioctl(struct file *file,
                       unsigned int cmd,
                       unsigned long arg)
{
    int len;

    switch (cmd) {

    case GET_BUFFER_LEN:

        mutex_lock(&lab2_mutex);
        len = (int)buffer_len;
        mutex_unlock(&lab2_mutex);

        if (copy_to_user((int __user *)arg,
                         &len,
                         sizeof(len)))
            return -EFAULT;

        return 0;


    case CLEAR_BUFFER:

        mutex_lock(&lab2_mutex);

        device_buffer[0] = '\0';
        buffer_len = 0;
        data_available = false;

        mutex_unlock(&lab2_mutex);

        pr_info("lab2_driver: buffer cleared by ioctl\n");

        return 0;


    default:
        return -ENOTTY;
    }
}


/* =========================================================
 * FILE OPERATIONS
 * ========================================================= */
static const struct file_operations lab2_fops = {
    .owner          = THIS_MODULE,
    .open           = lab2_open,
    .read           = lab2_read,
    .write          = lab2_write,
    .release        = lab2_release,
    .unlocked_ioctl = lab2_ioctl,
};


/* =========================================================
 * SYSFS ATTRIBUTES
 * ========================================================= */

/* Existing buffer_len */
static ssize_t buffer_len_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);
    ret = sysfs_emit(buf, "%zu\n", buffer_len);
    mutex_unlock(&lab2_mutex);

    return ret;
}

static DEVICE_ATTR_RO(buffer_len);


/* Existing open_count */
static ssize_t open_count_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);
    ret = sysfs_emit(buf, "%d\n", open_count);
    mutex_unlock(&lab2_mutex);

    return ret;
}

static DEVICE_ATTR_RO(open_count);


/* Existing last_data */
static ssize_t last_data_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);

    if (buffer_len > 0)
        ret = sysfs_emit(buf, "%s\n", device_buffer);
    else
        ret = sysfs_emit(buf, "(empty)\n");

    mutex_unlock(&lab2_mutex);

    return ret;
}

static DEVICE_ATTR_RO(last_data);


/* Question 1 */
static ssize_t read_count_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);
    ret = sysfs_emit(buf, "%d\n", read_count);
    mutex_unlock(&lab2_mutex);

    return ret;
}

static DEVICE_ATTR_RO(read_count);


/* Question 5 */
static ssize_t buffer_size_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
    return sysfs_emit(buf, "%d\n", BUFFER_SIZE);
}

static DEVICE_ATTR_RO(buffer_size);


/* Question 6 */
static ssize_t status_show(struct device *dev,
                           struct device_attribute *attr,
                           char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);

    if (buffer_len == 0)
        ret = sysfs_emit(buf, "EMPTY\n");
    else
        ret = sysfs_emit(buf, "DATA\n");

    mutex_unlock(&lab2_mutex);

    return ret;
}

static DEVICE_ATTR_RO(status);


/* Question 10 */
static ssize_t version_show(struct device *dev,
                            struct device_attribute *attr,
                            char *buf)
{
    return sysfs_emit(buf, "%s\n", DRIVER_VERSION);
}

static DEVICE_ATTR_RO(version);


/* Question 11 */
static ssize_t reset_stats_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf,
                                 size_t count)
{
    if (!sysfs_streq(buf, "reset"))
        return -EINVAL;

    mutex_lock(&lab2_mutex);

    open_count = 0;
    read_count = 0;
    write_count = 0;
    total_bytes_read = 0;
    total_bytes_written = 0;

    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: statistics reset\n");

    return count;
}

static DEVICE_ATTR_WO(reset_stats);


/* Question 16 */
static ssize_t max_size_show(struct device *dev,
                             struct device_attribute *attr,
                             char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);
    ret = sysfs_emit(buf, "%u\n", max_size);
    mutex_unlock(&lab2_mutex);

    return ret;
}


static ssize_t max_size_store(struct device *dev,
                              struct device_attribute *attr,
                              const char *buf,
                              size_t count)
{
    unsigned int value;

    if (kstrtouint(buf, 10, &value))
        return -EINVAL;

    if (value == 0 || value >= BUFFER_SIZE)
        return -EINVAL;

    mutex_lock(&lab2_mutex);
    max_size = value;
    mutex_unlock(&lab2_mutex);

    return count;
}

static DEVICE_ATTR_RW(max_size);


/* Question 17 */
static ssize_t last_write_size_show(
        struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);
    ret = sysfs_emit(buf, "%zu\n", last_write_size);
    mutex_unlock(&lab2_mutex);

    return ret;
}

static DEVICE_ATTR_RO(last_write_size);


static struct attribute *lab2_attrs[] = {
    &dev_attr_buffer_len.attr,
    &dev_attr_open_count.attr,
    &dev_attr_last_data.attr,

    &dev_attr_read_count.attr,
    &dev_attr_buffer_size.attr,
    &dev_attr_status.attr,
    &dev_attr_version.attr,
    &dev_attr_reset_stats.attr,
    &dev_attr_max_size.attr,
    &dev_attr_last_write_size.attr,

    NULL,
};


static const struct attribute_group lab2_attr_group = {
    .attrs = lab2_attrs,
};


/* =========================================================
 * PROCFS - Questions 2, 9, 18
 * ========================================================= */
static int lab2_proc_show(struct seq_file *m, void *v)
{
    mutex_lock(&lab2_mutex);

    seq_puts(m, "LAB-02 Extended Driver Statistics\n");
    seq_puts(m, "=================================\n");

    seq_printf(m, "Driver name         : %s\n",
               DEVICE_NAME);

    seq_printf(m, "Version             : %s\n",
               DRIVER_VERSION);

    seq_printf(m, "Major number        : %d\n",
               DRIVER_MAJOR);

    seq_printf(m, "Buffer size         : %d bytes\n",
               BUFFER_SIZE);

    seq_printf(m, "Data length         : %zu bytes\n",
               buffer_len);

    seq_printf(m, "Max write size      : %u bytes\n",
               max_size);

    seq_printf(m, "Open count          : %d\n",
               open_count);

    seq_printf(m, "Read count          : %d\n",
               read_count);

    seq_printf(m, "Write count         : %d\n",
               write_count);

    seq_printf(m, "Total bytes read    : %lu\n",
               total_bytes_read);

    seq_printf(m, "Total bytes written : %lu\n",
               total_bytes_written);

    seq_printf(m, "Last write size     : %zu\n",
               last_write_size);

    seq_printf(m, "Last write time     : %lld\n",
               (long long)last_write_time);

    seq_printf(m, "Buffer status       : %s\n",
               buffer_len == 0 ? "EMPTY" : "DATA");

    if (buffer_len > 0)
        seq_printf(m, "Last data           : [%s]\n",
                   device_buffer);
    else
        seq_puts(m, "Last data           : [(empty)]\n");

    mutex_unlock(&lab2_mutex);

    return 0;
}


static int lab2_proc_open(struct inode *inode,
                          struct file *file)
{
    return single_open(file, lab2_proc_show, NULL);
}


static const struct proc_ops lab2_proc_fops = {
    .proc_open    = lab2_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};


/* =========================================================
 * MODULE INIT
 * ========================================================= */
static int __init lab2_init(void)
{
    int ret;

    pr_info("lab2_driver: initializing extended practice module\n");

    mutex_lock(&lab2_mutex);

    device_buffer[0] = '\0';
    buffer_len = 0;

    open_count = 0;
    read_count = 0;
    write_count = 0;

    total_bytes_read = 0;
    total_bytes_written = 0;

    last_write_size = 0;
    last_write_time = 0;

    max_size = 128;
    data_available = false;

    mutex_unlock(&lab2_mutex);

    ret = register_chrdev(DRIVER_MAJOR,
                          DEVICE_NAME,
                          &lab2_fops);

    if (ret < 0) {
        pr_err("lab2_driver: register_chrdev failed\n");
        return ret;
    }


    lab2_class = class_create(THIS_MODULE,
                              CLASS_NAME);

    if (IS_ERR(lab2_class)) {
        ret = PTR_ERR(lab2_class);

        unregister_chrdev(DRIVER_MAJOR,
                          DEVICE_NAME);

        return ret;
    }


    lab2_device = device_create(lab2_class,
                                NULL,
                                MKDEV(DRIVER_MAJOR, 0),
                                NULL,
                                DEVICE_NAME);

    if (IS_ERR(lab2_device)) {
        ret = PTR_ERR(lab2_device);

        class_destroy(lab2_class);

        unregister_chrdev(DRIVER_MAJOR,
                          DEVICE_NAME);

        return ret;
    }


    lab2_proc = proc_create("lab2_info",
                            0444,
                            NULL,
                            &lab2_proc_fops);

    if (!lab2_proc) {
        device_destroy(lab2_class,
                       MKDEV(DRIVER_MAJOR, 0));

        class_destroy(lab2_class);

        unregister_chrdev(DRIVER_MAJOR,
                          DEVICE_NAME);

        return -ENOMEM;
    }


    ret = sysfs_create_group(&lab2_device->kobj,
                             &lab2_attr_group);

    if (ret) {
        proc_remove(lab2_proc);

        device_destroy(lab2_class,
                       MKDEV(DRIVER_MAJOR, 0));

        class_destroy(lab2_class);

        unregister_chrdev(DRIVER_MAJOR,
                          DEVICE_NAME);

        return ret;
    }


    pr_info("lab2_driver: /proc/lab2_info created\n");
    pr_info("lab2_driver: loaded, major=%d\n",
            DRIVER_MAJOR);

    return 0;
}


/* =========================================================
 * MODULE EXIT
 * ========================================================= */
static void __exit lab2_exit(void)
{
    sysfs_remove_group(&lab2_device->kobj,
                       &lab2_attr_group);

    if (lab2_proc)
        proc_remove(lab2_proc);

    device_destroy(lab2_class,
                   MKDEV(DRIVER_MAJOR, 0));

    class_destroy(lab2_class);

    unregister_chrdev(DRIVER_MAJOR,
                      DEVICE_NAME);

    pr_info("lab2_driver: module unloaded\n");
}


module_init(lab2_init);
module_exit(lab2_exit);
