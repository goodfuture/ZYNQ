#include "app.h"
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h> 
#include <sys/types.h> 
#include <sys/stat.h>

static uint8_t s_pTcpBuff[TCP_BUFF_SIZE];
static uint8_t s_pUdpBuff[UDP_BUFF_SIZE];
static uint8_t s_pMesgBuff[MESG_BUFF_SIZE];
static CmdHeader* s_pCmdHead = (CmdHeader*)s_pTcpBuff;
static AckHeader* s_pAckHead = (AckHeader*)s_pTcpBuff;
static UdpHeader* s_pUdpHead = (UdpHeader*)s_pUdpBuff;
static MesgHeader* s_pMesgHead = (MesgHeader*)s_pMesgBuff;

static uint32_t s_uiTcpSendLen = 0;

static uint8_t s_ucIsReady = FALSE;
static int32_t s_iCmdConnSock = -1;
static int32_t s_iCmdAcqSock = -1;
static int32_t s_iDataConnSock = -1;
static int32_t s_iDataAcqSock = - 1;
static int32_t s_iMesgConnSock = -1;
static int32_t s_iMesgAcqSock = -1;
static struct sockaddr_in TcpAddrLocal;
static struct sockaddr_in CmdAddrPeer;
static struct sockaddr_in DataAddrLocal;
static struct sockaddr_in DataAddrPeer;
static struct sockaddr_in MesgAddrLocal;
static struct sockaddr_in MesgAddrPeer;
static const char *s_pCalibrationIni = "Calibration.ini";
static const char *s_pConfigIni = "Configuration.ini";
static const char *s_pConfigIniDir = "/DATA/Configuration.ini";
static int32_t s_iIniFd = -1;
//static char *s_pDataFile = NULL;
static int32_t iDataFd = -1;
static const char *s_pDataFile = "/tmp/acquistion.dat";
static FILE *s_pDataFd = NULL;
static int32_t s_tick = 0;
static unsigned long long s_ulTotal = 0;
static uint32_t s_cnt = 0;
static uint32_t s_pSimuData[10485760];
static uint32_t s_total = 0;
static const char *s_pSdFile = "acquistion.dat";
static FILE *s_pSdFd = NULL;
static unsigned long long s_ulDataSize = 0;
static uint32_t s_puiPrbsBuff[64];
static unsigned long long s_ulSampleLength = 0;
static uint8_t s_ucIntHappend = 0;
static uint8_t s_ucDeliveredDone = 0;
static uint32_t *s_puiAddr_A = NULL;
static uint32_t *s_puiAddr_B = NULL;
static uint32_t *s_puiAddr = NULL;
static uint32_t s_uiFlag = 0;
static unsigned long long  s_ulTotalLen = 0;
static uint32_t s_uiCheck = 1;
static uint32_t s_WorkMode_TX = 1;
static FILE *s_pSdHPFd = NULL;
static char s_SdDataDIR[64]; //64bytes max
static uint32_t s_uiSavePC = 0; //0:sd,1:pc
static uint32_t s_puiConfigBuf[87]; //config data length is 87*4
static char  s_SD_FILE_NAME[32];

static uint8_t s_SampleRateReg[6] = {
		0x1D,	// 78.125KHz
		0x1C,	// 156.25KHz
		0x1B,	// 312.5KHz
		0x1A,	// 625KHz
		0x19,	// 1.25MHz
		0x18	// 2.5MHz
};

static uint32_t s_SampleRateValue[6] = {
	78125,		// 78.125KHz
	156250,		// 156.25KHz
	312500,		// 312.5KHz
	625000,		// 625KHz
	1250000,	// 1.25MHz
	2500000		// 2.5MHz
};

static InterData s_FullRange[] = 
{
	{3000000, 0},	// 3V
	{300000, 1},	// 300mV
	{30000, 2},		// 30mV
	{3000, 3},		// 3mV
	{300, 7},		// 300uV
};

static InterData s_Filter[] =
{
	{0, 1},			// None
	{12, 2},		// 1.2KHz
	{120, 0},		// 12KHz
	{1200, 3},		// 120KHz
};

static InterData s_InputSouce[] = 
{
	{1, 0},			// Signal
	{2, 1},			// GND
	{3, 3},			// DAC
};

static InterData s_SyncMode[] =
{
	{0, 2},			// GPS
	{1, 1},			// OUT
};

static InterData s_WorkMode[] =
{
	{1, 1},			// Calibration
	{2, 0},			// Work
};

static CommandData s_CmdHandler[] =
{
	{ACQ_START_CMD, AcqStart}, 
	{ACQ_STOP_CMD, AcqStop},

};

int32_t SetDataToRam(uint32_t uiSeed, uint32_t uiValue, uint32_t n)
{
	int32_t iRet = 0;
	int32_t ramFd = GetRamFd();
	int32_t i = 0, total = 0;
	GetLfsrUint(uiSeed, uiValue, s_puiPrbsBuff, n);
	printf("Seed:%d; User:%d; n:%d.\n", uiSeed, uiValue, n);
	total = Power(n);
	for (i=0; i<total/32; i++)
	{
		lseek(ramFd, i, SEEK_SET);
		iRet = write(ramFd, (const char *)&s_puiPrbsBuff[i], 4);
		//printf("0x%08X\n", s_puiPrbsBuff[i]);
		if (iRet < 0)
		{
			ErrorInfo("Write RAM failed![%d].\n", iRet);
		}
	}

	return iRet;
}

