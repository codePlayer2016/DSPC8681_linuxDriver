/*
 * common.c
 *
 *  Created on: Nov 4, 2015
 *      Author: spark
 */
//#include <stdint.h>
#include <linux/module.h>
//#include <linux/types.h>
#include <linux/kernel.h>
uint32_t byteTo32bits(uint8_t *pDspCode)
{
	uint32_t i, temp;

	temp = *pDspCode++;
	for (i = 0; i < 3; i++)
	{
		temp <<= 8;
		temp |= *pDspCode++;
	}
	return (temp);
}

int pollValue(uint32_t *pAddress, uint32_t pollVal, uint32_t maxPollCount)
{
	int retVal = 0;
	uint32_t loopCount = 0;
	uint32_t stopPoll = 0;
	uint32_t realTimeVal = 0;

	for (loopCount = 0; (loopCount < maxPollCount) && (stopPoll == 0); loopCount++)
	{
		printk("");
		realTimeVal = (*pAddress);
		if (realTimeVal & pollVal)
		{
			stopPoll = 1;
		}
		else
		{
		}
	}
	if (loopCount < maxPollCount)
	{
		retVal = 0;
	}
	else
	{
		retVal = -1;
	}

	return (retVal);
}
int pollCompareValue(uint32_t *pAddress, uint32_t *qAddress, uint32_t maxPollCount)
{
	int retVal = 0;
	uint32_t loopCount = 0;
	uint32_t stopPoll = 0;
	uint32_t realTimeVal = 0;
	uint32_t setTimeVal = 0;

	for (loopCount = 0; (loopCount < maxPollCount) && (stopPoll == 0); loopCount++)
	{
		printk("");
		setTimeVal = (*pAddress);
		realTimeVal = (*qAddress);
		if ((realTimeVal & setTimeVal) == 0xff)
		{
			stopPoll = 1;
		}
		else
		{
		}
	}
	if (loopCount < maxPollCount)
	{
		retVal = 0;
	}
	else
	{
		retVal = -1;
	}

	return (retVal);
}
