
obj-m :=irq.o
#obj-m :=ps_key.o
#KDIR=/home/sinogram/zynq/linux-xlnx-xilinx-v14.4/
KDIR=/home/kangqi/Project/zynq/kernel/linux-xlnx/

all:
	$(MAKE) -C $(KDIR) ARCH=arm M=$(PWD) modules

.PHONY: clean
clean:
	$(MAKE) -C $(KDIR) ARCH=arm M=$(PWD) clean 
