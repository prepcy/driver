#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#define DEVICE_NAME "pps_dev"
#define CLASS_NAME	"pps_class"
#define IOCTL_SET_TIME _IOR('a', 1, int *)

static unsigned long param = 0;
static int pps_major;
static struct class* pps_class;
static struct device* pps_device;
static struct cdev pps_cdev;
static struct timer_list pps_timer;

module_param(param, ulong, 0644);

static void pps_timer_handler(struct timer_list *timer)
{
	if (param == 0)
		goto TIMER;

	struct timespec64 new_time; 
	new_time.tv_sec	= param;
	new_time.tv_nsec = 0;
	
	int ret = do_settimeofday64(&new_time);
	if (ret != 0)
		printk(KERN_ERR "pps failed to set system time\n");

	printk(KERN_INFO "pps timer handler ...\n");

TIMER:
	mod_timer(&pps_timer, jiffies + msecs_to_jiffies(1000));
}

static long pps_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch(cmd) {
	case IOCTL_SET_TIME:
		int ret = copy_from_user(&param, (int*)arg, sizeof(param));
		if (ret)
			return -EFAULT;
		printk(KERN_INFO "received parameter : %lu\n", param);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct file_operations pps_ops = {
	.unlocked_ioctl = pps_ioctl,
};

static int __init pps_init(void)
{
	/* Register the driver and assign a major device number */
	pps_major = register_chrdev(0, DEVICE_NAME, &pps_ops);
	if (pps_major < 0) {
		printk(KERN_ALERT "fail to register pps device : %d\n", pps_major);
		return pps_major;
	}
#if 0
	/* Creating a device class */
	//pps_class = class_create(THIS_MODULE, CLASS_NAME);
	pps_class = class_create(CLASS_NAME);
	if (IS_ERR(pps_class)) {
		unregister_chrdev(pps_major, DEVICE_NAME);
		printk(KERN_ALERT "fail to creat pps class\n");
		return PTR_ERR(pps_class);
	}
	
	/* Creating a device node */
	pps_device = device_create(pps_class, NULL, MKDEV(pps_major, 0), NULL, DEVICE_NAME);
	if (IS_ERR(pps_device)) {
		class_destroy(pps_class);
		unregister_chrdev(pps_major, DEVICE_NAME);
		printk(KERN_ALERT "fail to creat pps device\n");
		return PTR_ERR(pps_device);
	}
	
	cdev_init(&pps_cdev, &pps_ops);
	pps_cdev.owner = THIS_MODULE;
	if (cdev_add(&pps_cdev, MKDEV(pps_major, 0), 1) < 0) {
		device_destroy(pps_class, MKDEV(pps_major, 0));
		class_destroy(pps_class);
		unregister_chrdev(pps_major, DEVICE_NAME);
		printk(KERN_ALERT "fail to add pps cdev\n");
	}
#endif
	printk(KERN_INFO "loading pps driver\n");
	
	/* Initialize timer */
	timer_setup(&pps_timer, pps_timer_handler, 0);
	/* Set the timer */
	mod_timer(&pps_timer, jiffies + msecs_to_jiffies(1000));

	return 0;
}

static void __exit pps_exit(void)
{
	del_timer(&pps_timer);
#if 0
	device_destroy(pps_class, MKDEV(pps_major, 0));
	class_unregister(pps_class);
	class_destroy(pps_class);
#endif
	unregister_chrdev(pps_major, DEVICE_NAME);

	printk(KERN_INFO "unloading pps driver\n");
}

module_init(pps_init);
module_exit(pps_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fan Loong");
MODULE_DESCRIPTION("PPS DRIVER");