void *MesgThread(void *args)
{
	int iIrqFd = GetIrqFd();
	struct timeval bb;
	uint32_t uiSeconds = 0;
	int iRet = 0;
	int status = 0;
	int DataSize = sizeof(struct gps_data_t);
	struct gps_data_t *pGpsData = NULL;

	//char  SD_FILE_NAME[32];  
	//char  SD_DIR_NAME[11] = "/mnt/DATA/";  
	//char  SDDIR_FILE_NAME[32];  
	uint32_t local_hour = 0;
	uint32_t local_day = 0;
	//char *pDataIrq2 = "/dev/data_irq0";

	printf("inside %s\n", __FUNCTION__);
	while(1)
	{
		//printf("Wait Irq\n");
		iRet = read(iIrqFd, (char *)&status, 4);
		//printf("1.\n");
		if (1 == iRet)
		{
			s_ucIntHappend = 1;
			s_ucIsReady = TRUE;		


			s_uiFlag = 0;
			s_ulTotalLen = 0;
			s_uiCheck = 1;

			
			pGpsData = GetGpsInfo();

			if(s_uiSavePC == 0)  //if save sd selected, create the dir and file
			{
				if(s_pSdHPFd != NULL)
				{
					fclose(s_pSdHPFd);
					s_pSdHPFd = NULL;
				}
				
				gettimeofday(&bb, NULL); 
				uiSeconds = bb.tv_sec;
				
				sprintf(s_SD_FILE_NAME,"/mnt/%s/%d_tx.dat", s_SdDataDIR, uiSeconds);
				printf("%s.\n", s_SD_FILE_NAME);
				
				s_pSdHPFd = fopen(s_SD_FILE_NAME, "wb");
				if (s_pSdHPFd ==NULL)
				{
					printf("Open file %s failed.\n", s_SD_FILE_NAME);
				}

				fwrite(s_puiConfigBuf, 1, sizeof(s_puiConfigBuf), s_pSdHPFd);
			}
			

			s_pMesgHead->usMesg = GpsTimeMesg;
			s_pMesgHead->usLength = DataSize;
			memcpy(s_pMesgBuff+MESGHEADER_SIZE, pGpsData, DataSize);
			//printf("GpsSize: %d %d.\n", MESGHEADER_SIZE, DataSize);
			iRet = send(s_iMesgAcqSock, s_pMesgBuff, MESGHEADER_SIZE+DataSize, 0);
			if (iRet < 0)
			{
				printf("Send GPS Info Failed![%d]\n", iRet);
			}
		}
	}

	return NULL;
}

int GetRealData(InterData pInterData[], int iPcData, int len)
{
	uint16_t i = 0;
	uint16_t usTotal = len/sizeof(InterData);
	int iArmData = 0;

	for (i=0; i<usTotal; i++)
	{
		if (iPcData == pInterData[i].iPcData)
		{
			iArmData = pInterData[i].iArmData;
			break;
		}
	}

	return iArmData;
	
}

CmdHandler GetCmdHandler(uint16_t usCmd)
{
	CmdHandler HandlerPtr = NULL;
	uint16_t i = 0;
	uint16_t CmdNum = sizeof(s_CmdHandler)/sizeof(CommandData);

	for (i=0; i<CmdNum; i++)
	{
		if (usCmd == s_CmdHandler[i].usCmd)
		{
			HandlerPtr = s_CmdHandler[i].Handler;
			break;
		}
	}

	if (i == CmdNum)
	{
		HandlerPtr = NULL;
		ErrorInfo("No Command Handler Found!\n");
	}

	return HandlerPtr;
}

int32_t UdpInitNetLink(void)
{
	int32_t iOpt = 0;
	int32_t iRet = 0;

	// Create Data Socket
	s_iDataConnSock = socket(AF_INET, SOCK_DGRAM, 0);
	if (s_iDataConnSock < 0)
	{
		ErrorInfo("Create socket error![%d]\n", s_iDataConnSock);
		return RET_FAIL;
	}

	iOpt = 1;
	ioctl(s_iDataConnSock, FIONBIO, &iOpt);

	DataAddrLocal.sin_family = AF_INET;
	DataAddrLocal.sin_port = htons(LOCAL_DATA_PORT);
	DataAddrLocal.sin_addr.s_addr = htonl(INADDR_ANY);

	DataAddrPeer.sin_family = AF_INET;
	DataAddrPeer.sin_port = htons(REMOTE_UDP_PORT);
	DataAddrPeer.sin_addr.s_addr = inet_addr(REMOTE_IP);

	iRet = bind(s_iDataConnSock, (struct sockaddr *)&DataAddrLocal, sizeof(struct sockaddr));
	if (iRet < 0)
	{
		ErrorInfo("bind error\n");
		return RET_FAIL;
	}

	///* Create TCP Socket */
	s_iCmdConnSock = socket(AF_INET, SOCK_STREAM, 0);
	if (s_iCmdConnSock < 0)
	{
		ErrorInfo("Create TCP Socket Error!\n");
		return RET_FAIL;
	}

	iOpt = 0;
	ioctl(s_iCmdConnSock, FIONBIO, &iOpt);

	TcpAddrLocal.sin_family = AF_INET;
	TcpAddrLocal.sin_port = htons(LOCAL_CMD_PORT);
	TcpAddrLocal.sin_addr.s_addr = htonl(INADDR_ANY);

	iRet = bind(s_iCmdConnSock, (struct sockaddr *)&TcpAddrLocal, sizeof(struct sockaddr));
	if (iRet < 0)
	{
		ErrorInfo("bind error\n");
		return RET_FAIL;
	}

	iRet = listen(s_iCmdConnSock, 5);
	if (iRet < 0)
	{
		ErrorInfo("listen error\n");
		close(s_iCmdConnSock);
		return RET_FAIL;
	}

	iOpt = sizeof(struct sockaddr_in);
	s_iCmdAcqSock = accept(s_iCmdConnSock, (struct sockaddr *)&CmdAddrPeer, (socklen_t *)&iOpt);
	if (s_iCmdAcqSock > 0 )
	{
		iOpt = 1;
		ioctl(s_iCmdAcqSock, FIONBIO, &iOpt);
		iOpt = 65536;
		setsockopt(s_iCmdAcqSock, SOL_SOCKET, SO_SNDBUF, (const char *)&iOpt, sizeof(int));

	}

	return RET_OK;
}

