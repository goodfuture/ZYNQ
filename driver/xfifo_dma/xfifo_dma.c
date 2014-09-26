/*
 * Driver for Linux DMA test application (FIFO)
 *
 * Copyright (C) 2012 Xilinx, Inc.
 * Copyright (C) 2012 Robert Armstrong
 *
 * Author: Robert Armstrong <robert.armstrong-jr@xilinx.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
 
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
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <asm/uaccess.h>
#include <asm/sizes.h>
#include <asm/dma.h>
#include <asm/io.h>
#include <mach/pl330.h>
#include <linux/of.h>
 
/* Define debugging for use during our driver bringup */
//#undef PDEBUG_S
#define PDEBUG_S(fmt, args...)	//printk(KERN_INFO fmt, ##args)
 
/* Offsets for control registers in the AXI MM2S FIFO */
#define AXI_TXFIFO_STS          0x0
#define AXI_TXFIFO_RST          0x08
#define AXI_TXFIFO_VAC          0x0c
#define AXI_TXFIFO              0x10
#define AXI_TXFIFO_LEN          0x14
 
#define TXFIFO_STS_CLR          0xffffffff
#define TXFIFO_RST              0x000000a5
 
#define MODULE_NAME             "xfifo_dma"
#define XFIFO_DMA_MINOR         0
 
int xfifo_dma_major = 60;
module_param(xfifo_dma_major, int, 0);
 
dma_addr_t write_buffer;
 
DECLARE_WAIT_QUEUE_HEAD(xfifo_dma_wait);
 
struct xfifo_dma_dev {
        dev_t devno;
        struct mutex mutex;
        struct cdev cdev;
        struct platform_device *pdev;
 
        struct pl330_client_data *client_data;
 
        u32 dma_channel;
        u32 fifo_depth;
        u32 burst_length;
 
        /* Current DMA buffer information */
        dma_addr_t buffer_d_addr;
        void *buffer_v_addr;
        size_t count;
        int busy;
 
        /* Hardware device constants */
        u32 dev_physaddr;
        void *dev_virtaddr;
        u32 dev_addrsize;
 
        /* Driver reference counts */
        u32 writers;
 
        /* Driver statistics */
        u32 bytes_written;
        u32 writes;
        u32 opens;
        u32 closes;
        u32 errors;
};
 
struct xfifo_dma_dev *xfifo_dma_dev;
 
static void xfifo_dma_reset_fifo(void)
{
        iowrite32(TXFIFO_STS_CLR, xfifo_dma_dev->dev_virtaddr + AXI_TXFIFO_STS);
        iowrite32(TXFIFO_RST, xfifo_dma_dev->dev_virtaddr + AXI_TXFIFO_RST);
}
 
/* File operations */
int xfifo_dma_open(struct inode *inode, struct file *filp)
{
        struct xfifo_dma_dev *dev;
        int retval;
 
        retval = 0;
        dev = container_of(inode->i_cdev, struct xfifo_dma_dev, cdev);
        filp->private_data = dev;       /* For use elsewhere */
 
        if (mutex_lock_interruptible(&dev->mutex)) {
                return -ERESTARTSYS;
        }
 
        /* We're only going to allow one write at a time, so manage that via
         * reference counts
         */
        switch (filp->f_flags & O_ACCMODE) {
        case O_RDONLY:
                break;
        case O_WRONLY:
                if (dev->writers || dev->busy) {
                        retval = -EBUSY;
                        goto out;
                }
                else {
                        dev->writers++;
                }
                break;
        case O_RDWR:
        default:
                if (dev->writers || dev->busy) {
                        retval = -EBUSY;
                        goto out;
                }
                else {
                        dev->writers++;
                }
        }
 
        dev->opens++;
 
out:
        mutex_unlock(&dev->mutex);
        return retval;
}
 
