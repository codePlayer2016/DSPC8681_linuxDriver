/*
 * bootLoader.c
 *
 *  Created on: Nov 4, 2015
 *      Author: spark
 */
//#include <stdint.h>
#include <linux/kernel.h>
#include <asm/cacheflush.h>
#include "DPUdebug.h"
#include "common.h"
#include "bootLoader.h"
#include "DPURegs.h"
#include "DSP_TBL_6678.h"
#include "dspCodeImg.h"

#define OB_MASK_ONE 	(0xFFF00000)  	// 20-31
#define OB_MASK_TWO 	(0xFFE00000)  	// 21-31
#define OB_MASK_FOUR 	(0xFFC00000) 	// 22-31
#define OB_MASK_EIGHT 	(0xFF800000)	// 23-31

#define DMA_TRANSFER_SIZE            (0x400000U)
registerTable *gpRegisterTable = NULL;
//
// small tools.
//

uint32_t writeDSPMemory(pcieBarReg_t *pPcieBarReg, uint32_t coreNum,
		uint32_t dspMemAddr, uint32_t *buffer, uint32_t length)
{
	uint32_t i, offset, tempReg = 0;
	uint32_t *ptr;

	uint32_t *ddrVirt = pPcieBarReg->ddrVirt;
	uint32_t *msmcVirt = pPcieBarReg->msmcVirt;
	uint32_t *regVirt = pPcieBarReg->regVirt;
	uint32_t *memVirt = pPcieBarReg->memVirt;

	if (length == 0)
	{
		return 0;
	}

	switch (coreNum)
	{
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	{
		dspMemAddr &= 0x00FFFFFF;
		tempReg = ioread32(regVirt + IB_OFFSET(1) / 4);
		iowrite32(tempReg + coreNum * 0x01000000, regVirt + IB_OFFSET(1) / 4); /* pointing to a different core */

		if (dspMemAddr < LL2_START)
		{
			return 0;
		}
		else
		{
			offset = dspMemAddr - LL2_START;
			ptr = (uint32_t *) memVirt + offset / 4;
		}
		break;
	}
	case 8: /* this is for MSMC */
		if (dspMemAddr < MSMC_START)
		{
			return 0;
		}
		else
		{
			offset = dspMemAddr - MSMC_START;
			ptr = (uint32_t *) msmcVirt + offset / 4;
		}
		break;
	case 9: /* this is for DDR */
		if (dspMemAddr < DDR_START)
		{
			return 0;
		}
		else
		{
			offset = dspMemAddr - DDR_START;
			ptr = (uint32_t *) ddrVirt + offset / 4;
		}
		break;
	default:
		printk(
				"Use coreNum 0-7 for core 0-7 of EVMC6678L, or coreNum 0-3 for core 0-3 of EVMC6670L; coreNum 8 for MSMC and coreNum 9 for DDR.\n");
		return 0;
		break;
	}

	for (i = 0; i < length / 4; i++)
	{
		*ptr = buffer[i];
		ptr++;
	}

	if ((coreNum >= 0) && (coreNum <= 7))
	{
		iowrite32(tempReg, regVirt + IB_OFFSET(1) / 4); /* Restore IB_OFFSET1 */
	}
	else
	{

	}

	return length;
}

