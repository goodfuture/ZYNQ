#ifndef _CSR_H_
#define _CSR_H_

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <string.h>

#include "csr.h"
#include "common.h"

#define REG_OFFSET(uiLabel)		(uiLabel >> 24) 
#define	REG_SHIFT(uiLabel)	((uiLabel&0x0000FF00)>>8)
#define REG_BITNUM(uiLabel)		(uiLabel&0x000000FF)

/* Label */
/* Reg 0 */
#define CSR_C_TO_IO_BUSY			0x00001F01
#define CSR_C_TO_IO_STOP			0x00001E01
#define CSR_C_TO_IO_CTS				0x00001D01
#define CSR_C_TO_IO_RTS				0x00001C01
#define CSR_C_TO_IO_CMD				0x00000010

/* Reg 1 */
#define CSR_IO_TO_C_BUSY			0x01001F01
#define CSR_IO_TO_C_STOP			0x01001E01
#define CSR_IO_TO_C_CTS				0x01001D01
#define CSR_IO_TO_C_RTS				0x01001C01
#define CSR_IO_TO_C_D				0x01000010

/* Reg 2 */
#define CSR_TRANS_EN				0x02000D01
#define CSR_REC_EN					0x02000C01
#define CSR_SYNC_MODE				0x02000B01
#define CSR_MODE					0x02000A01
#define CSR_FLT_SEL					0x02000802
#define CSR_FLL_SCALE_SET			0x02000206
#define CSR_INPUT_SEL				0x02000002

/* Reg 3 */
#define CSR_CODE_LENGTH				0x03000020

/* Reg 4 */
#define CSR_SAMPLES_LENGTH			0x04000020

/* Reg 5 */
#define CSR_CODE_CLK_CYCLES			0x05000020

/* Reg 6 */
#define CSR_T_START					0x06001F01
#define CSR_T_DELTA					0x0600100F
#define CSR_T_ACQ					0x06000010

/* Reg 10 */
//#define CSR_DATA_RDY				0x0A001F01
//#define CSR_RDY_NO					0x0A000002
//#define CSR_DATA_RDY_MASK			0x80000000
//#define CSR_RDY_NO_MASK				0x00000003
#define CSR_PROG_FULL					0x0A000001

/* Reg 11 */
//#define CSR_START					0x0B000101
#define CSR_PPS1					0x0B000001

#define INPUT_OUT_SINGAL			(uint8_t)0
#define INPUT_GND					(uint8_t)1
#define INPUT_DAC					(uint8_t)3

#define FULL_SCALE_3V				(uint8_t)0
#define FULL_SCALE_300mV			(uint8_t)1
#define FULL_SCALE_30mV				(uint8_t)2
#define FULL_SCALE_3mV				(uint8_t)3
#define FULL_SCALE_300uV			(uint8_t)7

#define FILTER_12KHz				(uint8_t)0
#define FILTER_NONE					(uint8_t)1
#define FILTER_1_2KHz				(uint8_t)2
#define FILTER_120KHz				(uint8_t)3

#define CALIBRATION_MODE			(uint8_t)1
#define WORK_MODE					(uint8_t)0

#define SYNC_MODE_GPS				(uint8_t)0
#define SYNC_MODE_OUT				(uint8_t)1

int CsrInit(void);
uint32_t RegRead(int32_t fd, uint32_t uiLabel);
int32_t RegWrite(int32_t fd, uint32_t uiLabel, uint32_t uiValue);

#endif
