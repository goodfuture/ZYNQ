#ifndef _APP_H_
#define _APP_H_

#include "common.h"
#include "csr.h"
#include "main.h"
#include "inifile.h"
#include "lfsr.h"
#include "gps.h"
#include <sys/time.h>
#include <math.h>
#include <unistd.h>
#include <sys/mman.h>


#define CMDHEADER_SIZE			sizeof(CmdHeader)
#define ACKHEADER_SIZE			CMDHEADER_SIZE
#define UDPHEADER_SIZE			sizeof(UdpHeader)
#define MESGHEADER_SIZE			sizeof(MesgHeader)

#define TCP_DATA_SIZE			1024
#define TCP_BUFF_SIZE			(TCP_DATA_SIZE+CMDHEADER_SIZE)
#define UDP_DATA_SIZE			65536
#define UDP_BUFF_SIZE			(UDP_DATA_SIZE+UDPHEADER_SIZE)
#define MESG_MAX_SIZE			1024
#define MESG_BUFF_SIZE			(MESG_MAX_SIZE+MESGHEADER_SIZE)

#define REMOTE_IP				"192.168.1.100"
#define LOCAL_CMD_PORT			14310
#define LOCAL_DATA_PORT			14311
#define LOCAL_MSG_PORG			14312
#define REMOTE_UDP_PORT			1601
#define REMOTE_TCP_PORT			1431

#define SAMPLE_SIZE				12500*4
//#define BRAM_SIZE				(uint32_t)131072	
#define BRAM_SIZE				(uint32_t)65536
#define FILE_BUFF_SIZE			BRAM_SIZE*16
#define PACKAGE_SIZE			(uint32_t)32768
//#define BLOCK_DATA_SIZE			(uint32_t)16384	
#define PACKAGE_TOTAL			
#define CALIBRATE_TIMES			5

#define INI_ITEM_NUM			100

#define SEC_3V_POS				"3V_3V"
#define SEC_3V_NEG				"3V_-3V"
#define SEC_3V_NON				"3V_Zero"

#define SEC_300mV_POS			"300mV_300mV"
#define SEC_300mV_NEG			"300mV_-300mV"
#define SEC_300mV_NON			"300mV_Zero"

#define SEC_30mV_POS			"30mV_30mV"
#define SEC_30mV_NEG			"30mV_-30mV"
#define SEC_30mV_NON			"30mV_Zero"

#define SEC_3mV_POS				"3mV_3mV"
#define SEC_3mV_NEG				"3mV_-3mV"
#define SEC_3mV_NON				"3mV_Zero"

#define SEC_300uV_POS			"300uV_300uV"
#define SEC_300uV_NEG			"300uV_-300uV"
#define SEC_300uV_NON			"300uV_Zero"

#define KEY_NONE				"Filter_None"
#define KEY_120KHz				"Filter_120KHz"
#define KEY_12KHz				"Filter_12KHz"
#define KEY_1_2KHz				"Filter_1.2KHz"

#define Sample_Rate_78_125KHz		0
#define Sample_Rate_156_25KHz		1
#define Sample_Rate_312_5KHz		2
#define Sample_Rate_625KHz			3
#define Sample_Rate_1_25MHz			4
#define Sample_Rate_2_5MHz			5

//#define LENGTH						128*1024
#define LENGTH						64*1024

typedef enum enuCmd
{
	ACQ_START_CMD = 0x1001,
	ACQ_START_ACK,

	ACQ_STOP_CMD,
	ACQ_STOP_ACK,


}enuCmd;

typedef enum enuMesg
{
	AcqBeginMesg = 0x2001,
	AcqBeginAck,

	AcqEndMesg,
	AcqEndAck,

	GpsTimeMesg,
	GpsTimeAck,

}enuMesg;

typedef struct tagIniData
{
	char pItem[50];
	int32_t iValue;

}IniData;

typedef struct tagCmdHeader
{
	uint16_t usCmd;
	uint16_t usLength;
	uint32_t uiReserved;

}CmdHeader;

typedef struct tagMesgHeader
{
	uint16_t usMesg;
	uint16_t usLength;
	uint32_t uiReserved;

}MesgHeader;

typedef struct tagCmdHeader AckHeader;

typedef struct tagUdpHeader
{
	uint16_t usAck;
	uint16_t usReserved;
	uint32_t uiLength;

}UdpHeader;

typedef int32_t (*CmdHandler)(void);

typedef struct tagCommandData
{
	uint16_t usCmd;
	CmdHandler Handler;

}CommandData;

typedef struct tagInterData
{
	int iPcData;
	int iArmData;

}InterData;


void *MesgThread(void *args);

CmdHandler GetCmdHandler(uint16_t usCmd);

uint32_t   CreateDir(const  char  *sPathName); 
int32_t InitNetLink(void);
int32_t AcceptLink(void);
int32_t InitMem(void);
void SetReadyFlag(uint8_t ucFlag);
uint8_t GetReadyFlag(void);
void DeliverData(void);
void DeliverData_s(void);
void UdpDeliverData(void);
int32_t UdpInitNetLink(void);
void TestSd(void);
int32_t App(void);
int32_t AcqStart(void);
int32_t AcqStop(void);
int32_t AppStart(void);
int32_t GetAverage(int32_t *Data, uint32_t len);
uint32_t GetAverageUnit(uint32_t *Data, uint32_t len);
int32_t SetDataToRam(uint32_t uiSeed, uint32_t uiValue, uint32_t n);


#endif