int xfifo_dma_release(struct inode *inode, struct file *filp)
{
        struct xfifo_dma_dev *dev = filp->private_data;
 
        if (mutex_lock_interruptible(&dev->mutex)) {
                return -EINTR;
        }
 
        /* Manage writes via reference counts */
        switch (filp->f_flags & O_ACCMODE) {
        case O_RDONLY:
                break;
        case O_WRONLY:
                dev->writers--;
                break;
        case O_RDWR:
        default:
                dev->writers--;
        }
 
        dev->closes++;
 
        mutex_unlock(&dev->mutex);
 
        return 0;
}

static void xfifo_dma_fault_callback(unsigned int channel,
	unsigned int fault_type,
	unsigned int fault_address,
	void *data)
{
	struct xfifo_dma_dev *dev = data;

	dev_err(&dev->pdev->dev,
		"DMA fault type %d at address 0x%0x on channel %d\n",
		fault_type, fault_address, channel);

	dev->errors++;
	//xfifo_dma_reset_fifo();
	dev->busy = 0;
	wake_up_interruptible(&xfifo_dma_wait);
} 

static void xfifo_dma_done_callback(unsigned int channel, void *data)
{
	struct xfifo_dma_dev *dev = data;

	dev->bytes_written += dev->count;
	dev->busy = 0;

	/* Write the count to the FIFO control register */
	//iowrite32(dev->count, xfifo_dma_dev->dev_virtaddr + AXI_TXFIFO_LEN);

	wake_up_interruptible(&xfifo_dma_wait);
}

ssize_t xfifo_dma_read(struct file *filp, char __user *buf, size_t count,
        loff_t *f_pos)
{
	struct xfifo_dma_dev *dev = filp->private_data;
	int fpos = filp->f_pos;
	size_t transfer_size;

	int retval = 0;

	if (mutex_lock_interruptible(&dev->mutex)) {
		return -EINTR;
	}

	//dev->reads++;

	transfer_size = count;
	if (count > dev->fifo_depth) {
		transfer_size = dev->fifo_depth;
	}

	//printk(KERN_INFO "fpos: %d.\n", fpos);
	//dev->buffer_d_addr = dev->buffer_d_addr + fpos;

	/* Allocate a DMA buffer for the transfer */
	dev->buffer_v_addr = dma_alloc_coherent(&dev->pdev->dev, transfer_size,
		&dev->buffer_d_addr, GFP_KERNEL);
	if (!dev->buffer_v_addr) {
		dev_err(&dev->pdev->dev,
			"coherent DMA buffer allocation failed\n");
		retval = -ENOMEM;
		goto fail_buffer;
	}

	PDEBUG_S("dma buffer alloc - d @0x%0x v @0x%0x\n",
		(u32)dev->buffer_d_addr, (u32)dev->buffer_v_addr);

	if (request_dma(dev->dma_channel, MODULE_NAME)) 
	{
		dev_err(&dev->pdev->dev, "unable to alloc DMA channel %d\n", dev->dma_channel);
		retval = -EBUSY;
		goto fail_client_data;
	}
	PDEBUG_S("**1.\n");
	dev->busy = 1;
	dev->count = transfer_size;

	set_dma_mode(dev->dma_channel, DMA_MODE_READ);
	PDEBUG_S("**2.\n");
	set_dma_addr(dev->dma_channel, dev->buffer_d_addr);
	PDEBUG_S("**3.\n");
	set_dma_count(dev->dma_channel, transfer_size);
	PDEBUG_S("**4.\n");
	dev->client_data->dev_addr = xfifo_dma_dev->dev_physaddr + fpos;
	set_pl330_client_data(dev->dma_channel, dev->client_data);
	PDEBUG_S("**5.\n");

	set_pl330_done_callback(dev->dma_channel,
		xfifo_dma_done_callback, dev);
	PDEBUG_S("**6.\n");
	set_pl330_fault_callback(dev->dma_channel,
		xfifo_dma_fault_callback, dev);
	set_pl330_incr_dev_addr(dev->dma_channel, 1);
	PDEBUG_S("**7.\n");

	//xfifo_dma_reset_fifo();
	//PDEBUG_S("**9.\n");
	/* Kick off the DMA */
	enable_dma(dev->dma_channel);
	PDEBUG_S("**10.\n");
	mutex_unlock(&dev->mutex);
	PDEBUG_S("**11.\n");
	wait_event_interruptible(xfifo_dma_wait, dev->busy == 0);
	PDEBUG_S("**12.\n");

	/* Load our DMA buffer with the user data */
	copy_to_user(buf, dev->buffer_v_addr, transfer_size);
	PDEBUG_S("**8.\n");

	/* Deallocate the DMA buffer and free the channel */
	free_dma(dev->dma_channel);
	PDEBUG_S("**13.\n");
	dma_free_coherent(&dev->pdev->dev, dev->count, dev->buffer_v_addr,
		dev->buffer_d_addr);
	PDEBUG_S("**14.\n");
	PDEBUG_S("dma read %d bytes\n", transfer_size);

	return transfer_size;

fail_client_data:
	PDEBUG_S("**15.\n");
	dma_free_coherent(&dev->pdev->dev, transfer_size, dev->buffer_v_addr,
		dev->buffer_d_addr);
	PDEBUG_S("**16.\n");
fail_buffer:
	PDEBUG_S("**17.\n");
	mutex_unlock(&dev->mutex);
	PDEBUG_S("**18.\n");
	return retval;
}

