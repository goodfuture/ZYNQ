//#include <linux/kernel.h>
//#include <linux/init.h>
//#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <asm/uaccess.h>
#include <asm/sizes.h>
#include <asm/io.h>
#include <linux/of.h>
#include <linux/module.h>   // Needed by all modules
#include <linux/kernel.h>   // Needed for KERN_ALERT
#include <linux/init.h>     // Needed for the macros
#include <linux/interrupt.h> //Needed for the interrupt

#define MODULE_NAME "counter_irq" /* Dev name as it appears in /proc/devices   */

int counter_irq_major = 0;
static volatile int irqHappend = 0;
static int devidR = 1;
static DECLARE_WAIT_QUEUE_HEAD(pps_waitq);

irqreturn_t IRQ(int irq, void *dev_id, struct pt_regs *regs) //interrupt handler
{
	printk(KERN_EMERG "R\n");
	irqHappend = 1; 
	wake_up_interruptible(&pps_waitq); 
	return IRQ_RETVAL(IRQ_HANDLED);
}

static int counter_irq_open(struct inode *inode, struct file *filp)
{
	int result = request_irq(91, IRQ, IRQF_TRIGGER_RISING ,MODULE_NAME,(void *)&devidR); // 挂中断，PL过来的，91是xps生成的中断号，91最大，然后是90,89，。。。
	if (result) 
	{
		printk(KERN_EMERG "IRQ_module: can't get assigned irq %i\n", 91);
		return -1;
	}

	return 0;
}

static int counter_irq_close(struct inode *inode,struct file *file)
{
	free_irq(91, (void *)&devidR);

	return 0;
}

static int counter_irq_read(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
	//unsigned long err;
	//int IsOk = 0;
	wait_event_interruptible(pps_waitq, irqHappend);
	irqHappend = 0;
	//IsOk = 1;
	//err = copy_to_user(buff,(const void *)&IsOk, 4);
	//IsOk = 0;
	
	return 1;
}

struct file_operations counter_irq_fops = {
	.owner = THIS_MODULE,
	.open = counter_irq_open,
	.read = counter_irq_read,
	.release = counter_irq_close
};

static int __init counter_irq_init(void)
{
	printk(KERN_EMERG "IRQ_module initialized");

	counter_irq_major = register_chrdev(counter_irq_major, MODULE_NAME, &counter_irq_fops);
	printk(KERN_EMERG "counter_irq major: %d\n", counter_irq_major);
	if (counter_irq_major < 0)
	{
		printk(KERN_EMERG"counter_irq: can't get major %d\n", counter_irq_major);
		return -1;
	}

	return 0;
}

static void __exit counter_irq_exit(void)
{
	printk(KERN_EMERG "Exit counter_irq module.\n" );

	unregister_chrdev(counter_irq_major, MODULE_NAME);
	
}

module_init(counter_irq_init);
module_exit(counter_irq_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Xilinx Counter IRQ drivier");
MODULE_AUTHOR("ZWM,Inc.");
MODULE_VERSION("1.00a");
