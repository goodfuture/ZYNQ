#ifndef _MAIN_H_
#define _MAIN_H_

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <sys/mman.h>
#include <pthread.h>



#include "common.h"
#include "csr.h"
#include "app.h"



int GetCsrFd(void);
int GetBramFd(void);
int GetRamFd(void);
int GetIrqFd(void);
int GetDataIrqFd(void);
//void SetDataIrqFd(int newFd);

#endif