loff_t xfifo_dma_lseek(struct file *filp, loff_t off, int whence)
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

	//case 2:		/* SEEK_END */
	//	newpos = dev->dev_addrsize - off;
	//	break;

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
 
ssize_t xfifo_dma_write(struct file *filp, const char __user *buf, size_t count,
        loff_t *f_pos)
{
        struct xfifo_dma_dev *dev = filp->private_data;
        size_t transfer_size;
		int fpos = filp->f_pos;
        int retval = 0;
 
        if (mutex_lock_interruptible(&dev->mutex)) {
                return -EINTR;
        }
 
        dev->writes++;
 
        transfer_size = count;
        if (count > dev->fifo_depth) {
                transfer_size = dev->fifo_depth;
        }
 
        /* Allocate a DMA buffer for the transfer */
        dev->buffer_v_addr = dma_alloc_coherent(&dev->pdev->dev, transfer_size,
                &dev->buffer_d_addr, GFP_KERNEL);
        if (!dev->buffer_v_addr) {
                dev_err(&dev->pdev->dev,
                        "coherent DMA buffer allocation failed\n");
                retval = -ENOMEM;
                goto fail_buffer;
        }
 
        PDEBUG_S("dma buffer alloc - d @0x%0x v @0x%0x\n",
                (u32)dev->buffer_d_addr, (u32)dev->buffer_v_addr);
 
        if (request_dma(dev->dma_channel, MODULE_NAME)) {
                dev_err(&dev->pdev->dev,
                        "unable to alloc DMA channel %d\n",
                        dev->dma_channel);
                retval = -EBUSY;
                goto fail_client_data;
        }
 
        dev->busy = 1;
        dev->count = transfer_size;
 
        set_dma_mode(dev->dma_channel, DMA_MODE_WRITE);
        set_dma_addr(dev->dma_channel, dev->buffer_d_addr);
        set_dma_count(dev->dma_channel, transfer_size);
		dev->client_data->dev_addr = xfifo_dma_dev->dev_physaddr + fpos;
		PDEBUG_S("Phy Addr is 0x%X.\n", dev->client_data->dev_addr);
        set_pl330_client_data(dev->dma_channel, dev->client_data);

        set_pl330_done_callback(dev->dma_channel,
                xfifo_dma_done_callback, dev);
        set_pl330_fault_callback(dev->dma_channel,
                xfifo_dma_fault_callback, dev);
        set_pl330_incr_dev_addr(dev->dma_channel, 0);
 
        /* Load our DMA buffer with the user data */
        copy_from_user(dev->buffer_v_addr, buf, transfer_size);
 
        xfifo_dma_reset_fifo();
        /* Kick off the DMA */
        enable_dma(dev->dma_channel);
 
        mutex_unlock(&dev->mutex);
 
        wait_event_interruptible(xfifo_dma_wait, dev->busy == 0);
 
        /* Deallocate the DMA buffer and free the channel */
        free_dma(dev->dma_channel);
 
        dma_free_coherent(&dev->pdev->dev, dev->count, dev->buffer_v_addr,
                dev->buffer_d_addr);
 
        PDEBUG_S("dma write %d bytes\n", transfer_size);
 
        return transfer_size;
 
fail_client_data:
        dma_free_coherent(&dev->pdev->dev, transfer_size, dev->buffer_v_addr,
                dev->buffer_d_addr);
fail_buffer:
        mutex_unlock(&dev->mutex);
        return retval;
}
 
