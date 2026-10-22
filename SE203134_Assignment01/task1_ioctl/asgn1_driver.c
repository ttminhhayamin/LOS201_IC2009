#include "asgn1_ioctl.h"
#include <linux/string.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/mutex.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tran Tuan Minh - SE203134");
MODULE_DESCRIPTION("Assignment 1 - Char Device Driver");
MODULE_VERSION("Se203134 - 1.0");

#define DEVICE_NAME "asgn1"
#define CLASS_NAME "asgn1_class"
#define BUFFER_SIZE 1024
#define DRIVER_MAJOR 241


static int major_number = DRIVER_MAJOR;

static char device_buffer[BUFFER_SIZE];
static int buffer_len = 0;
static int open_count = 0;

static int read_count = 0;
static int write_count = 0;
static int last_write_size = 0;

static int asgn1_mode = ASGN1_MODE_ECHO;
static struct class *lab2_class = NULL;
static struct device *lab2_device = NULL;

static DEFINE_MUTEX(lab2_mutex);
static struct proc_dir_entry *proc_entry;

static int lab2_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Assignment 01 Driver Statistics\n");
    seq_printf(m, "======================\n");
    seq_printf(m, "Driver name : %s\n", DEVICE_NAME);
    seq_printf(m, "Major number : %d\n", major_number);
    seq_printf(m, "Buffer size : %d bytes\n", BUFFER_SIZE);
    seq_printf(m, "Data length : %d bytes\n", buffer_len);
    seq_printf(m, "Open count : %d\n", open_count);

    if (buffer_len > 0)
        seq_printf(m, "Last data : [%s]\n", device_buffer);

    return 0;
}

static int lab2_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, lab2_proc_show, NULL);
}

static const struct proc_ops lab2_proc_fops = {
    .proc_open    = lab2_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};


static ssize_t buffer_len_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    return sprintf(buf, "%d\n", buffer_len);
}

static ssize_t open_count_show(struct device *dev,
                               struct device_attribute *attr,
                               char *buf)
{
    return sprintf(buf, "%d\n", open_count);
}

static ssize_t last_data_show(struct device *dev,
                              struct device_attribute *attr,
                              char *buf)
{
    if (buffer_len > 0)
        return sprintf(buf, "%s\n", device_buffer);

    return sprintf(buf, "(empty)\n");
}

static DEVICE_ATTR_RO(buffer_len);
static DEVICE_ATTR_RO(open_count);
static DEVICE_ATTR_RO(last_data);

static struct attribute *lab2_attrs[] = {
    &dev_attr_buffer_len.attr,
    &dev_attr_open_count.attr,
    &dev_attr_last_data.attr,
    NULL,
};

static struct attribute_group lab2_attr_group = {
    .attrs = lab2_attrs,
};


static int lab2_open(struct inode *inode, struct file *file)
{
    mutex_lock(&lab2_mutex);
    open_count++;
    mutex_unlock(&lab2_mutex);

    pr_info("asgn1_driver: device opened (count=%d)\n",
            open_count);

    return 0;
}

static int lab2_release(struct inode *inode, struct file *file)
{
    pr_info("asgn1_driver: device closed\n");

    return 0;
}


