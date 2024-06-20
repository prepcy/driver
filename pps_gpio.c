#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/gpio.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

#define GPIO_NUM 1
#define IRQF_TRIGGER IRQF_TRIGGER_FALLING
#define DEVICE_NAME "pps_dev"
#define CLASS_NAME	"pps_class"
#define IOCTL_SET_TIME _IOR('a', 1, int *)

static unsigned long param = 0;
static int pps_major;
static int pps_irq;
static struct class* pps_class;
static struct device* pps_device;
static struct cdev pps_cdev;
static struct gpio_desc *gpio_desc;

module_param(param, ulong, 0644);

static irqreturn_t pps_timer_handler(int irq, void *dev_id)
{
	if (param == 0)
		return IRQ_HANDLED;

	struct timespec64 new_time; 
	new_time.tv_sec	= param;
	new_time.tv_nsec = 0;
	
	int ret = do_settimeofday64(&new_time);
	if (ret != 0)
		printk(KERN_ERR "pps failed to set system time\n");

	printk(KERN_INFO "pps gpio handler ...\n");
	return IRQ_HANDLED;
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

	printk(KERN_INFO "loading pps driver\n");
	
	if (gpio_is_valid(GPIO_NUM)) {
		printk(KERN_ALERT "fail to get gpio num : %d\n", GPIO_NUM);
		return -ENODEV;
	}


	gpio_desc = gpio_to_desc(GPIO_NUM);
	if (gpio_desc == NULL) {
		printk(KERN_ALERT "fail to get gpio descriptor : %d\n", GPIO_NUM);
		return -ENODEV;
	}

	gpiod_direction_input(gpio_desc);
	gpiod_export(gpio_desc, false);

	pps_irq = gpiod_to_irq(gpio_desc);
	if (pps_irq < 0) {
		printk(KERN_ALERT "fail to get gpio interrupti num\n");
		return pps_irq;
	}

	int result = request_irq(pps_irq, (irq_handler_t)pps_timer_handler, IRQF_TRIGGER, "pps_gpio_irq_handle", NULL);
	if (result) {
		printk(KERN_ALERT "fail to request gpio interrupt\n");
		return result;
	}

	printk(KERN_INFO "pps gpio interrupt num : %d\n", pps_irq);

	return 0;
}

static void __exit pps_exit(void)
{
	free_irq(pps_irq, NULL);
	gpiod_unexport(gpio_desc);
	device_destroy(pps_class, MKDEV(pps_major, 0));
	class_unregister(pps_class);
	class_destroy(pps_class);
	unregister_chrdev(pps_major, DEVICE_NAME);

	printk(KERN_INFO "unloading pps driver\n");
}

module_init(pps_init);
module_exit(pps_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fan Loong");
MODULE_DESCRIPTION("PPS DRIVER");