struct file_operations xfifo_dma_fops = {
        .owner = THIS_MODULE,
        .read = xfifo_dma_read,
        .write = xfifo_dma_write,
        .open = xfifo_dma_open,
		.llseek = xfifo_dma_lseek,
        .release = xfifo_dma_release
};
 
/* Driver /proc filesystem operations so that we can show some statistics */
static void *xfifo_dma_proc_seq_start(struct seq_file *s, loff_t *pos)
{
        if (*pos == 0) {
                return xfifo_dma_dev;
        }
 
        return NULL;
}
 
static void *xfifo_dma_proc_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
        (*pos)++;
        return NULL;
}
 
static void xfifo_dma_proc_seq_stop(struct seq_file *s, void *v)
{
}
 
static int xfifo_dma_proc_seq_show(struct seq_file *s, void *v)
{
        struct xfifo_dma_dev *dev;
 
        dev = v;
        if (mutex_lock_interruptible(&dev->mutex)) {
                return -EINTR;
        }
 
        seq_printf(s, "\nFIFO DMA Test:\n\n");
        seq_printf(s, "Device Physical Address: 0x%0x\n", dev->dev_physaddr);
        seq_printf(s, "Device Virtual Address:  0x%0x\n",
                (u32)dev->dev_virtaddr);
        seq_printf(s, "Device Address Space:    %d bytes\n", dev->dev_addrsize);
        seq_printf(s, "DMA Channel:             %d\n", dev->dma_channel);
        seq_printf(s, "FIFO Depth:              %d bytes\n", dev->fifo_depth);
        seq_printf(s, "Burst Length:            %d words\n", dev->burst_length);
        seq_printf(s, "\n");
        seq_printf(s, "Opens:                   %d\n", dev->opens);
        seq_printf(s, "Writes:                  %d\n", dev->writes);
        seq_printf(s, "Bytes Written:           %d\n", dev->bytes_written);
        seq_printf(s, "Closes:                  %d\n", dev->closes);
        seq_printf(s, "Errors:                  %d\n", dev->errors);
        seq_printf(s, "Busy:                    %d\n", dev->busy);
        seq_printf(s, "\n");
 
        mutex_unlock(&dev->mutex);
        return 0;
}
 
/* SEQ operations for /proc */
static struct seq_operations xfifo_dma_proc_seq_ops = {
        .start = xfifo_dma_proc_seq_start,
        .next = xfifo_dma_proc_seq_next,
        .stop = xfifo_dma_proc_seq_stop,
        .show = xfifo_dma_proc_seq_show
};
 
static int xfifo_dma_proc_open(struct inode *inode, struct file *file)
{
        return seq_open(file, &xfifo_dma_proc_seq_ops);
}
 
static struct file_operations xfifo_dma_proc_ops = {
        .owner = THIS_MODULE,
        .open = xfifo_dma_proc_open,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = seq_release
};
 