int32_t InitNetLink(void)
{
	int32_t iOpt = 0;
	int32_t iRet = 0;

	// Create Data Socket
	s_iDataConnSock = socket(PF_INET, SOCK_STREAM, 0);
	if (s_iDataConnSock < 0)
	{
		ErrorInfo("Create socket error![%d]\n", s_iDataConnSock);
		return RET_FAIL;
	}

	iOpt = 0;
	ioctl(s_iDataConnSock, FIONBIO, &iOpt);

	DataAddrLocal.sin_family = PF_INET;
	DataAddrLocal.sin_port = htons(LOCAL_DATA_PORT);
	DataAddrLocal.sin_addr.s_addr = htonl(INADDR_ANY);

	//DataAddrPeer.sin_family = AF_INET;
	//DataAddrPeer.sin_port = htons(REMOTE_UDP_PORT);
	//DataAddrPeer.sin_addr.s_addr = inet_addr(REMOTE_IP);

	iRet = bind(s_iDataConnSock, (struct sockaddr *)&DataAddrLocal, sizeof(struct sockaddr));
	if (iRet < 0)
	{
		ErrorInfo("Data bind error\n");
		return RET_FAIL;
	}

	iRet = listen(s_iDataConnSock, 5);
	if (iRet < 0)
	{
		ErrorInfo("Data listen error\n");
		close(s_iDataConnSock);
		return RET_FAIL;
	}

	//iOpt = sizeof(struct sockaddr);
	//s_iDataAcqSock = accept(s_iDataConnSock, (struct sockaddr *)&DataAddrPeer, (socklen_t *)&iOpt);
	//if (s_iDataAcqSock < 0 )
	//{
	//	close(s_iDataConnSock);
	//	ErrorInfo("accepted error %d.\n", s_iCmdAcqSock);
	//	return RET_FAIL;
	//}

	//iOpt = 1;
	//ioctl(s_iDataAcqSock, FIONBIO, &iOpt);


	///* Create TCP Socket */
	s_iCmdConnSock = socket(PF_INET, SOCK_STREAM, 0);
	if (s_iCmdConnSock < 0)
	{
		ErrorInfo("Create TCP Socket Error!\n");
		return RET_FAIL;
	}

	iOpt = 0;
	ioctl(s_iCmdConnSock, FIONBIO, &iOpt);

	TcpAddrLocal.sin_family = PF_INET;
	TcpAddrLocal.sin_port = htons(LOCAL_CMD_PORT);
	TcpAddrLocal.sin_addr.s_addr = htonl(INADDR_ANY);

	iRet = bind(s_iCmdConnSock, (struct sockaddr *)&TcpAddrLocal, sizeof(struct sockaddr));
	if (iRet < 0)
	{
		ErrorInfo("Cmd bind error\n");
		return RET_FAIL;
	}

	iRet = listen(s_iCmdConnSock, 5);
	if (iRet < 0)
	{
		ErrorInfo("Cmd listen error\n");
		close(s_iCmdConnSock);
		return RET_FAIL;
	}


	s_iMesgConnSock = socket(PF_INET, SOCK_STREAM, 0);
	if (s_iMesgConnSock < 0)
	{
		ErrorInfo("Create Mesg Socket Error!\n");
		return RET_FAIL;
	}

	iOpt = 0;
	ioctl(s_iMesgConnSock, FIONBIO, &iOpt);

	MesgAddrLocal.sin_family = PF_INET;
	MesgAddrLocal.sin_port = htons(LOCAL_MSG_PORG);
	MesgAddrLocal.sin_addr.s_addr = htonl(INADDR_ANY);

	iRet = bind(s_iMesgConnSock, (struct sockaddr *)&MesgAddrLocal, sizeof(struct sockaddr));
	if (iRet < 0)
	{
		ErrorInfo("Mesg bind error\n");
		return RET_FAIL;
	}

	iRet = listen(s_iMesgConnSock, 5);
	if (iRet < 0)
	{
		ErrorInfo("Mesg listen error\n");
		close(s_iMesgConnSock);
		return RET_FAIL;
	}
	

	return RET_OK;
}