int uploadProgram(pcieBarReg_t *pPcieBarReg, uint8_t *pDspImgArray,
		uint8_t coreNum)
{
	int retValue = 0;

	uint8_t *pDspImg = pDspImgArray;
	uint32_t i, j, tempArray[BLOCK_TRANSFER_SIZE / 4];
	uint32_t secSize = 0, totalSize = 0;
	uint32_t count = 0, remainder = 0, secStartAddr = 0, temp = 0;
	int secNum = 0;
	uint8_t newCoreNum = 0;
	uint32_t bootEntryAddr = 0;

	uint32_t checkAddr = 0;

	// Get the boot entry address
	bootEntryAddr = byteTo32bits(pDspImg);
	pDspImg += 4;

	// Get the sect secSize
	secSize = byteTo32bits(pDspImg);
	pDspImg += 4;
	for (secNum = 0; secSize != 0; secNum++)
	{
		// adjusts the secSize.
		if ((secSize / 4) * 4 != secSize)
		{
			secSize = ((secSize / 4) + 1) * 4;
		}
		totalSize += secSize;

		// Gets the section start address.
		secStartAddr = byteTo32bits(pDspImg);
		pDspImg += 4;

		/* In case there are several sections within different memory regions */
		temp = (secStartAddr & 0xFF000000) >> 24;
		checkAddr = secStartAddr;

		if (temp == 0x00 || ((temp >> 4) == 0x1))
		{
			if (coreNum < 8)
			{
				/* Write address like 0x00800000 to core 1, 2, ... */
				newCoreNum = coreNum;
			}
			else
			{
				newCoreNum = 0;
			}
		}
		else if (temp == 0x0C)
		{
			newCoreNum = 8;
		}
		else
		{
			newCoreNum = 9;
		}

		//transfer the code to DSP.
		count = secSize / BLOCK_TRANSFER_SIZE;
		remainder = secSize - count * BLOCK_TRANSFER_SIZE;
		for (i = 0; i < count; i++)
		{
			for (j = 0; j < BLOCK_TRANSFER_SIZE / 4; j++)
			{
				tempArray[j] = byteTo32bits(pDspImg);
				pDspImg += 4;
			}
			/* Transfer boot tables to DSP */
			writeDSPMemory(pPcieBarReg, newCoreNum, secStartAddr, tempArray,
			BLOCK_TRANSFER_SIZE);
			secStartAddr += BLOCK_TRANSFER_SIZE;
		}

		for (j = 0; j < remainder / 4; j++)
		{
			tempArray[j] = byteTo32bits(pDspImg);
			pDspImg += 4;
		}
		writeDSPMemory(pPcieBarReg, newCoreNum, secStartAddr, tempArray,
				remainder);

		// Get the next sect secSize
		secSize = byteTo32bits(pDspImg);
		pDspImg += 4;
	} // for

#if 1
	retValue = writeDSPMemory(pPcieBarReg, 0, MAGIC_ADDR, &bootEntryAddr, 4);
	if (retValue == 4)
	{
		retValue = 0;
	}
	else
	{
		retValue = -1;
	}
#endif
	printk(
			"Section %d started at %x with secSize %x bytes written to core %d\n",
			secNum, secStartAddr, secSize, newCoreNum);
	return (retValue);
}

int dspMemoryMap(uint32_t *pRegVirt, uint32_t pcAddr, uint32_t size)
{
	int retValue = 0;
	uint32_t tmp, pageBase, i;
	uint32_t *pReg = (uint32_t *) pRegVirt;
	uint32_t obSize = 0;
	uint32_t baseMask = 0;
	// TODO: convert to 64 bit machine,will change this code.

	if (size <= 0x100000)
	{
		obSize = 0x0;
		baseMask = OB_MASK_ONE;
	}
	else if (size <= 0x200000)
	{
		obSize = 0x1;
		baseMask = OB_MASK_TWO;
	}
	else if (size <= 0x400000)
	{
		obSize = 0x2;
		baseMask = OB_MASK_FOUR;
	}
	else if (size <= 0x800000)
	{
		obSize = 0x3;
		baseMask = OB_MASK_EIGHT;
	}
	else
	{
		retValue = -1;
	}

	if (retValue == 0)
	{
		iowrite32(obSize, pReg + OB_SIZE / 4);
		pageBase = (pcAddr & baseMask);
		iowrite32(pageBase | 0x1, pReg + OB_OFFSET_INDEX(0) / 4);
		iowrite32(0x0, pReg + OB_OFFSET_HI(0) / 4);
	}
	else
	{
	}
	return (retValue);
}
/**
 *  @brief Function bootLoader() put DSP code to C6678 and bootLoad it by PCIE.
 *  	put code twice,first directly write platform initial code to L2 and run
 *  	second push the app DSP code to 0x60000000+2*1024*4 and run.
 *
 *  @param[in] pPcieDev 	the pci device handler.
 *  @param[in] pPcieBarReg		self-defined pcie bar structure data point.
 *  @retval 0 : success, -1 : platform init fail, -2 : DSP receive code fail, -3 : crc check fail in the DSP-side.
 */