static ssize_t lab2_read(struct file *file,
                         char __user *buf,
                         size_t len,
                         loff_t *offset)
{
    ssize_t bytes_read;

    mutex_lock(&lab2_mutex);

    read_count++;

    if (*offset >= buffer_len) {
        mutex_unlock(&lab2_mutex);
        return 0;
    }

    if (len > buffer_len - *offset)
        len = buffer_len - *offset;

    if (copy_to_user(buf,
                     device_buffer + *offset,
                     len)) {

        mutex_unlock(&lab2_mutex);

        pr_err("asgn1_driver: copy_to_user failed\n");

        return -EFAULT;
    }

    *offset += len;

    bytes_read = len;

    mutex_unlock(&lab2_mutex);

    pr_info("asgn1_driver: sent %zd bytes to user\n",
            bytes_read);

    return bytes_read;
}
static ssize_t lab2_write(struct file *file,
                          const char __user *buf,
                          size_t len,
                          loff_t *offset)
{
    size_t prefix_len = 0;
    size_t bytes_to_copy = len;

    if (len == 0)
        return 0;

    mutex_lock(&lab2_mutex);

    if (asgn1_mode == ASGN1_MODE_KERNEL)
        prefix_len = sizeof("[KERNEL]") - 1;

    if (bytes_to_copy > BUFFER_SIZE - 1 - prefix_len)
        bytes_to_copy = BUFFER_SIZE - 1 - prefix_len;

    if (copy_from_user(device_buffer + prefix_len,
                       buf,
                       bytes_to_copy)) {

        mutex_unlock(&lab2_mutex);
        return -EFAULT;
    }

    if (prefix_len > 0)
        memcpy(device_buffer, "[KERNEL]", prefix_len);

    buffer_len = prefix_len + bytes_to_copy;

    device_buffer[buffer_len] = '\0';

    write_count++;
    last_write_size = (int)bytes_to_copy;

    mutex_unlock(&lab2_mutex);

    pr_info("asgn1_driver: wrote %zu user bytes\n",
            bytes_to_copy);

    return bytes_to_copy;
}
static long asgn1_ioctl(struct file *file,
                        unsigned int cmd,
                        unsigned long arg)
{
    switch (cmd) {

    case ASGN1_RESET_BUFFER:

        mutex_lock(&lab2_mutex);

        memset(device_buffer, 0, sizeof(device_buffer));

        buffer_len = 0;

        read_count = 0;
        write_count = 0;
        last_write_size = 0;

        mutex_unlock(&lab2_mutex);

        pr_info("asgn1_driver: ioctl RESET_BUFFER executed\n");

        return 0;
case ASGN1_GET_STATS:
{
    struct asgn1_stats stats;

    mutex_lock(&lab2_mutex);

    stats.open_count = open_count;
    stats.write_count = write_count;
    stats.read_count = read_count;
    stats.buffer_len = buffer_len;
    stats.last_write_size = last_write_size;

    mutex_unlock(&lab2_mutex);

    if (copy_to_user((void __user *)arg,
                     &stats,
                     sizeof(stats))) {
        return -EFAULT;
    }

    pr_info("asgn1_driver: ioctl GET_STATS executed\n");

    return 0;
}
case ASGN1_SET_MODE:
{
    int new_mode;

    if (copy_from_user(&new_mode,
                       (int __user *)arg,
                       sizeof(new_mode))) {
        return -EFAULT;
    }

    if (new_mode != ASGN1_MODE_KERNEL &&
        new_mode != ASGN1_MODE_ECHO) {
        return -EINVAL;
    }

    mutex_lock(&lab2_mutex);

    asgn1_mode = new_mode;

    mutex_unlock(&lab2_mutex);

    pr_info("asgn1_driver: ioctl SET_MODE = %d\n",
            new_mode);

    return 0;
}
case ASGN1_GET_VERSION:
{
    static const char version[ASGN1_VERSION_SIZE] =
        "asgn1_driver v1.0 - SE203134";

    if (copy_to_user((void __user *)arg,
                     version,
                     sizeof(version))) {

        pr_err("asgn1_driver: GET_VERSION failed\n");
        return -EFAULT;
    }

    pr_info("asgn1_driver: ioctl GET_VERSION executed\n");

    return 0;
}
    default:
        return -ENOTTY;
    }
}
static struct file_operations lab2_fops = {
    .owner   = THIS_MODULE,
    .open    = lab2_open,
    .release = lab2_release,
    .read    = lab2_read,
.unlocked_ioctl = asgn1_ioctl,
    .write   = lab2_write,
};


static int __init lab2_init(void)
{
    int ret;

    pr_info("asgn1_driver: initializing module\n");

    ret = register_chrdev(major_number,
                          DEVICE_NAME,
                          &lab2_fops);

    if (ret < 0) {
        pr_err("asgn1_driver: register_chrdev failed: %d\n",
               ret);
        return ret;
    }

    lab2_class = class_create(THIS_MODULE,
                              CLASS_NAME);

    if (IS_ERR(lab2_class)) {
        unregister_chrdev(major_number,
                          DEVICE_NAME);

        return PTR_ERR(lab2_class);
    }

    lab2_device =
        device_create(lab2_class,
                      NULL,
                      MKDEV(major_number, 0),
                      NULL,
                      DEVICE_NAME);

    if (IS_ERR(lab2_device)) {
        class_destroy(lab2_class);

        unregister_chrdev(major_number,
                          DEVICE_NAME);

        return PTR_ERR(lab2_device);
    }

    ret = sysfs_create_group(&lab2_device->kobj,
                             &lab2_attr_group);

    if (ret)
        pr_warn("asgn1_driver: sysfs_create_group failed\n");

    proc_entry = proc_create("asgn1_info",
                             0444,
                             NULL,
                             &lab2_proc_fops);

    if (!proc_entry)
        pr_warn("asgn1_driver: failed to create /proc/asgn1_info\n");
    else
        pr_info("asgn1_driver: /proc/asgn1_info created\n");

    pr_info("asgn1_driver: loaded, major=%d\n",
            major_number);

    return 0;
}


static void __exit lab2_exit(void)
{
    if (proc_entry)
        proc_remove(proc_entry);

    sysfs_remove_group(&lab2_device->kobj,
                       &lab2_attr_group);

    device_destroy(lab2_class,
                   MKDEV(major_number, 0));

    class_destroy(lab2_class);

    unregister_chrdev(major_number,
                      DEVICE_NAME);

    pr_info("asgn1_driver: module unloaded\n");
}


module_init(lab2_init);
module_exit(lab2_exit);

