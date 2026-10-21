#include <linux/ioctl.h>
#include <linux/string.h>
#include "asgn1_ioctl.h"
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
MODULE_AUTHOR("NhomTuanMinh");
MODULE_DESCRIPTION("Assignment 01: IOCTL Character Device Driver");
MODULE_VERSION("1.0");

#define DEVICE_NAME     "asgn1"
#define CLASS_NAME      "asgn1_class"
#define BUFFER_SIZE     1024
#define DRIVER_MAJOR    241

static int    major_number = DRIVER_MAJOR;
static char   device_buffer[BUFFER_SIZE];
static int    buffer_len = 0;
static int    open_count = 0;
static int write_count = 0;
static int read_count = 0;
static int last_write_size = 0;
static int driver_mode = 1;
static struct class  *lab2_class  = NULL;
static struct device *lab2_device = NULL;
static DEFINE_MUTEX(lab2_mutex);

static int lab2_open(struct inode *inode, struct file *file) {
    mutex_lock(&lab2_mutex);
    open_count++;
    mutex_unlock(&lab2_mutex);
    pr_info("lab2_driver: device opened (count=%d)\n", open_count);
    return 0;
}

static int lab2_release(struct inode *inode, struct file *file) {
    pr_info("lab2_driver: device closed\n");
    return 0;
}

static ssize_t lab2_read(struct file *file, char __user *buf,
                         size_t len, loff_t *offset)
{
    size_t bytes_read;

    if (mutex_lock_interruptible(&lab2_mutex))
        return -ERESTARTSYS;

    if (*offset >= buffer_len) {
        mutex_unlock(&lab2_mutex);
        return 0;
    }

    if (len > buffer_len - *offset)
        len = buffer_len - *offset;

    if (copy_to_user(buf, device_buffer + *offset, len)) {
        mutex_unlock(&lab2_mutex);
        return -EFAULT;
    }

    *offset += len;
    bytes_read = len;
    read_count++;

    mutex_unlock(&lab2_mutex);

    pr_info("asgn1_driver: read %zu bytes\n", bytes_read);

    return bytes_read;
}

static ssize_t lab2_write(struct file *file,
                          const char __user *buf,
                          size_t len, loff_t *offset)
{
    char temp[BUFFER_SIZE];
    size_t prefix_len = 0;
    size_t data_len;

    if (len == 0)
        return 0;

    if (mutex_lock_interruptible(&lab2_mutex))
        return -ERESTARTSYS;

    if (driver_mode == 0) {
        prefix_len = strlen("[KERNEL]");
    }

    if (len > BUFFER_SIZE - 1 - prefix_len)
        len = BUFFER_SIZE - 1 - prefix_len;

    if (copy_from_user(temp, buf, len)) {
        mutex_unlock(&lab2_mutex);
        return -EFAULT;
    }

    data_len = len;

    if (prefix_len > 0) {
        memcpy(device_buffer, "[KERNEL]", prefix_len);
    }

    memcpy(device_buffer + prefix_len, temp, data_len);

    buffer_len = prefix_len + data_len;
    device_buffer[buffer_len] = '\0';

    write_count++;
    last_write_size = data_len;

    mutex_unlock(&lab2_mutex);

    pr_info("asgn1_driver: write %zu bytes, mode=%d\n",
            data_len, driver_mode);

    return data_len;
}

static long asgn1_ioctl(struct file *file,
                        unsigned int cmd,
                        unsigned long arg)
{
    int mode;
    struct asgn1_stats stats;
    char version[64] = "asgn1_driver v1.0";

    switch (cmd) {
    case ASGN1_RESET_BUFFER:
        pr_info("asgn1_driver: RESET_BUFFER called\n");

        mutex_lock(&lab2_mutex);

        memset(device_buffer, 0, BUFFER_SIZE);
        buffer_len = 0;
        write_count = 0;
        read_count = 0;
        last_write_size = 0;

        mutex_unlock(&lab2_mutex);
        return 0;

    case ASGN1_GET_STATS:
        pr_info("asgn1_driver: GET_STATS called\n");

        mutex_lock(&lab2_mutex);

        stats.open_count = open_count;
        stats.write_count = write_count;
        stats.read_count = read_count;
        stats.buffer_len = buffer_len;
        stats.last_write_size = last_write_size;

        mutex_unlock(&lab2_mutex);

        if (copy_to_user((void __user *)arg,
                         &stats, sizeof(stats)))
            return -EFAULT;

        return 0;

    case ASGN1_SET_MODE:
        pr_info("asgn1_driver: SET_MODE called\n");

        if (copy_from_user(&mode, (int __user *)arg,
                           sizeof(mode)))
            return -EFAULT;

        if (mode != 0 && mode != 1)
            return -EINVAL;

        mutex_lock(&lab2_mutex);
        driver_mode = mode;
        mutex_unlock(&lab2_mutex);

        pr_info("asgn1_driver: mode set to %d\n", mode);
        return 0;

    case ASGN1_GET_VERSION:
        pr_info("asgn1_driver: GET_VERSION called\n");

        if (copy_to_user((void __user *)arg,
                         version, sizeof(version)))
            return -EFAULT;

        return 0;

    default:
        pr_info("asgn1_driver: unknown ioctl command\n");
        return -ENOTTY;
    }
}
static struct file_operations lab2_fops = {
    .owner          = THIS_MODULE,
    .open            = lab2_open,
    .release         = lab2_release,
    .read             = lab2_read,
    .write            = lab2_write,
    .unlocked_ioctl   = asgn1_ioctl,
};

static int __init lab2_init(void) {
    int ret;
    pr_info("lab2_driver: initializing module\n");
    /* Đăng ký character device với major number cố định */
    ret = register_chrdev(major_number, DEVICE_NAME, &lab2_fops);
    if (ret < 0) {
        pr_err("lab2_driver: register_chrdev failed: %d\n", ret);
        return ret;
    }
    /* Tạo device class và device node tự động trong /sys */
    lab2_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(lab2_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(lab2_class);
   }
    lab2_device = device_create(lab2_class, NULL,
                    MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(lab2_device)) {
        class_destroy(lab2_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(lab2_device);
    }
    pr_info("lab2_driver: loaded, major=%d\n", major_number);
    return 0;
}

static void __exit lab2_exit(void) {
    device_destroy(lab2_class, MKDEV(major_number, 0));
    class_destroy(lab2_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("lab2_driver: module unloaded\n");
}

module_init(lab2_init);
module_exit(lab2_exit);
