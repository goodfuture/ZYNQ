#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
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

#define MODULE_NAME			"bram"
#define BRAM_MINOR		0
#define BRAM_PHY_ADDR	0x80030000
#define BRAM_PHY_SIZE	80			// 32bits

int bram_major = 0;
module_param(bram_major, int, 0);

struct bram_dev {
	dev_t devno;
	struct mutex mutex;
	struct cdev cdev;
	struct platform_device *pdev;

	/* Hardware device constants */
	u32 dev_physaddr;
	void *dev_virtaddr;
	u32 dev_addrsize;

	/* Driver reference counts */
	u32 writers;

	/* Drivers statistics */
	u32 bytes_written;
	u32 writes;
	u32 opens;
	u32 closes;
	u32 errors;

};


/* File Operation  */
int bram_open(struct inode *inode, struct file *filp)
{
	struct bram_dev *dev;
    int retval;
 
    retval = 0;

		printk(KERN_EMERG "******1*******\n");
    dev = container_of(inode->i_cdev, struct bram_dev, cdev);
		printk(KERN_EMERG "******2*******\n");
    filp->private_data = dev;       /* For use elsewhere */
 
    //if (mutex_lock_interruptible(&dev->mutex)) {
    //        return -ERESTARTSYS;
    //}
 
	//	printk(KERN_EMERG "******3*******\n");
    /* We're only going to allow one write at a time, so manage that via
     * reference counts
    */
    //switch (filp->f_flags & O_ACCMODE) {
    //case O_RDONLY:
    //        break;
    //case O_WRONLY:
    //        if (dev->writers) {
    //                retval = -EBUSY;
    //                goto out;
    //        }
    //        else {
    //                dev->writers++;
    //        }
    //        break;
    //case O_RDWR:
    //default:
    //        if (dev->writers) {
    //                retval = -EBUSY;
    //                goto out;
    //        }
    //        else {
    //                dev->writers++;
    //        }
    //}
 
    dev->opens++;

	

	dev->dev_addrsize = BRAM_PHY_SIZE;
	dev->dev_physaddr = BRAM_PHY_ADDR;
	if(!request_mem_region(dev->dev_physaddr, dev->dev_addrsize*4, MODULE_NAME))
	{
		printk(KERN_EMERG "request_mem_region failed!!!!\n");
	}
		printk(KERN_EMERG "******4*******\n");
	dev->dev_virtaddr = ioremap_nocache(dev->dev_physaddr, dev->dev_addrsize*4);
		printk(KERN_EMERG "****** 5 virt:0x%X; phy:0x%X\n",(unsigned int)dev->dev_virtaddr, (unsigned int)dev->dev_physaddr);
 
//out:
    //    mutex_unlock(&dev->mutex);
        return retval;
}

int bram_release(struct inode *inode, struct file *filp)
{
	struct bram_dev *dev = filp->private_data;

	//if (mutex_lock_interruptible(&dev->mutex)) {
	//	return -EINTR;
	//}

	/* Manage writes via reference counts */
	//switch (filp->f_flags & O_ACCMODE) {
	//case O_RDONLY:
	//	break;
	//case O_WRONLY:
	//	dev->writers--;
	//	break;
	//case O_RDWR:
	//default:
	//	dev->writers--;
	//}

	//dev->closes++;

	iounmap(dev->dev_virtaddr);

	release_mem_region(dev->dev_physaddr, dev->dev_addrsize*4);

	printk(KERN_EMERG "****** Close the bram\n");
	//mutex_unlock(&dev->mutex);

	return 0;
}

int bram_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	return 0;
}

ssize_t bram_read(struct file *filp, char *buf, size_t len, loff_t *off)
{
	struct bram_dev *dev = filp->private_data;
	unsigned long f_pos = filp->f_pos;
	unsigned int *pbuf = (unsigned int *)buf;
	const volatile void *addr = (const volatile void *)((unsigned int)dev->dev_virtaddr + sizeof(int)*f_pos);

	if (f_pos >= dev->dev_addrsize)
	{
		return 0;
	}
	
	//printk(KERN_EMERG "****** ioread32 Addr:0x%X\n", (unsigned int)addr);
	*pbuf = ioread32(addr);

	return sizeof(int);
}

ssize_t bram_write(struct file *filp, const char *buf, size_t len, loff_t *off)
{
	struct bram_dev *dev = filp->private_data;
	unsigned long f_pos = filp->f_pos;
	unsigned int uiValue = *((unsigned int *)buf);
	volatile void *addr = (volatile void *)((unsigned int)dev->dev_virtaddr + sizeof(int)*f_pos);

	if (f_pos >= dev->dev_addrsize)
	{
		return 0;
	}

	//printk(KERN_EMERG "****** iowrite32 Value:0x%X; Addr:0x%X\n",uiValue, addr);

	iowrite32(uiValue, addr);

	return sizeof(int);
}

loff_t bram_lseek(struct file *filp, loff_t off, int whence)
{
	struct bram_dev *dev = filp->private_data;
	loff_t newpos;

	switch(whence)
	{
	case 0:		/* SEEK_SET */
		newpos = off;
		break;

	case 1:		/* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;

	case 2:		/* SEEK_END */
		newpos = dev->dev_addrsize - off;
		break;

	default:
		return -EINVAL;
	}

	if (newpos < 0)
	{
		return -EINVAL;
	}
	
	filp->f_pos = newpos;

	return newpos;
}


struct file_operations bram_fops = {
	.owner = THIS_MODULE,
	.open = bram_open,
	.read = bram_read,
	.write = bram_write,
	.llseek = bram_lseek,
	.release = bram_release
};

static int __init bram_int(void)
{
	int ret;

	printk(KERN_EMERG "Init bram module.\n");

	bram_major = register_chrdev(bram_major, MODULE_NAME, &bram_fops);
	printk(KERN_EMERG "bram major: %d\n", bram_major);
	if (bram_major < 0)
	{
		printk(KERN_EMERG"bram: can't get major %d\n", bram_major);
		return ret;
	}
	
	return 0;
}

static void __exit bram_exit(void)
{
	printk(KERN_EMERG"Exit bram module.\n" );
	
	unregister_chrdev(bram_major, MODULE_NAME);
}

module_init(bram_int);
module_exit(bram_exit);


MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Xilinx AXI CSR drivier");
MODULE_AUTHOR("ZWM,Inc.");
MODULE_VERSION("1.00a");
