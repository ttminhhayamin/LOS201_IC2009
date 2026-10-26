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
#include "asgn1_ioctl.h"
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Lab");
MODULE_DESCRIPTION("Assignment-01: Simple Character Device Driver");
MODULE_VERSION("1.0");
#define DEVICE_NAME     "asgn1"
#define CLASS_NAME      "asgn1_class"
#define BUFFER_SIZE     1024
#define DRIVER_MAJOR    241
#define STUDENT_NAME    "Hoang Trung Hai"
#define STUDENT_ID      "SS204779"
#define ASGN1_MODE_KERNEL   0
#define ASGN1_MODE_ECHO     1
#define ASGN1_VERSION       "ASGN1 Driver v1.0"
#define ASGN1_KERNEL_PREFIX "[KERNEL] "

static int    major_number = DRIVER_MAJOR;
static char   device_buffer[BUFFER_SIZE];
static int    buffer_len = 0;
static int    open_count = 0;
static int    write_count = 0;
static int    read_count = 0;
static int    last_write_size = 0;
static int    current_mode = ASGN1_MODE_ECHO;
static struct class  *asgn1_class  = NULL;
static struct device *asgn1_device = NULL;
static DEFINE_MUTEX(asgn1_mutex);


/* ===== PROCFS Implementation ===== */
static struct proc_dir_entry *proc_entry;

static int asgn1_proc_show(struct seq_file *m, void *v) {
    seq_printf(m, "ASGN1 Driver Statistics\n");
    seq_printf(m, "======================\n");
    seq_printf(m, "Driver name   : %s\n", DEVICE_NAME);
    seq_printf(m, "Student name  : %s\n", STUDENT_NAME);
    seq_printf(m, "Student ID    : %s\n", STUDENT_ID);
    seq_printf(m, "Major number  : %d\n", major_number);
    seq_printf(m, "Buffer size   : %d bytes\n", BUFFER_SIZE);
    seq_printf(m, "Data length   : %d bytes\n", buffer_len);
    seq_printf(m, "Open count    : %d\n", open_count);
    if (buffer_len > 0)
        seq_printf(m, "Last data     : [%s]\n", device_buffer);
    return 0;
}

static int asgn1_proc_open(struct inode *inode, struct file *file) {
    return single_open(file, asgn1_proc_show, NULL);
}

static const struct proc_ops asgn1_proc_fops = {
    .proc_open    = asgn1_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};
static ssize_t buffer_len_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", buffer_len);
}

static ssize_t open_count_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", open_count);
}

static ssize_t last_data_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    if (buffer_len > 0)
        return sprintf(buf, "%s\n", device_buffer);

    return sprintf(buf, "(empty)\n");
}

static ssize_t student_name_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", STUDENT_NAME);
}

static ssize_t student_id_show(struct device *dev,
                struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", STUDENT_ID);
}

static long asgn1_ioctl(struct file *file,
                        unsigned int cmd,
                        unsigned long arg)
{
    struct asgn1_stats stats;
    int mode;
    char version[ASGN1_VERSION_SIZE];

    pr_info("asgn1_driver: ioctl cmd=0x%x\n", cmd);

    switch (cmd) {

    case ASGN1_RESET_BUFFER:
        mutex_lock(&asgn1_mutex);

        memset(device_buffer, 0, BUFFER_SIZE);
        buffer_len = 0;
        write_count = 0;
        read_count =0;
        last_write_size = 0;

        mutex_unlock(&asgn1_mutex);
        return 0;

    case ASGN1_GET_STATS:
        mutex_lock(&asgn1_mutex);

        stats.open_count      = open_count;
        stats.write_count     = write_count;
        stats.read_count      = read_count;
        stats.buffer_len      = buffer_len;
        stats.last_write_size = last_write_size;

        mutex_unlock(&asgn1_mutex);

        if (copy_to_user((void __user *)arg,
                         &stats,
                         sizeof(stats)))
            return -EFAULT;

        return 0;

    case ASGN1_SET_MODE:
        if (copy_from_user(&mode,
                           (void __user *)arg,
                           sizeof(mode)))
            return -EFAULT;

       if (mode != ASGN1_MODE_KERNEL &&
                                        mode != ASGN1_MODE_ECHO)
                                        return -EINVAL;

        mutex_lock(&asgn1_mutex);
        current_mode = mode;
        mutex_unlock(&asgn1_mutex);

        return 0;

    case ASGN1_GET_VERSION:
          memset(version, 0, sizeof(version));

    strncpy(version, ASGN1_VERSION, ASGN1_VERSION_SIZE - 1);

    if (copy_to_user((void __user *)arg,
                 version,
                 sizeof(version)))
    return -EFAULT;

return 0;

    default:
        pr_warn("asgn1_driver: unknown ioctl cmd=0x%x\n", cmd);
        return -ENOTTY;
    }
}

static DEVICE_ATTR_RO(buffer_len);
static DEVICE_ATTR_RO(open_count);
static DEVICE_ATTR_RO(last_data);
static DEVICE_ATTR_RO(student_name);
static DEVICE_ATTR_RO(student_id);

