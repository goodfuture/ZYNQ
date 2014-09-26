#include "csr.h"

static uint32_t GetRegMask(uint32_t uiLabel);

uint32_t GetRegMask(uint32_t uiLabel)
{
	uint32_t uiMask = 0;
	uint8_t i = 0;
	uint8_t ucBitNum = 0;
	uint8_t ucShift = 0;

	ucBitNum = REG_BITNUM(uiLabel);
	ucShift = REG_SHIFT(uiLabel);
	for (i=0; i<ucBitNum; i++)
	{
		uiMask = uiMask | (1 << (ucShift + i));
	}

	return uiMask;
}

uint32_t RegRead(int32_t fd, uint32_t uiLabel)
{
	int32_t iRet = 0;
	uint32_t uiRegValue = 0;
	uint32_t uiMask = 0;
	uint8_t ucShift = 0;
	uint8_t ucOffset = 0;
	uint32_t uiRead = 0;

	ucOffset = REG_OFFSET(uiLabel);
	ucShift = REG_SHIFT(uiLabel);
	uiMask = GetRegMask(uiLabel);

	Debugout("RegRead: offset %d shift %d mask 0x%X\n", ucOffset, ucShift, uiMask);

	lseek(fd, ucOffset, 0);
	iRet = read(fd, (char *)&uiRead, 4);

	uiRegValue = (uiRead & uiMask) >> ucShift;

	return uiRegValue;
}

int32_t RegWrite(int32_t fd, uint32_t uiLabel, uint32_t uiValue)
{
	int32_t iRet = 0;
	uint32_t uiMask = 0;
	uint8_t ucShift = 0;
	uint8_t ucOffset = 0;

	uint32_t uiRead = 0;
	uint32_t uiWrite = 0;

	ucOffset = REG_OFFSET(uiLabel);
	ucShift = REG_SHIFT(uiLabel);
	uiMask = GetRegMask(uiLabel);
	Debugout("RegWrite: offset %d shift %d mask 0x%X\n", ucOffset, ucShift, uiMask);

	lseek(fd, ucOffset, 0);
	iRet = read(fd, (char *)&uiRead, 4);
	uiWrite = (uiRead & (~uiMask)) | ((uiValue << ucShift) & uiMask);
	Debugout("uiRead:0x%X; uiWrite:0x%X; uiValue:0x%X.\n", uiRead, uiWrite, uiValue);
	iRet = write(fd, (char *)&uiWrite, 4);

	return iRet;
}