static int xfifo_dma_remove(struct platform_device *pdev)
{
        cdev_del(&xfifo_dma_dev->cdev);
 
        remove_proc_entry("driver/xfifo_dma", NULL);
 
        unregister_chrdev_region(xfifo_dma_dev->devno, 1);
 
        /* Unmap the I/O memory */
        if (xfifo_dma_dev->dev_virtaddr) {
                iounmap(xfifo_dma_dev->dev_virtaddr);
                release_mem_region(xfifo_dma_dev->dev_physaddr,
                        xfifo_dma_dev->dev_addrsize);
        }
 
        /* Free the PL330 buffer client data descriptors */
        if (xfifo_dma_dev->client_data) {
                kfree(xfifo_dma_dev->client_data);
        }
 
        if (xfifo_dma_dev) {
                kfree(xfifo_dma_dev);
        }
 
        return 0;
}
 
#ifdef CONFIG_OF
static struct of_device_id xfifodma_of_match[] __devinitdata = {
        { .compatible = "xlnx,fifo-dma", },
        { /* end of table */}
};
MODULE_DEVICE_TABLE(of, xfifodma_of_match);
#else
#define xfifodma_of_match NULL
#endif /* CONFIG_OF */
 
static int xfifo_dma_probe(struct platform_device *pdev)
{
        int status;
        struct proc_dir_entry *proc_entry;
        struct resource *xfifo_dma_resource;
 
        /* Get our platform device resources */
        PDEBUG_S("We have %d resources\n", pdev->num_resources);
        xfifo_dma_resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        if (xfifo_dma_resource == NULL) {
                dev_err(&pdev->dev, "No resources found\n");
                return -ENODEV;
        }
 
        /* Allocate a private structure to manage this device */
        xfifo_dma_dev = kmalloc(sizeof(struct xfifo_dma_dev), GFP_KERNEL);
        if (xfifo_dma_dev == NULL) {
                dev_err(&pdev->dev,
                        "unable to allocate device structure\n");
                return -ENOMEM;
        }
        memset(xfifo_dma_dev, 0, sizeof(struct xfifo_dma_dev));
 
        /* Get our device properties from the device tree, if they exist */
        if (pdev->dev.of_node) {
                if (of_property_read_u32(pdev->dev.of_node, "dma-channel",
                        &xfifo_dma_dev->dma_channel) < 0) {
                        dev_warn(&pdev->dev,
                                "DMA channel unspecified - assuming 0\n");
                        xfifo_dma_dev->dma_channel = 0;
                }
				//set DMA channel
				xfifo_dma_dev->dma_channel = 0;

                dev_info(&pdev->dev,
                        "read DMA channel is %d\n", xfifo_dma_dev->dma_channel);
                if (of_property_read_u32(pdev->dev.of_node, "fifo-depth",
                        &xfifo_dma_dev->fifo_depth) < 0) {
                        dev_warn(&pdev->dev,
                                "depth unspecified, assuming 0xffffffff\n");
                        xfifo_dma_dev->fifo_depth = 0xffffffff;
                }
				xfifo_dma_dev->fifo_depth = 65536;
                dev_info(&pdev->dev,
                        "DMA fifo depth is %d\n", xfifo_dma_dev->fifo_depth);
                if (of_property_read_u32(pdev->dev.of_node, "burst-length",
                        &xfifo_dma_dev->burst_length) < 0) {
                        dev_warn(&pdev->dev,
                                "burst length unspecified - assuming 1\n");
                        xfifo_dma_dev->burst_length = 1;
                }
				// set burst length
				xfifo_dma_dev->burst_length = 16;
                dev_info(&pdev->dev,
                        "DMA burst length is %d\n",
                        xfifo_dma_dev->burst_length);
        }
 
        xfifo_dma_dev->pdev = pdev;
 
        xfifo_dma_dev->devno = MKDEV(xfifo_dma_major, XFIFO_DMA_MINOR);
        PDEBUG_S("devno is 0x%0x, pdev id is %d\n", xfifo_dma_dev->devno, XFIFO_DMA_MINOR);
 
        status = register_chrdev_region(xfifo_dma_dev->devno, 1, MODULE_NAME);
        if (status < 0) {
                dev_err(&pdev->dev, "unable to register chrdev %d\n",
                        xfifo_dma_major);
                goto fail;
        }
 
        /* Register with the kernel as a character device */
        cdev_init(&xfifo_dma_dev->cdev, &xfifo_dma_fops);
        xfifo_dma_dev->cdev.owner = THIS_MODULE;
        xfifo_dma_dev->cdev.ops = &xfifo_dma_fops;
 
        /* Initialize our device mutex */
        mutex_init(&xfifo_dma_dev->mutex);
 
        xfifo_dma_dev->dev_physaddr = xfifo_dma_resource->start;
        xfifo_dma_dev->dev_addrsize = xfifo_dma_resource->end -
                xfifo_dma_resource->start + 1;
        PDEBUG_S("dev_phyaddr is 0x%0X, dev_addrsize is %d\n", xfifo_dma_dev->dev_physaddr, xfifo_dma_dev->dev_addrsize);
        if (!request_mem_region(xfifo_dma_dev->dev_physaddr,
                xfifo_dma_dev->dev_addrsize, MODULE_NAME)) {
                dev_err(&pdev->dev, "can't reserve i/o memory at 0x%08X\n",
                        xfifo_dma_dev->dev_physaddr);
                status = -ENODEV;
                goto fail;
        }
        xfifo_dma_dev->dev_virtaddr = ioremap(xfifo_dma_dev->dev_physaddr,
                xfifo_dma_dev->dev_addrsize);
        PDEBUG_S("xfifo_dma: mapped 0x%0x to 0x%0x\n", xfifo_dma_dev->dev_physaddr,
                (unsigned int)xfifo_dma_dev->dev_virtaddr);
 
        xfifo_dma_dev->client_data = kmalloc(sizeof(struct pl330_client_data),
                GFP_KERNEL);
        if (!xfifo_dma_dev->client_data) {
                dev_err(&pdev->dev, "can't allocate PL330 client data\n");
                goto fail;
        }
        memset(xfifo_dma_dev->client_data, 0, sizeof(struct pl330_client_data));
 
        xfifo_dma_dev->client_data->dev_addr =
                xfifo_dma_dev->dev_physaddr; // + AXI_TXFIFO;
        xfifo_dma_dev->client_data->dev_bus_des.burst_size = 4;
        xfifo_dma_dev->client_data->dev_bus_des.burst_len =
                xfifo_dma_dev->burst_length;
        xfifo_dma_dev->client_data->mem_bus_des.burst_size = 4;
        xfifo_dma_dev->client_data->mem_bus_des.burst_len =
                xfifo_dma_dev->burst_length;
 
        status = cdev_add(&xfifo_dma_dev->cdev, xfifo_dma_dev->devno, 1);
 
        /* Create statistics entry under /proc */
        proc_entry = create_proc_entry("driver/xfifo_dma", 0, NULL);
        if (proc_entry) {
                proc_entry->proc_fops = &xfifo_dma_proc_ops;
        }
 
        xfifo_dma_reset_fifo();
        dev_info(&pdev->dev, "added Xilinx FIFO DMA successfully\n");
 
        return 0;
 
        fail:
        xfifo_dma_remove(pdev);
        return status;
}
 
static struct platform_driver xfifo_dma_driver = {
        .driver = {
                .name = MODULE_NAME,
                .owner = THIS_MODULE,
                .of_match_table = xfifodma_of_match,
        },
        .probe = xfifo_dma_probe,
        .remove = xfifo_dma_remove,
};
 
static void __exit xfifo_dma_exit(void)
{
        platform_driver_unregister(&xfifo_dma_driver);
}
 
static int __init xfifo_dma_init(void)
{
        int status;
 
        status = platform_driver_register(&xfifo_dma_driver);
 
        return status;
}
 
module_init(xfifo_dma_init);
module_exit(xfifo_dma_exit);
 
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Xilinx FIFO DMA driver");
MODULE_AUTHOR("Xilinx, Inc.");
MODULE_VERSION("1.00a");
