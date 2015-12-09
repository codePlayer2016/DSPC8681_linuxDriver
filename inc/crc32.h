/*
 * crc32.h
 *
 *  Created on: Oct 20, 2015
 *      Author: spark
 */

#ifndef INC_CRC32_H_
#define INC_CRC32_H_

void crc32_getCrc(const unsigned char *pSrc, unsigned int srcLength,
		unsigned int *pCrcVal);

void crc32_initCrc(unsigned int *pCrcVal);

void crc32_updateCrc(unsigned int *pCrcVal, unsigned char *pCharVal);

void crc32_finalCrc(unsigned int *pCrcVal);

#endif /* INC_CRC32_H_ */