int32_t AcceptLink(void)
{
	int32_t iOpt = 0;
	struct timeval tv;

	iOpt = sizeof(struct sockaddr_in);
	s_iDataAcqSock = accept(s_iDataConnSock, (struct sockaddr *)&DataAddrPeer, (socklen_t *)&iOpt);
	if (s_iDataAcqSock > 0 )
	{
		iOpt = 0;
		ioctl(s_iDataAcqSock, FIONBIO, &iOpt);
		iOpt = 256*1024;
		setsockopt(s_iDataAcqSock, SOL_SOCKET, SO_SNDBUF, (const char *)&iOpt, sizeof(int));
		tv.tv_sec = 15;
		tv.tv_usec = 0;
		setsockopt(s_iDataAcqSock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	}

	iOpt = sizeof(struct sockaddr_in);
	s_iCmdAcqSock = accept(s_iCmdConnSock, (struct sockaddr *)&CmdAddrPeer, (socklen_t *)&iOpt);
	if (s_iCmdAcqSock > 0 )
	{
		iOpt = 1;
		ioctl(s_iCmdAcqSock, FIONBIO, &iOpt);
		iOpt = 256*1024;
		setsockopt(s_iCmdAcqSock, SOL_SOCKET, SO_SNDBUF, (const char *)&iOpt, sizeof(int));
		tv.tv_sec = 15;
		tv.tv_usec = 0;
		setsockopt(s_iCmdAcqSock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	}

	iOpt = sizeof(struct sockaddr_in);
	s_iMesgAcqSock = accept(s_iMesgConnSock, (struct sockaddr *)&CmdAddrPeer, (socklen_t *)&iOpt);
	if (s_iMesgAcqSock > 0 )
	{
		iOpt = 1;
		ioctl(s_iMesgAcqSock, FIONBIO, &iOpt);
		iOpt = 256*1024;
		setsockopt(s_iMesgAcqSock, SOL_SOCKET, SO_SNDBUF, (const char *)&iOpt, sizeof(int));
		tv.tv_sec = 15;
		tv.tv_usec = 0;
		setsockopt(s_iMesgAcqSock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	}


	return RET_OK;
}

int32_t InitMem(void)
{
	unsigned int i = 0, j =1;

	int fd = open("/dev/mem", O_RDWR);
	if (fd < 0)
	{
		printf("open memory failed!\n");
		return -1;
	}

	s_puiAddr_A = (unsigned int *)mmap(NULL, LENGTH, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x34000000);
	if (NULL == s_puiAddr_A)
	{
		printf("mmap A failed!\n");
		return -1;
	}

	s_puiAddr_B = (unsigned int *)mmap(NULL, LENGTH, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x34010000);
	if (NULL == s_puiAddr_B)
	{
		printf("mmap B failed!\n");
		return -1;
	}

	return RET_OK;
}

void SetReadyFlag(uint8_t ucFlag)
{
	s_ucIsReady = ucFlag;
}

uint8_t GetReadyFlag(void)
{
	return s_ucIsReady;
}

int32_t SetSampleRate(uint8_t ucSampleRate)
{
	int32_t iCsFd = GetCsrFd();

	RegWrite(iCsFd, CSR_C_TO_IO_CMD, 0xFF00|s_SampleRateReg[ucSampleRate]);
	usleep(100);
	RegWrite(iCsFd, CSR_C_TO_IO_CMD, 0x0000);

	return RET_OK;
}

uint32_t   CreateDir(const  char  *sPathName)  
{  
	char   DirName[256];  
	strcpy(DirName,   sPathName);  
	int   i, len = strlen(DirName);  
	if(DirName[len-1]!='/')  
		strcat(DirName,   "/");  

	len   =   strlen(DirName);  

	for(i=1;   i<len;   i++)  
	{  
		if(DirName[i]=='/')  
		{  
			DirName[i]   =   0;  
			if(   access(DirName,   F_OK)!=0   )  
			{  
				if(mkdir(DirName,   0777)==-1)  
				{   
					perror("mkdir   error");   
					return   0;   
				}  
			}  
			DirName[i]   =   '/';  
		}  
	}  

	return   1;  
}
//
//void CreateMulvPath( char *muldir )
//{
//	int    i,len;
//	char    str[256];
//
//	strncpy( str, muldir, 512 );
//	len=strlen(str);
//	printf( "Dir string length =%d\n", len );
//	for( i=0; i<len; i++ )
//	{
//		if( str[i]=='/' )
//		{
//			str[i] = '\0';
//			
//			if( access(str, F_OK)!=0 )
//			{
//				printf( "Create %d dir p=[%s]\n", i, str );
//				mkdir( str, 0777 );
//			}
//			str[i]='/';
//		}
//	}
//	if( len>0 && access(str, F_OK)!=0 )
//	{
//		printf( "Last dir = %s!!!!!!!\n", str );
//		mkdir( str, 0777 );
//	}
//
//	return;
//
//}

int32_t Configuration(void)
{
	int32_t iCsrFd = GetCsrFd();
	double t1 = 1.0;
	uint32_t t2 = 0;
	uint32_t t3 = 0, tDelta = 0;
	uint32_t uiRet = 0;
	uint32_t uiSeconds = 0;
	struct gps_data_t *pGpsData = NULL;
	uint32_t diReadCheck = 0;
	uint32_t creatDirCheck = 0;
	uint32_t i = 0;
	struct timeval aa;

	int32_t WorkMode = read_profile_int("Coinfiguration", "WorkMode", 0, s_pConfigIni);
	int32_t SyncMode = read_profile_int("Coinfiguration", "SyncMode", 0, s_pConfigIni);
	int32_t FullScalRange = read_profile_int("Coinfiguration", "FullScalRange", 0, s_pConfigIni);
	int32_t LpfFilter = read_profile_int("Coinfiguration", "LpfFilter", 0, s_pConfigIni);
	int32_t InputSource = read_profile_int("Coinfiguration", "InputSource", 0, s_pConfigIni);

	int32_t rWorkMode = GetRealData(s_WorkMode, WorkMode, sizeof(s_WorkMode));
	int32_t rSyncMode = GetRealData(s_SyncMode, SyncMode, sizeof(s_SyncMode));
	int32_t rFullScalRange = GetRealData(s_FullRange, FullScalRange, sizeof(s_FullRange));
	int32_t rLpfFilter = GetRealData(s_Filter, LpfFilter, sizeof(s_Filter));	
	int32_t rInputSource = GetRealData(s_InputSouce, InputSource, sizeof(s_InputSouce));

	int32_t rImpulseWidth = read_profile_int("Coinfiguration", "ImpulseWidth", 0, s_pConfigIni);	
	int32_t rAdcSamplingRates = read_profile_int("Coinfiguration", "AdcSamplingRates", 0, s_pConfigIni);
	int32_t rSequenceLength = read_profile_int("Coinfiguration", "SequenceLength", 0, s_pConfigIni);
	int32_t rGenerationPolymerization = read_profile_int("Coinfiguration", "GenerationPolymerization", 0, s_pConfigIni);
	int32_t rInitialValue = read_profile_int("Coinfiguration", "InitialValue", 0, s_pConfigIni);
	s_uiSavePC = read_profile_int("Coinfiguration", "SavePosition", 0, s_pConfigIni); //change to SavePosition later
	int32_t AdcSamplingRates = rAdcSamplingRates;
	
	diReadCheck = read_profile_string("Coinfiguration", "DataDirectory", s_SdDataDIR, sizeof(s_SdDataDIR), 0, s_pConfigIni);
	if(diReadCheck == 0)
	{
		printf("New data dir get failed!!!\n");
	}
	
	creatDirCheck = CreateDir(s_SdDataDIR);
	if(creatDirCheck == 0)
	{
		printf("Create new dir %s failed!!!\n", s_SdDataDIR);
	}
	
	//Save config info to arry for later use
	s_puiConfigBuf[i++] = WorkMode;
	s_puiConfigBuf[i++] = SyncMode;
	memcpy(&(s_puiConfigBuf[i]),s_SdDataDIR,sizeof(s_SdDataDIR));
	
	i = i + (sizeof(s_SdDataDIR))/4;
	s_puiConfigBuf[i++] = FullScalRange;
	s_puiConfigBuf[i++] = InputSource;
	s_puiConfigBuf[i++] = LpfFilter;
	s_puiConfigBuf[i++] = rAdcSamplingRates;
	s_puiConfigBuf[i++] = rImpulseWidth;
	s_puiConfigBuf[i++] = rSequenceLength;
	s_puiConfigBuf[i++] = rGenerationPolymerization;
	s_puiConfigBuf[i++] = rInitialValue;
	s_puiConfigBuf[i++] = s_uiSavePC;
	
	//set work mode flag
	s_WorkMode_TX = rWorkMode;

	// Input Select
	RegWrite(iCsrFd, CSR_INPUT_SEL, rInputSource);

	// Set tx or rx work Mode.
	RegWrite(iCsrFd, CSR_TRANS_EN, rWorkMode);

	// Set Syn Mode.
	RegWrite(iCsrFd, CSR_SYNC_MODE, rSyncMode);

	// Set full scale range
	RegWrite(iCsrFd, CSR_FLL_SCALE_SET, rFullScalRange);

	// Set Filter.
	RegWrite(iCsrFd, CSR_FLT_SEL, rLpfFilter);

	// Set Sample Rate 
	SetSampleRate(AdcSamplingRates);

	// Set Code Length
	RegWrite(iCsrFd, CSR_CODE_LENGTH, (Power(rSequenceLength) - 1));
	printf("Code Length: %d bits.\n", (Power(rSequenceLength) - 1));
	// Set Samples Length
	s_ulSampleLength = (uint64_t)(Power(rSequenceLength) - 1)*(uint64_t)rImpulseWidth*(uint64_t)s_SampleRateValue[AdcSamplingRates]/1000000*4;
	printf("%d %d %d\n", (Power(rSequenceLength) - 1), rImpulseWidth, s_SampleRateValue[AdcSamplingRates]);
	printf("SampleLength: %lld bytes.\n", s_ulSampleLength);
	//RegWrite(iCsrFd, CSR_SAMPLES_LENGTH, (Power(rSequenceLength) - 1)*rImpulseWidth*s_SampleRateValue[AdcSamplingRates]);

	// Set Code Clock Length
	RegWrite(iCsrFd, CSR_CODE_CLK_CYCLES, rImpulseWidth*40);
	printf("Code Clock Cycles: %d.\n", rImpulseWidth*40);

	// Write Code to RAM
	SetDataToRam(rInitialValue, rGenerationPolymerization, rSequenceLength);

	//pGpsData = PraseGPS();
	pGpsData = GetGpsInfo();
	//uiSeconds = pGpsData->tm_time.tm_hour*3600 + pGpsData->tm_time.tm_min*60 + pGpsData->tm_time.tm_sec;
	//printf("%02d:%02d:%02d.\n", pGpsData->tm_time.tm_hour, pGpsData->tm_time.tm_min, pGpsData->tm_time.tm_sec);
	
	gettimeofday(&aa, NULL); 
	uiSeconds = aa.tv_sec;
	printf("Second: %d.\n", uiSeconds);

	t1 = (double)(Power(rSequenceLength) - 1)*(double)rImpulseWidth/1000000.0;
	t2 = ceil(t1);
	t3 = t2 + 2;
	tDelta = (t3 - (uiSeconds%t3));

	printf("T_ACQ: %d.\n", t3);
	printf("T_Delta: %d.\n", tDelta);

	RegWrite(iCsrFd, CSR_T_ACQ, t3);
	RegWrite(iCsrFd, CSR_T_DELTA, tDelta);
	RegWrite(iCsrFd, CSR_T_START, 1);

	return RET_OK;
}

void TestSd(void)
{
	const uint32_t size = 10485760;
	printf("Test Sd...\n");
	struct timeval aa, bb;
	float cc = 2.4;
	s_pDataFd = fopen(s_pDataFile, "wb");
	if (s_pDataFd < 0)
	{
		printf("Open file %s failed.\n", s_pDataFile);
	}
	printf("cc %f.\n", cc);
	gettimeofday(&aa, NULL); 

	fwrite(s_pSimuData, sizeof(int), size, s_pDataFd);
	fwrite(s_pSimuData, sizeof(int), size, s_pDataFd);
	fwrite(s_pSimuData, sizeof(int), size, s_pDataFd);
	fwrite(s_pSimuData, sizeof(int), size, s_pDataFd);
	fwrite(s_pSimuData, sizeof(int), size, s_pDataFd);
	fclose(s_pDataFd);
	gettimeofday(&bb, NULL); 

	cc = (float)size*5.0*4.0/(bb.tv_sec - aa.tv_sec);
	printf("Speed %f.\n", cc);

}

void UdpDeliverData(void)
{
	uint32_t regValue = 0;
	uint32_t uiReady = 0;
	uint32_t uiReadyNo = 0;
	int32_t iRet = 0;
	int32_t i = 0;
	uint8_t DataBuff[BRAM_SIZE];

	int32_t csrFd = GetCsrFd();
	int32_t bramFd = GetBramFd();

	uint32_t uiRet = 0;

	//for(i = 0; i < 1280*32; i++)
	//{
	//	//if (iRet > 0)
	//	{
	//		iRet = sendto(s_iDataAcqSock, (((uint8_t*)s_pSimuData)+i*1024), 1024, 0, (struct sockaddr*)&DataAddrPeer, sizeof(struct sockaddr));			
	//	}	
	//}

	while(1)
	{
		regValue = RegRead(csrFd, CSR_PROG_FULL);
		uiReady = regValue & 0x1;
		//printf("reg 10 is %d; ready is %d.\n", regValue, uiReady);
		if (1 == uiReady)
		{
			break;
		}


	}
	if (1 == uiReady)
	{		
		s_tick++;
		iRet = read(bramFd, DataBuff, BRAM_SIZE);
		uiRet = GetAverageUnit((uint32_t *)DataBuff, BRAM_SIZE);		


		//if (s_tick <= 10)
		//{
		//if (iRet > 0)
		//{
		//	iRet = fwrite(DataBuff, sizeof(char), BRAM_SIZE, s_pDataFd);
		//	if (iRet < 0)
		//	{
		//		printf("Write file %s failed.\n", s_pDataFile);
		//	}

		//}
		//}

		for(i = 0; i<2; i++)
		{
			if (iRet > 0)
			{
				iRet = sendto(s_iDataConnSock, DataBuff+i*32768, 32768, 0, (struct sockaddr*)&DataAddrPeer, sizeof(struct sockaddr));	
				//iRet = sendto(s_iDataConnSock, DataBuff, 65536, 0, (struct sockaddr*)&DataAddrPeer, sizeof(struct sockaddr));			
			}	
		}
	}

}

//void TcpDeliverData(const uint8_t *buff, uint32_t uiExpectedLen)
//{
//	int32_t iRet = 0;
//	if (uiExpectedLen <= PACKAGE_SIZE)
//	{
//		iRet = send(s_iDataAcqSock, buff, uiExpectedLen, 0);
//		if (iRet < 0)
//		{
//			ErrorInfo("Send Data Failed![%d]\n", iRet);
//		}
//
//		s_ucDeliveredDone = 1;
//	}
//}

void DeliverData(void)
{
	uint32_t regValue = 0;
	uint32_t uiReady = 0;
	uint32_t uiReadyNo = 0;
	int32_t iRet = 0;
	int32_t i = 0;
	uint8_t DataBuff[BRAM_SIZE];
	uint8_t FileBuff[FILE_BUFF_SIZE];
	
	static unsigned long long m_ulTotalLen = 0;
	static unsigned long long m_SendLen = 0;

	int32_t csrFd = GetCsrFd();
	int32_t bramFd = GetBramFd();
	int32_t irqFd = GetIrqFd();
	int32_t status = 0;
	uint32_t uiRet = 0;

	while(1)
	{
		regValue = RegRead(csrFd, CSR_PROG_FULL);
		uiReady = regValue & 0x1;
		//printf("reg 10 is %d; ready is %d.\n", regValue, uiReady);
		if (1 == uiReady)
		{
			break;
		}
	}

	if (1 == uiReady)
	{		
		s_tick++;
		iRet = read(bramFd, DataBuff, BRAM_SIZE);

		if ((s_ulSampleLength-m_ulTotalLen) <= PACKAGE_SIZE)
		{
			iRet = send(s_iDataAcqSock, DataBuff, (s_ulSampleLength-m_ulTotalLen), 0);
			s_ucDeliveredDone = 1;
			m_ulTotalLen = 0;
			s_ucIsReady = FALSE;
			m_SendLen += iRet;
			printf("1. Send %lld[%d].\n", m_SendLen, iRet);
			m_SendLen = 0;
		}
		else if ((s_ulSampleLength-m_ulTotalLen) <= BRAM_SIZE)
		{

			iRet = send(s_iDataAcqSock, DataBuff, PACKAGE_SIZE, 0);
			m_SendLen += iRet;
			//printf("2.1. Send %lld[%d].\n", m_SendLen, iRet);
			iRet = send(s_iDataAcqSock, DataBuff+PACKAGE_SIZE, ((s_ulSampleLength-m_ulTotalLen)-PACKAGE_SIZE), 0);
			m_SendLen += iRet;
			s_ucDeliveredDone = 1;
			m_ulTotalLen = 0;
			s_ucIsReady = FALSE;
			printf("2.2. Send %lld[%d].\n", m_SendLen, iRet);
			m_SendLen = 0;

		}
		else
		{
			for (i=0; i<2; ++i)
			{
				iRet = send(s_iDataAcqSock, DataBuff+i*PACKAGE_SIZE, PACKAGE_SIZE, 0);				
				m_SendLen += iRet;
				//printf("3. Send %lld[%d].\n", m_SendLen, iRet);
			}

			m_ulTotalLen += BRAM_SIZE;
		}		
	}

}

void DeliverData_s(void)

{
	uint32_t regValue = 0;
	uint32_t uiReady = 0;
	uint32_t uiReadyNo = 0;
	int32_t iRet = 0;
	uint32_t i = 0, j = 0;
	uint8_t DataBuff[BRAM_SIZE];
	uint8_t FileBuff[FILE_BUFF_SIZE];

	//static unsigned long long m_ulTotalLen = 0;
	static unsigned long long m_SendLen = 0;

	int32_t status = 0;
	uint32_t uiRet = 0;

	int iDataIrqFd = GetDataIrqFd();
	iRet = read(iDataIrqFd, &status, 4);

	if ((1 == iRet) && (TRUE == s_ucIsReady))
	{		
		s_uiFlag++;
		//printf("D: %d\n", s_uiFlag);
		if (s_uiFlag%2 == 1)
		{
			s_puiAddr = s_puiAddr_A;
		}
		else
		{
			s_puiAddr = s_puiAddr_B;
		}

		if ((s_ulSampleLength-s_ulTotalLen) <= PACKAGE_SIZE)
		{
			// for (j=0; j<((s_ulSampleLength-m_ulTotalLen)/4); j++)
			// {				
			// if (*(s_puiAddr+j) != s_uiCheck)
			// {
			// printf("1 Error: %d, %d.\n", *(s_puiAddr+j), s_uiCheck);
			// s_uiCheck = *(s_puiAddr+j);
			// }
			// s_uiCheck++;
			// }
			// printf("1 finished compare data length: %d.\n", s_uiCheck);
			if(s_uiSavePC == 0)
			{
				//fwrite(s_puiAddr, 1, (s_ulSampleLength-s_ulTotalLen), s_pSdHPFd);
				fwrite(s_puiAddr, (s_ulSampleLength-s_ulTotalLen), 1, s_pSdHPFd);
				//fflush(s_pSdHPFd);

				/*pMap = (char *)mmap(0,(s_ulSampleLength-s_ulTotalLen), PROT_WRITE, MAP_SHARED, s_pSdHPFd, s_ulTotalTimes*BRAM_SIZE);
				memcpy(pMap , (char *)s_puiAddr, (s_ulSampleLength-s_ulTotalLen));
				munmap(pMap, (s_ulSampleLength-s_ulTotalLen));*/
			}
			else
			{
				iRet = send(s_iDataAcqSock, s_puiAddr, (s_ulSampleLength-s_ulTotalLen), 0);
				//m_SendLen += iRet;iRet = send(s_iDataAcqSock, ((uint8_t *)s_puiAddr)+i*PACKAGE_SIZE, PACKAGE_SIZE, 0);	
			}
			s_ucDeliveredDone = 1;
			s_ucIsReady = FALSE;
			//printf("1. Send %lld[%d].\n", m_SendLen, iRet);
			m_SendLen = 0;
			//s_uiFlag = 0;
			//s_ulTotalLen = 0;
			//s_uiCheck = 1;
			//fclose(s_pSdHPFd);
		}
		else if ((s_ulSampleLength-s_ulTotalLen) <= BRAM_SIZE)
		{
			// for (j=0; j<((s_ulSampleLength-m_ulTotalLen)/4); j++)
			// {				
			// if (*(s_puiAddr+j) != s_uiCheck)
			// {
			// printf("2 Error: %d, %d.\n", *(s_puiAddr+j), s_uiCheck);
			// s_uiCheck = *(s_puiAddr+j);
			// }
			// s_uiCheck++;
			// }
			// printf("2 finished compare data length: %d.\n", s_uiCheck);

			if(s_uiSavePC == 0)
			{
				//fwrite(s_puiAddr,1, (s_ulSampleLength-s_ulTotalLen), s_pSdHPFd);
				fwrite(s_puiAddr, (s_ulSampleLength-s_ulTotalLen), 1, s_pSdHPFd);
				//fflush(s_pSdHPFd);

				/*pMap = (char *)mmap(0,(s_ulSampleLength-s_ulTotalLen), PROT_WRITE, MAP_SHARED, s_pSdHPFd, s_ulTotalTimes*BRAM_SIZE);
				memcpy(pMap , (char *)s_puiAddr, (s_ulSampleLength-s_ulTotalLen));
				munmap(pMap, (s_ulSampleLength-s_ulTotalLen));*/
			}
			else
			{
				iRet = send(s_iDataAcqSock, (uint8_t *)s_puiAddr, PACKAGE_SIZE, 0);
				//m_SendLen += iRet;
				iRet = send(s_iDataAcqSock, ((uint8_t *)s_puiAddr)+PACKAGE_SIZE, ((s_ulSampleLength-s_ulTotalLen)-PACKAGE_SIZE), 0);
				//m_SendLen += iRet;iRet = send(s_iDataAcqSock, ((uint8_t *)s_puiAddr)+i*PACKAGE_SIZE, PACKAGE_SIZE, 0);	
			}
			
			//printf("2.1. Send %lld[%d].\n", m_SendLen, iRet);
			s_ucDeliveredDone = 1;
			s_ucIsReady = FALSE;
			//printf("2.2. Send %lld[%d].\n", m_SendLen, iRet);
			m_SendLen = 0;
			//s_uiFlag = 0;
			//s_ulTotalLen = 0;
			//s_uiCheck = 1;
			//fclose(s_pSdHPFd);
		}
		else
		{
			// for (j=0; j<(BRAM_SIZE/4); j++)
			// {					
			// if (*(s_puiAddr+j) != s_uiCheck)
			// {
			// printf("3 Error: %d, %d.\n", *(s_puiAddr+j), s_uiCheck);
			// s_uiCheck = *(s_puiAddr+j);
			// }
			// s_uiCheck++;
			// }

			if(s_uiSavePC == 0)
			{
				//fwrite(s_puiAddr, 1, BRAM_SIZE, s_pSdHPFd);
				fwrite(s_puiAddr, BRAM_SIZE, 1, s_pSdHPFd);
				//fflush(s_pSdHPFd);

				/*	pMap = (char *)mmap(0,BRAM_SIZE, PROT_WRITE, MAP_SHARED, s_pSdHPFd, s_ulTotalTimes*BRAM_SIZE);
				//mmap(pMap,BRAM_SIZE, PROT_WRITE, MAP_SHARED, s_pSdHPFd, s_ulTotalTimes*BRAM_SIZE);
				memcpy(pMap , (char *)s_puiAddr, BRAM_SIZE);
				munmap(pMap, BRAM_SIZE);
				s_ulTotalTimes += iRet;*/
			}
			else
			{
				for (i=0; i<2; ++i)
				{
					iRet = send(s_iDataAcqSock, ((uint8_t *)s_puiAddr)+i*PACKAGE_SIZE, PACKAGE_SIZE, 0);	
					//m_SendLen += iRet;
					//printf("3. Send %lld[%d].\n", m_SendLen, iRet);
				}
			}

			s_ulTotalLen += BRAM_SIZE;
		}		
	}

}

int32_t GetAverage(int32_t *Data, uint32_t len)
{
	int32_t iRet = 0;
	long long llTotal = 0;
	uint32_t i = 0;
	uint32_t uiCount = len / sizeof(int32_t);

	for (i=0; i<uiCount; i++)
	{
		llTotal += Data[i];
	}

	iRet = llTotal/uiCount;

	return iRet;

}

uint32_t GetAverageUnit(uint32_t *Data, uint32_t len)
{
	uint32_t uiRet = 0;
	unsigned long long ulTotal = 0;
	uint32_t i = 0;
	uint32_t uiCount = len / sizeof(int32_t);


	for (i=0; i<uiCount; i++)
	{
		s_cnt++;
		if (s_cnt != Data[i])
		{
			printf("%u-%u.\n", s_cnt, Data[i]);
			s_cnt = Data[i];
		}
	}

	//uiRet = ulTotal/uiCount;
	//uiRet = s_ulTotal/(s_tick*uiCount);

	return uiRet;
}

int32_t App(void)
{
	int32_t iRet = 0;
	CmdHandler pCmdHandler = NULL;
	int32_t len = sizeof(struct sockaddr_in);
	iRet = recvfrom(s_iCmdAcqSock, s_pTcpBuff, TCP_BUFF_SIZE, 0, (struct sockaddr *)&CmdAddrPeer, (socklen_t *)&len);
	if (iRet > 0)
	{
		pCmdHandler = GetCmdHandler(s_pCmdHead->usCmd);
		if (pCmdHandler != NULL)
		{
			pCmdHandler();
		}

		iRet = send(s_iCmdAcqSock, s_pTcpBuff, s_uiTcpSendLen, 0);
	}

	return RET_OK;
}

int32_t AcqStart(void)
{
	int32_t i = 0;
	int32_t csrFd = GetCsrFd();
	// Read Configuration.ini
	Configuration();

	//s_tick = 0;
	//s_cnt = 0;
	//s_pDataFd = fopen(s_pDataFile, "wb");
	//if (s_pDataFd < 0)
	//{
	//	printf("Open file %s failed.\n", s_pDataFile);
	//}
	//for (i=0; i<10485760; i++)
	//{
	//	s_pSimuData[i]= i+1;
	//}

	s_uiFlag = 0;
	RegWrite(csrFd, CSR_REC_EN, 1);

	s_pAckHead->usCmd = ACQ_START_ACK;
	s_pAckHead->usLength = 0;

	s_uiTcpSendLen = CMDHEADER_SIZE;

	//s_ucIsReady = TRUE;

	InitInfo("%s\n", __FUNCTION__);

	return RET_OK;
}

int32_t SaveFileToSd(void)
{
	uint32_t i = 0;
	FILE *pMemFd = NULL;
	unsigned char tmpBuff[BRAM_SIZE];
	pMemFd = fopen(s_pDataFile, "rb");
	if (NULL == pMemFd)
	{
		printf("open file %s failed.\n", s_pDataFile);
	}
	s_pSdFd = fopen(s_pSdFile, "wb");
	if (NULL == s_pSdFd)
	{
		printf("open file %s failed.\n", s_pSdFile);
	}

	for (i=0; i<s_ulDataSize/BRAM_SIZE; i++)
	{
		fread(tmpBuff, 1, BRAM_SIZE, pMemFd);
		fwrite((const void *)tmpBuff, 1, BRAM_SIZE, s_pSdFd);
	}
	s_ulDataSize = 0;
	fclose(pMemFd);
	fclose(s_pSdFd);

	return RET_OK;
}

int32_t AcqStop(void)
{

	int32_t iCsrFd = GetCsrFd();
	InitInfo("%s\n", __FUNCTION__);

	RegWrite(iCsrFd, CSR_REC_EN, 0);

	//s_tick = 0;
	//s_cnt = 0;
	//fclose(s_pDataFd);

	//SaveFileToSd();

	RegWrite(iCsrFd, CSR_T_ACQ, 0);
	RegWrite(iCsrFd, CSR_T_DELTA, 0);
	RegWrite(iCsrFd, CSR_T_START, 0);

	s_pAckHead->usCmd = ACQ_STOP_ACK;
	s_pAckHead->usLength = 0;

	s_uiTcpSendLen = CMDHEADER_SIZE;

	s_ucIsReady = FALSE;
	s_uiFlag = 0;
	
	if(s_uiSavePC == 0)  //if save sd selected, remove the file
	{
		if(s_pSdHPFd != NULL)
		{
			remove(s_SD_FILE_NAME);
			fclose(s_pSdHPFd);
			s_pSdHPFd = NULL;
		}
	}


	return RET_OK;
}


int32_t AppStart(void)
{


	return RET_OK;
}
