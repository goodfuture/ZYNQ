#include "main.h"
#include <errno.h>


static int s_csrFd = 0;
static int s_iBramFd = 0;
static int s_iRamFd = 0;
static int s_iIrqFd = 0;
static int s_iDataIrqFd = 0;

static pthread_t s_MesgThreadId = 0;
static pthread_t s_GpsThreadId = 0;

int GetCsrFd(void)
{
	return s_csrFd;
}

int GetBramFd(void)
{
	return s_iBramFd;
}

int GetRamFd(void)
{
	return s_iRamFd;
}

int GetIrqFd(void)
{
	return s_iIrqFd;
}

int GetDataIrqFd(void)
{
	return s_iDataIrqFd;
}

//void SetDataIrqFd(int newFd)
//{
//	s_iDataIrqFd = newFd;
//}

int main(void)
{
	uint32_t regValue = 0;
	uint32_t uiReady = 0;
	uint32_t j = 0, i = 0;
	uint32_t uiRet = 0;
	uint32_t uiTaShift = 0;
	uint32_t ii = 0;
	uint32_t uiReadyNo = 0;
	int32_t iRet = 0;

	const char *m_csrDev = "/dev/axi_csr0";

	char *BramAddr0 = NULL;
	char *pBramDev0 = "/dev/xfifo_dma0";
	char *pRamDev0 = "/dev/bram0";
	char *pIrqDev0 = "/dev/counter_irq0";
	char *pDataIrq = "/dev/data_irq0";
	uint32_t uiMapLen = 4096;

	s_csrFd = open(m_csrDev, O_RDWR);
	if (s_csrFd < 0)
	{	
		ErrorInfo("can't open device %s[%d].\n", m_csrDev, s_csrFd);
		return -1;
	}
	InitInfo("csrfd is %d.\n", s_csrFd);

	/*
	   s_iBramFd = open(pBramDev0, O_RDWR);
	   if (s_iBramFd < 0)
	   {	
	   ErrorInfo("can't open device %s[%d].\n", pBramDev0, s_iBramFd);
	   return -1;
	   }
	   InitInfo("BramFd is %d.\n", s_iBramFd);
	 */

	s_iRamFd = open(pRamDev0, O_RDWR);
	if (s_iRamFd < 0)
	{
		ErrorInfo("Can't open device %s[%d].\n", pRamDev0, s_iRamFd);
		return -1;
	}

	s_iIrqFd = open(pIrqDev0, O_RDWR);
	if (s_iIrqFd < 0)
	{
		ErrorInfo("Can't open device %s[%d].\n", pIrqDev0, s_iIrqFd);
		return -1;
	}

	s_iDataIrqFd = open(pDataIrq, O_RDWR);
	if (s_iDataIrqFd < 0)
	{
		ErrorInfo("Can't open device %s[%d].\n", pDataIrq, s_iDataIrqFd);
		printf("error:%s\n",strerror(errno));
		return -1;
	}

	InitInfo("AcquisitionBoard Version 1.0 No 21 build date 2014-8-15.\n");

	//TestSd();
	InitMem();
	InitGps();
	InitNetLink();
	AcceptLink();
	//UdpInitNetLink();
	iRet = pthread_create(&s_MesgThreadId, NULL, MesgThread, NULL);
	if (iRet != 0)
	{
		ErrorInfo("Create Mesg Thread Failed![%d]\n", iRet);
		return -1;
	}

	iRet = pthread_create(&s_GpsThreadId, NULL, GpsThread, NULL);
	if (iRet != 0)
	{
		ErrorInfo("Create GPS Thread Failed![%d]\n", iRet);
		return -1;
	}

	while(1)
	{
		App();

		if (TRUE == GetReadyFlag())
		{			
			//DeliverData();
			DeliverData_s();
			//UdpDeliverData();
		}   

	}

	close(s_iBramFd);
	close(s_csrFd);
	close(s_iRamFd);
	close(s_iIrqFd);

	return 0;
}
