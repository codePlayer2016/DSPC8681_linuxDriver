/*
 * common.h
 *
 *  Created on: Nov 4, 2015
 *      Author: spark
 */

#ifndef INC_COMMON_H_
#define INC_COMMON_H_

//#include <stdint.h>
#include <linux/pci.h>

#if __WORDSIZE == 64
# ifndef __intptr_t_defined
typedef long int intptr_t;
#  define __intptr_t_defined
# endif
#else
# ifndef __intptr_t_defined
typedef int intptr_t;
#  define __intptr_t_defined
# endif
#endif

typedef struct __tagPcieBarReg
{
	uint32_t *regVirt;
	uint32_t *memVirt;
	uint32_t *msmcVirt;
	uint32_t *ddrVirt;

	resource_size_t regBase;
	resource_size_t memBase;
	resource_size_t msmcBase;
	resource_size_t ddrBase;

	resource_size_t memLen;
	resource_size_t regLen;
	resource_size_t msmcLen;
	resource_size_t ddrLen;
} pcieBarReg_t;

// TODO: rename the element:PC_bootStatusReg,PC_writeStatusReg...,PC_bootCtlReg.
/*
typedef struct _tagRegisterTable
{
	// status registers. (4k)
	uint32_t DPUBootStatus;
	uint32_t writeStatus;
	uint32_t readStatus;
	uint32_t getPicNumers;
	uint32_t failPicNumers;
	uint32_t reserved0[0x1000 / 4 - 5];

	// control registers. (4k)
	uint32_t DPUBootControl;
	uint32_t writeControl;
	uint32_t readControl;
	uint32_t PC_urlNumsReg;
	uint32_t reserved1[0x1000 / 4 - 4];
} registerTable;
*/
uint32_t byteTo32bits(uint8_t *pDspCode);

int pollValue(uint32_t *pAddress, uint32_t pollVal, uint32_t maxPollCount);
#endif /* INC_COMMON_H_ */
