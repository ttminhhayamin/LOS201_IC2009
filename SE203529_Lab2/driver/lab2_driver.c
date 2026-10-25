/*
 * lab2_driver.c - LAB-02 Embedded Linux Systems
 * Character device driver + procfs (/proc/lab2_info) + sysfs (/sys/class/lab2_class/lab2/)
 * Target: ARMv7 (vexpress-a9), kernel 5.15
 */
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
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Lab");
MODULE_DESCRIPTION("LAB-02: Simple Character Device Driver");
MODULE_VERSION("1.0");

#define DEVICE_NAME   "lab2"
#define CLASS_NAME    "lab2_class"
#define BUFFER_SIZE   1024
#define DRIVER_MAJOR  240   /* 240-254: dai major danh cho local/experimental */

/* class_create() doi chu ky tu kernel 6.4 -> bao ve de build duoc tren ca hai */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
#define LAB2_CLASS_CREATE(n) class_create(n)
#else
#define LAB2_CLASS_CREATE(n) class_create(THIS_MODULE, n)
#endif

static int    major_number = DRIVER_MAJOR;
static char   device_buffer[BUFFER_SIZE];   /* bo dem noi bo cua driver (kernel space) */
static int    buffer_len = 0;               /* so byte hop le trong device_buffer */
static int    open_count = 0;               /* thong ke: so lan open() */
static struct class  *lab2_class  = NULL;
static struct device *lab2_device = NULL;

/*
 * MUTEX: kernel chay da lenh (SMP, QEMU -smp 2) va nhieu process co the cung mo /dev/lab2.
 * device_buffer, buffer_len, open_count la tai nguyen dung chung -> neu hai luong cung
 * ghi se xay ra race condition (du lieu tron lan, buffer_len lech noi dung, open_count
 * mat lan dem do "read-modify-write" khong nguyen tu). Mutex dam bao chi 1 luong
 * o trong critical section tai mot thoi diem. Dung mutex (co the ngu) thay vi spinlock
 * vi copy_to/from_user co the gay page fault -> co the sleep.
 */
static DEFINE_MUTEX(lab2_mutex);

/* ===================== PROCFS ===================== */
static struct proc_dir_entry *proc_entry;

/*
 * seq_file API: giup in ra /proc an toan voi buffer co kich thuoc bat ky (tu xu ly
 * phan trang, offset). Ham show chi can seq_printf() vao seq_file 'm'.
 */
static int lab2_proc_show(struct seq_file *m, void *v)
{
	mutex_lock(&lab2_mutex);
	seq_printf(m, "LAB-02 Driver Statistics\n");
	seq_printf(m, "======================\n");
	seq_printf(m, "Driver name   : %s\n", DEVICE_NAME);
	seq_printf(m, "Major number  : %d\n", major_number);
	seq_printf(m, "Buffer size   : %d bytes\n", BUFFER_SIZE);
	seq_printf(m, "Data length   : %d bytes\n", buffer_len);
	seq_printf(m, "Open count    : %d\n", open_count);
	if (buffer_len > 0)
		seq_printf(m, "Last data     : [%s]\n", device_buffer);
	mutex_unlock(&lab2_mutex);
	return 0;
}

/*
 * single_open(): helper cua seq_file cho truong hop ra du lieu "mot lan" (khong lap
 * qua danh sach). No gan lab2_proc_show lam ham show va cap phat seq_file cho 'file'.
 * Phai di cung single_release() de giai phong.
 */
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

/* ===================== SYSFS ===================== */
/*
 * DEVICE_ATTR_RO(name): macro sinh ra 'struct device_attribute dev_attr_name' voi mode
 * 0444, gan ham name_show() lam callback khi userspace doc file. Moi attribute la mot
 * file trong thu muc cua device. Co che kobject: moi struct device nhung mot kobject;
 * kobject chinh la mot thu muc trong /sys, attribute_group them cac file vao thu muc do
 * (sysfs_create_group). Quy tac sysfs: moi file chi chua MOT gia tri.
 */
static ssize_t buffer_len_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", buffer_len);
}

static ssize_t open_count_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%d\n", open_count);
}

static ssize_t last_data_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	if (buffer_len > 0)
		return sysfs_emit(buf, "%s\n", device_buffer);
	return sysfs_emit(buf, "(empty)\n");
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

/* ===================== FILE OPERATIONS ===================== */

/* open(): duoc goi khi userspace open("/dev/lab2"). Tang bo dem, tra 0 = thanh cong. */
static int lab2_open(struct inode *inode, struct file *file)
{
	mutex_lock(&lab2_mutex);
	open_count++;
	mutex_unlock(&lab2_mutex);
	pr_info("lab2_driver: device opened (count=%d)\n", open_count);
	return 0;
}

