/*
 * crc32.c
 *
 *  Created on: Oct 20, 2015
 *      Author: spark
 */

#include "crc32_table.h"

void crc32_getCrc(const unsigned char *pSrc, unsigned int srcLength,
		unsigned int *pCrcVal)
{
	unsigned int crcState = 0xFFFFFFFF;
	unsigned int offset = 0;
	unsigned char byte = 0;

	for (offset = 0; offset < srcLength; offset++)
	{
		byte = (crcState >> 24) ^ pSrc[offset];
		crcState = crc32Table[byte] ^ (crcState << 8);
	}
	*pCrcVal = (~crcState);
}

void crc32_initCrc(unsigned int *pCrcVal)
{
	*pCrcVal = 0xffffffff;
}

void crc32_updataCrc(unsigned int *pCrcVal, unsigned char *pCharVal)
{
	unsigned char byte = 0;
	byte = (((*pCrcVal) >> 24) ^ (*pCharVal));
	*pCrcVal = ((*pCrcVal) << 8) ^ crc32Table[byte];
}

void crc32_finalCrc(unsigned int *pCrcVal)
{
	*pCrcVal = ~(*pCrcVal);
}

