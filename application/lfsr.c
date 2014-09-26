#include <stdio.h>
#include "common.h"
#include "lfsr.h"

uint8_t buff[2048];

uint32_t BitToHex(uint8_t bits[], uint8_t len);

uint32_t Power(uint32_t n)
{
	uint32_t uiRet = 1;
	uint32_t i = 0;

	for (i=0; i<n; i++)
	{
		uiRet = uiRet*2;
	}

	return uiRet;
}

uint32_t BitToHex(uint8_t bits[], uint8_t len)
{
	uint32_t uiHex = 0;
	int i = 0;

	for (i=len-1; i>=0; i--)
	{
		uiHex = (uiHex<<1) | bits[i];
	}

	return uiHex;
}

uint8_t HexToBit(uint16_t usHex, uint8_t bits[])
{
	uint8_t len = 0;
	uint8_t i = 0;
	uint16_t usValue = usHex;

	for (i=0; i<16; i++)
	{
		if ((0x8000 & usValue) == 0x8000)
		{
			break;
		}
		else
		{
			usValue = usValue << 1;
		}
	}

	len = 16 - i;

	for (i=0; i<len; i++)
	{
		bits[i] = (usHex >> i) & 1;
	}

	return len;
}

void GetBitSet(uint32_t uiValue, uint8_t *pucFirst, uint8_t *pucSecond)
{
	uint8_t i = 0;
	uint8_t ucLen = sizeof(uiValue)*8;
	uint32_t uiMask = 1 << (ucLen - 1);
	*pucFirst = 0;
	*pucSecond = 0;

	for(i=0; i<ucLen; i++)
	{
		if (0 == *pucFirst)
		{
			if (uiMask == (uiMask & (uiValue << i)))
			{
				*pucFirst = ucLen - i - 1;
			}
		}
		else
		{
			if (uiMask == (uiMask & (uiValue << i)))
			{
				*pucSecond = ucLen - i - 1;
				break;
			}
		}
	}

}

uint32_t GetLsfr(uint32_t uiSeed, uint8_t ucFirst, uint32_t ucSecond, uint8_t *bit)
{
	uint32_t uiA = 0, uiB = 0, uiC = 0;
	uint32_t uiRet = 0;

	uiA = (uiSeed >> (7 - ucFirst)) & 0x1;
	uiB = (uiSeed >> (7 - ucSecond)) & 0x1;
	uiC = uiA ^ uiB;

	*bit = uiSeed & 0x1;

	uiRet = (uiSeed >> 1) | (uiC << 6);

	return uiRet;

}

int32_t CheckValue(uint32_t uiValue, uint32_t n)
{
	int32_t iRet = 0;

	if ((uiValue & 0x1) != 0x1)
	{
		return -1;
	}

	if (((uiValue >> n) & 0x1) != 0x1)
	{
		return -1;
	}

	return iRet;
}

uint32_t GetBitNum(uint32_t uiValue)
{
	uint32_t uiRet = 0;

	while(uiValue)
	{
		uiValue = uiValue & (uiValue -1);
		uiRet++;

	}

	return uiRet;
}

int GetLfsrUint(uint32_t uiSeed, uint32_t uiValue, uint32_t *puiBuff, uint32_t n)
{
	uint8_t bitBuff[2048];
	uint32_t i = 0, j = 0, uiNum = 0;
	uint32_t uiRet = 0;

	if (n > 11)
	{
		printf("n exceeds 11.\n");
		return 0;
	}

	uiNum = Power(n) - 1;
	uiRet = uiSeed;
	printf("%d %d %d.\n", uiSeed, uiValue, uiNum);
	for (i=0; i<uiNum; i++)
	{
		//if (i%32 == 0)
		//{
		//	printf("\n");
		//}
		//uiRet = GetLfsrBit(uiRet, uiValue, &bitBuff[i], n);
		bitBuff[i] = 0;
		//printf("%d ", bitBuff[i]);

	}

	for (j=0; j<(uiNum+1)/32; j++)
	{
		puiBuff[j] = BitToHex(&bitBuff[j*32], 32);
	}

	return 0;
}