/* release(): goi khi file descriptor cuoi cung bi close(). Khong co gi de don dep. */
static int lab2_release(struct inode *inode, struct file *file)
{
	pr_info("lab2_driver: device closed\n");
	return 0;
}

/*
 * read(): chep du lieu tu device_buffer (kernel) sang buf (userspace).
 * Dung copy_to_user() KHONG dung memcpy() vi 'buf' la con tro userspace: no co the
 * sai/khong duoc map/bi swap-out/tro vao vung kernel (loi bao mat). copy_to_user kiem
 * tra dia chi (access_ok), xu ly page fault an toan va tra ve so byte CHUA chep duoc
 * (khac 0 -> ta tra -EFAULT). memcpy trong kernel se gay Oops hoac lo bo nho kernel.
 * Tra ve 0 nghia la EOF (de 'cat' dung lai).
 */
static ssize_t lab2_read(struct file *file, char __user *buf,
			 size_t len, loff_t *offset)
{
	int bytes_read;

	mutex_lock(&lab2_mutex);
	if (*offset >= buffer_len) {
		mutex_unlock(&lab2_mutex);
		return 0;               /* EOF */
	}
	if (len > buffer_len - *offset)
		len = buffer_len - *offset;

	if (copy_to_user(buf, device_buffer + *offset, len)) {
		mutex_unlock(&lab2_mutex);
		pr_err("lab2_driver: copy_to_user failed\n");
		return -EFAULT;
	}
	*offset += len;
	bytes_read = len;
	mutex_unlock(&lab2_mutex);

	pr_info("lab2_driver: sent %d bytes to user\n", bytes_read);
	return bytes_read;
}

/*
 * write(): chep tu buf (userspace) vao device_buffer (kernel) bang copy_from_user()
 * (ly do giong copy_to_user o tren). Mutex bao ve de hai process ghi dong thoi khong
 * lam hong device_buffer/buffer_len. Ghi de toan bo buffer (khong append).
 */
static ssize_t lab2_write(struct file *file, const char __user *buf,
			  size_t len, loff_t *offset)
{
	if (len > BUFFER_SIZE - 1) {
		pr_warn("lab2_driver: write too large, truncating\n");
		len = BUFFER_SIZE - 1;
	}

	mutex_lock(&lab2_mutex);
	if (copy_from_user(device_buffer, buf, len)) {
		mutex_unlock(&lab2_mutex);
		return -EFAULT;
	}
	buffer_len = len;
	device_buffer[buffer_len] = '\0';
	mutex_unlock(&lab2_mutex);

	pr_info("lab2_driver: received %zu bytes: [%s]\n", len, device_buffer);
	return len;
}

/* Bang con tro ham: VFS dung bang nay de chuyen syscall open/read/write/close vao driver */
static struct file_operations lab2_fops = {
	.owner   = THIS_MODULE,
	.open    = lab2_open,
	.release = lab2_release,
	.read    = lab2_read,
	.write   = lab2_write,
};

/*
 * init(): chay khi insmod. Dang ky chardev (major/minor) -> tao class -> tao device
 * (sinh /sys/class/lab2_class/lab2 va /dev/lab2 neu co devtmpfs) -> sysfs group -> procfs.
 * Neu buoc nao loi phai huy nguoc lai cac buoc truoc do.
 */
static int __init lab2_init(void)
{
	int ret;

	pr_info("lab2_driver: initializing module\n");

	ret = register_chrdev(major_number, DEVICE_NAME, &lab2_fops);
	if (ret < 0) {
		pr_err("lab2_driver: register_chrdev failed: %d\n", ret);
		return ret;
	}

	lab2_class = LAB2_CLASS_CREATE(CLASS_NAME);
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

	ret = sysfs_create_group(&lab2_device->kobj, &lab2_attr_group);
	if (ret)
		pr_warn("lab2_driver: sysfs_create_group failed\n");

	proc_entry = proc_create("lab2_info", 0444, NULL, &lab2_proc_fops);
	if (!proc_entry)
		pr_warn("lab2_driver: failed to create /proc/lab2_info\n");
	else
		pr_info("lab2_driver: /proc/lab2_info created\n");

	pr_info("lab2_driver: loaded, major=%d\n", major_number);
	return 0;
}

/* exit(): chay khi rmmod. Giai phong theo thu tu NGUOC voi init. */
static void __exit lab2_exit(void)
{
	if (proc_entry)
		proc_remove(proc_entry);
	sysfs_remove_group(&lab2_device->kobj, &lab2_attr_group);
	device_destroy(lab2_class, MKDEV(major_number, 0));
	class_destroy(lab2_class);
	unregister_chrdev(major_number, DEVICE_NAME);
	pr_info("lab2_driver: module unloaded\n");
}

module_init(lab2_init);
module_exit(lab2_exit);
