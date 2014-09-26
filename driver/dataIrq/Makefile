
obj-m :=dataIrq.o
#obj-m :=ps_key.o
#KDIR=/home/qingsong.cao/Zynq/linux-xlnx-xilinx-v14.4/
#KDIR=/home/sinogram/zynq/linux-xlnx-xilinx-v14.4/
#KDIR=/home/sinogram/zynq/linux-xlnx-xilinx-v2014.2/
KDIR=/home/sinogram/zynq/linux-xlnx-master/

all:
	$(MAKE) -C $(KDIR) ARCH=arm M=$(PWD) modules
	cp -f *.ko ../../ 

.PHONY: clean
clean:
	$(MAKE) -C $(KDIR) ARCH=arm M=$(PWD) clean 