int bootLoader(struct pci_dev *pPciDev, pcieBarReg_t *pPcieBarReg)
{

	registerTable *pRegisterTable = NULL;
	uint8_t *DMAVirAddr = NULL;
	dma_addr_t DMAPhyAddr = 0;
	uint32_t *pPutDSPImgZone = 0;
	uint32_t *regVirt = pPcieBarReg->regVirt;
	int retValue = 0;
	int retPollVal = -1;

	uint32_t memOffset = 0;
	int retMapVal = 0;
	uint32_t alignPhyAddr = 0;
	uint32_t addrMap2DSPPCIE = 0;
	uint32_t registerLength = REG_LEN;

// alloc DMA zone.
	DMAVirAddr = (uint8_t*) dma_alloc_coherent(&pPciDev->dev, DMA_TRANSFER_SIZE,
			&DMAPhyAddr, GFP_KERNEL);

// because of PCIE Outbound windows size is 1M.
	alignPhyAddr = ((DMAPhyAddr + 0x100000) & (~0x100000));
	debug_printf("the dma phy addr is 0x%x\n", alignPhyAddr);
	memOffset = (alignPhyAddr - DMAPhyAddr);
	addrMap2DSPPCIE = (uint32_t)(DMAVirAddr + memOffset);

	pRegisterTable = (registerTable *) addrMap2DSPPCIE;
	gpRegisterTable = pRegisterTable;
//pRegisterTable->registerPhyAddrInPc = alignPhyAddr;

	set_memory_ro(addrMap2DSPPCIE, 1);

	pPutDSPImgZone = (uint32_t *) (addrMap2DSPPCIE + registerLength);

	// TODO: the dspMemoryMap ,the last parameter should be variant.
	retMapVal = dspMemoryMap(regVirt, alignPhyAddr, 0x200000);
	// TODO process the error.
	if(retMapVal!=0)
	{}
	else
	{}

// pushs DSPInit code.
	retValue = uploadProgram(pPcieBarReg, _thirdBLCode, 0);

// waits DSPInitReady.
	if (retValue == 0)
	{
		retPollVal = pollValue(&(pRegisterTable->DPUBootStatus), DSP_INIT_READY,
				0x7fffffff);
	}
	else
	{
		retValue = -1;
		debug_printf("DPUBootStatus = %x \n", pRegisterTable->DPUBootStatus);
		return (retValue);
	}

// pushs the data to the DSP.
	if (retPollVal == 0)
	{
		uint32_t DPUCoreLength = ((sizeof(_DPUCore) + 3) / 4) * 4;

		debug_printf("the CodeLength = %x \n", DPUCoreLength);

		memcpy(pPutDSPImgZone, _DPUCore, DPUCoreLength);

		(pRegisterTable->DPUBootControl) = PC_PUSHCODE_FINISH;
	}
	else
	{
	}

//wait dsp get the code.
	if (retPollVal == 0)
	{
		retPollVal = pollValue(&(pRegisterTable->DPUBootStatus),
		DSP_GETCODE_FINISH, 0x7fffffff);
		if (retPollVal == 0)
		{
			debug_printf("DSP get DSPImg successful \n");
		}
		else
		{
			retValue = -2;
			debug_printf("DPUBootStatus=%x\n", (pRegisterTable->DPUBootStatus));
			return (retValue);
		}
	}
	else
	{

	}

// wait the dsp crc check result.
	if (retPollVal == 0)
	{
		retPollVal = pollValue(&(pRegisterTable->DPUBootStatus),
		DSP_CRCCHECK_SUCCESSFUL, 0x00ffffff);
		if (retPollVal == 0)
		{
			debug_printf("DSP check crc successful \n");
		}
		else
		{
			retValue = -3;
			debug_printf("failed DPUBootStatus=%x\n",
					(pRegisterTable->DPUBootStatus));
			return (retValue);
		}
	}
	else
	{
	}

// wait the dsp jump to the dpm code.
	if (retPollVal == 0)
	{
		retPollVal = pollValue(&(pRegisterTable->DPUBootStatus),
		DSP_GETENTRY_FINISH, 0x00ffffff);
		if (retPollVal == 0)
		{
			debug_printf("DSP jump to the dpm code successful \n");
		}
		else
		{
			retValue = -4;
			debug_printf("DPUBootStatus=%x\n", (pRegisterTable->DPUBootStatus));
			return (retValue);
		}
	}
	else
	{
	}

	debug_printf("LINKLAYER_Open end,DPUBootStatus=%x\n",
			pRegisterTable->DPUBootStatus);

	return (retValue);
}
