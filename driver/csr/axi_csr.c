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

#define MODULE_NAME			"axi_csr"
#define AXI_CSR_MINOR		0
#define AXI_CSR_PHY_ADDR	0x67200000
#define AXI_CSR_PHY_SIZE	14			// 32bits

int axi_csr_major = 0;
module_param(axi_csr_major, int, 0);

struct axi_csr_dev {
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
int axi_csr_open(struct inode *inode, struct file *filp)
{
	struct axi_csr_dev *dev;
    int retval;
 
    retval = 0;

		printk(KERN_EMERG "******1*******\n");
    dev = container_of(inode->i_cdev, struct axi_csr_dev, cdev);
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

	

	dev->dev_addrsize = AXI_CSR_PHY_SIZE;
	dev->dev_physaddr = AXI_CSR_PHY_ADDR;
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

int axi_csr_release(struct inode *inode, struct file *filp)
{
	struct axi_csr_dev *dev = filp->private_data;

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

	printk(KERN_EMERG "****** Close the axi_csr\n");
	//mutex_unlock(&dev->mutex);

	return 0;
}

int axi_csr_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	return 0;
}

ssize_t axi_csr_read(struct file *filp, char *buf, size_t len, loff_t *off)
{
	struct axi_csr_dev *dev = filp->private_data;
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

ssize_t axi_csr_write(struct file *filp, const char *buf, size_t len, loff_t *off)
{
	struct axi_csr_dev *dev = filp->private_data;
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

loff_t axi_csr_lseek(struct file *filp, loff_t off, int whence)
{
	struct axi_csr_dev *dev = filp->private_data;
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


struct file_operations axi_csr_fops = {
	.owner = THIS_MODULE,
	.open = axi_csr_open,
	.read = axi_csr_read,
	.write = axi_csr_write,
	.llseek = axi_csr_lseek,
	.release = axi_csr_release
};

static int __init axi_csr_int(void)
{
	int ret;

	printk(KERN_EMERG "Init axi_csr module.\n");

	axi_csr_major = register_chrdev(axi_csr_major, MODULE_NAME, &axi_csr_fops);
	printk(KERN_EMERG "axi_csr major: %d\n", axi_csr_major);
	if (axi_csr_major < 0)
	{
		printk(KERN_EMERG"axi_csr: can't get major %d\n", axi_csr_major);
		return ret;
	}
	
	return 0;
}

static void __exit axi_csr_exit(void)
{
	printk(KERN_EMERG"Exit axi_csr module.\n" );
	
	unregister_chrdev(axi_csr_major, MODULE_NAME);
}

module_init(axi_csr_int);
module_exit(axi_csr_exit);


MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Xilinx AXI CSR drivier");
MODULE_AUTHOR("ZWM,Inc.");
MODULE_VERSION("1.00a");
