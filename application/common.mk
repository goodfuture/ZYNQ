#########################################################################
# Author: qingsong.cao
# Date  : 2013-08-08
# (C) Copyright 2013 Sinogram Technology(Beijing) Co., Ltd.
#########################################################################

CROSS_COMPILE = arm-xilinx-linux-gnueabi-
AS  = $(CROSS_COMPILE)as
LD	= $(CROSS_COMPILE)ld
CC	= $(CROSS_COMPILE)gcc
CPP	= $(CC) -E
AR	= $(CROSS_COMPILE)ar
NM	= $(CROSS_COMPILE)nm
STRIP	= $(CROSS_COMPILE)strip
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
RANLIB	= $(CROSS_COMPILE)RANLIB


gccincdir = $(shell $(CC) -print-file-name=include)
PLATFORM_LIBS += -L $(shell dirname `$(CC) $(CFLAGS) -print-libgcc-file-name`) -lgcc
#BUILDIN_FLAGS =-fno-builtin-printf -fno-builtin-sprintf -fno-builtin-strcpy -fno-builtin-fputs -fno-builtin-fwrite

#CFLAGS= -g  -O0 $(BUILDIN_FLAGS)-fno-omit-frame-pointer -Wstrict-prototypes -Wundef -Wstrict-prototypes -Wno-trigraphs \
#        -fno-strict-aliasing -fno-common -fno-optimize-sibling-calls -fno-builtin -nostdinc \
#        -fno-builtin -ffreestanding -nostdinc -isystem $(gccincdir) -pipe \
#        -mhard-float  -Wall 

#ASFLAGS= -g  -O0 -D__ASSEMBLY__ -fno-omit-frame-pointer -Wundef -Wstrict-prototypes -Wno-trigraphs \
#         -fno-strict-aliasing -fno-common -fno-optimize-sibling-calls -fno-builtin -nostdinc -meabi \
#         -fno-builtin -ffreestanding -nostdinc -isystem $(gccincdir) -pipe \
#         -mhard-float 



CFLAGS = -g  -O0 -Wall -Wno-unused-variable -Wno-unused-but-set-variable -isystem $(gccincdir) 
#CFLAGS += -g  -O0 $(BUILDIN_FLAGS) -ffreestanding -nostdinc -isystem $(gccincdir) -pipe -mhard-float -Wall

ARFLAGS=crv

ifdef STRICK
CFLAGS = -g  -O0 -Wall -isystem $(gccincdir) 
endif


LDFLAGS= 
IMAGE = mAcquisitonBoard


%.s:    %.S 
	$(CC) $(ASFLAGS) -c -o $@ $<
%.o:	%.S
	$(CC) $(ASFLAGS) -c -o $@ $<
%.o:	%.c
	$(CC) $(CFLAGS) -c -o $@ $<