static struct attribute *asgn1_attrs[] = {
    &dev_attr_buffer_len.attr,
    &dev_attr_open_count.attr,
    &dev_attr_last_data.attr,
    &dev_attr_student_name.attr,
    &dev_attr_student_id.attr,
    NULL,
};

static struct attribute_group asgn1_attr_group = {
    .attrs = asgn1_attrs,
};
static int asgn1_open(struct inode *inode, struct file *file) {
    mutex_lock(&asgn1_mutex);
    open_count++;
    mutex_unlock(&asgn1_mutex);
    pr_info("asgn1_driver: device opened (count=%d)\n", open_count);
    return 0;
}
static int asgn1_release(struct inode *inode, struct file *file) {
    pr_info("asgn1_driver: device closed\n");
    return 0;
}
static ssize_t asgn1_read(struct file *file, char __user *buf,
                         size_t len, loff_t *offset)
{
    int bytes_read = 0;

    mutex_lock(&asgn1_mutex);

    if (*offset >= buffer_len) {
        mutex_unlock(&asgn1_mutex);
        return 0;  /* EOF */
    }

    if (len > buffer_len - *offset)
        len = buffer_len - *offset;

    if (copy_to_user(buf, device_buffer + *offset, len)) {
        mutex_unlock(&asgn1_mutex);
        pr_err("asgn1_driver: copy_to_user failed\n");
        return -EFAULT;
    }

    *offset += len;
    bytes_read = len;
    read_count++;

    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1_driver: sent %d bytes to user\n", bytes_read);
    return bytes_read;
}

static ssize_t asgn1_write(struct file *file, const char __user *buf,
                          size_t len, loff_t *offset) {
    char temp_buffer[BUFFER_SIZE];
    mutex_lock(&asgn1_mutex);

    if (current_mode == ASGN1_MODE_KERNEL) {
        if (len > BUFFER_SIZE - 1 - strlen(ASGN1_KERNEL_PREFIX)) {
            pr_warn("asgn1_driver: write too large for kernel mode, truncating\n");
            len = BUFFER_SIZE - 1 - strlen(ASGN1_KERNEL_PREFIX);
        }
    } else {
        if (len > BUFFER_SIZE - 1) {
            pr_warn("asgn1_driver: write too large, truncating\n");
            len = BUFFER_SIZE - 1;
        }
    }
    if (copy_from_user(temp_buffer, buf, len)) {
        mutex_unlock(&asgn1_mutex);
        return -EFAULT;
    }

    if (current_mode == ASGN1_MODE_KERNEL) {
        strcpy(device_buffer, ASGN1_KERNEL_PREFIX);
        memcpy(device_buffer + strlen(ASGN1_KERNEL_PREFIX),
               temp_buffer, len);
        buffer_len = strlen(ASGN1_KERNEL_PREFIX) + len;
    } else {
        memcpy(device_buffer, temp_buffer, len);
        buffer_len = len;
    }

    device_buffer[buffer_len] = '\0';

    write_count++;
    last_write_size = len;
    mutex_unlock(&asgn1_mutex);
    pr_info("asgn1_driver: received %zu bytes: [%s]\n", len, device_buffer);
    return len;
}
static struct file_operations asgn1_fops = {
    .owner   = THIS_MODULE,
    .open    = asgn1_open,
    .release = asgn1_release,
    .read    = asgn1_read,
    .write   = asgn1_write,
    .unlocked_ioctl = asgn1_ioctl,
};
static int __init asgn1_init(void) {
    int ret;
    pr_info("asgn1_driver: initializing module\n");
    /* Đăng ký character device với major number cố định */
    ret = register_chrdev(major_number, DEVICE_NAME, &asgn1_fops);
    if (ret < 0) {
        pr_err("asgn1_driver: register_chrdev failed: %d\n", ret);
        return ret;
    }
    /* Tạo device class và device node tự động trong /sys */
    asgn1_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(asgn1_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(asgn1_class);
    }
    asgn1_device = device_create(asgn1_class, NULL,
                    MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(asgn1_device)) {
        class_destroy(asgn1_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(asgn1_device);
    }
    /* Đăng ký sysfs attribute group */
    ret = sysfs_create_group(&asgn1_device->kobj, &asgn1_attr_group);
    if (ret)
        pr_warn("asgn1_driver: sysfs_create_group failed\n");
    pr_info("asgn1_driver: loaded, major=%d\n", major_number);
/* Tạo /proc/asgn1_info */
proc_entry = proc_create("asgn1_info", 0444, NULL, &asgn1_proc_fops);

if (!proc_entry)
    pr_warn("asgn1_driver: failed to create /proc/asgn1_info\n");
else
    pr_info("asgn1_driver: /proc/asgn1_info created\n");

return 0;

}
static void __exit asgn1_exit(void) {
if (proc_entry)
    proc_remove(proc_entry);
 sysfs_remove_group(&asgn1_device->kobj, &asgn1_attr_group);
    device_destroy(asgn1_class, MKDEV(major_number, 0));
    class_destroy(asgn1_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("asgn1_driver: module unloaded\n");
}

module_init(asgn1_init);
module_exit(asgn1_exit);
