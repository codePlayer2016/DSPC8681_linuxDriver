#include <linux/ioport.h>
#include <linux/pci.h>
#include <asm/dma-mapping.h>

//#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>

#include "DPUdebug.h"
#include "LinkLayer.h"
#include "DPURegs.h"
#include "pcie.h"
#include "crc32.h"
#include "common.h"

// PC-side write(DSP-side read) buffer status.
#define PC_WT_READY		(0x000055aaU)
#define PC_WT_OVER		(0x55aa0000U)
#define PC_WT_BUSY		(0x55555555U)
// DSP-side read buffer status.
#define DSP_RD_INIT		(0x000055aaU)
#define DSP_RD_READY 	(0x55aa0000U)
#define DSP_RD_OVER 	(0x000055aaU)
#define DSP_RD_BUSY		(0x55555555U)

// PC-side read(DSP-side write) buffer status.
#define PC_RD_INIT		(0xaa000055U)
#define PC_RD_READY		(0x550000aaU)
#define PC_RD_OVER		(0xaa000055U)
#define PC_RD_BUSY		(0x55555555U)
// DSP-side write buffer status.
#define DSP_WT_READY 	(0xaa000055U)
#define DSP_WT_OVER 	(0x550000aaU)
#define DSP_WT_BUSY		(0x55555555U)

extern LinkLayerRegisterTable *gpRegisterTable;

int LinkLayer_Open(LinkLayerHandler **ppHandle, struct pci_dev *pPciDev,
		pcieBarReg_t *pPcieBarReg, struct semaphore *pWriteSemaphore)
{
	LinkLayerHandler *pHandle = NULL;
	uint32_t *regVirt = pPcieBarReg->regVirt;
	int retValue = 0;
	int retPollVal = -1;

	uint32_t addrMap2DSPPCIE = 0;

//  malloc for the LinkLayerHandle and init it.
	pHandle = (LinkLayerHandler *) kmalloc(sizeof(LinkLayerHandler),
			GFP_KERNEL);
	if (pHandle != NULL)
	{
		pHandle->pRegisterTable = gpRegisterTable;
		pHandle->outBufferLength = WTBUFLENGTH;
		pHandle->inBufferLength = RDBUFLENGTH;
		pHandle->pOutBuffer = (uint32_t *) ((uint8_t *) gpRegisterTable
				+ sizeof(LinkLayerRegisterTable));
		pHandle->pInBuffer = (uint32_t *) ((uint8_t *) (pHandle->pOutBuffer)
				+ (pHandle->outBufferLength));
		pHandle->pReadConfirmReg = (uint32_t *) ((uint8_t *) regVirt
				+ LEGACY_A_IRQ_STATUS_RAW);
		pHandle->pWriteConfirmReg = (uint32_t *) ((uint8_t *) regVirt
				+ LEGACY_B_IRQ_STATUS_RAW);

		*ppHandle = pHandle;
	}
	else
	{
		debug_printf("malloc for LinkLayerHandler error\n");
		retValue = -1;
		return (retValue);
	}

// polling the
#if 0
	// TODO: syn with the DSP alorgrithm	and there need some code in the dsp code[alogrithm]

	if (retPollVal == 0)
	{
		retPollVal = pollValue(
				(uint32_t *) (pRegisterTable->xxxx), 0xxxxxxxxx,
				0x7fffffff);
		if (retPollVal == 0)
		{
			printk("LHS in %s DSP get DSPImg successful \n", __func__);
		}
		else
		{
			printk("LHS in %s DSP get DSPImg failed:DSPGetCodeStatus=%x\n",
					__func__,
					*(uint32_t *) (pRegisterTable->DSPCodeCrcStatus));
		}
	}
	else
	{
	}
#endif
	debug_printf("LinkLayer_Open is successful\n");
	return (retValue);
}

int LinkLayer_WaitBufferReady(LinkLayerHandler *pHandle,
		LINKLAYER_IO_TYPE ioType, uint32_t pendtime)
{
	int retValue = 1;
	int pollCount = 0;
	uint32_t *pBufferStatus = NULL;
	uint32_t readyValue = 0;

	if (ioType == LINKLAYER_IO_READ)
	{
		// wait dsp write over.PC can read.
		pBufferStatus = (uint32_t *) &(pHandle->pRegisterTable->readStatus);
		readyValue = PC_RD_READY;
	}
	else
	{
		// PC wait dsp read zone empty.so PC can write.
		pBufferStatus = (uint32_t *) &(pHandle->pRegisterTable->writeStatus);
		readyValue = PC_WT_READY;
	}
	retValue = pollValue(pBufferStatus, readyValue, pendtime);
	if (retValue == 0)
	{
		if (ioType == LINKLAYER_IO_READ)
		{
			debug_printf("read buffer ready\n");
		}
		else
		{
			debug_printf("write buffer ready\n");
		}
	}
	else
	{
		if (ioType == LINKLAYER_IO_READ)
		{
			debug_printf("wait read buffer ready time out\n");
		}
		else
		{
			debug_printf("wait write buffer ready time out\n");
		}
	}
	return (retValue);
}

int LinkLayer_Confirm(LinkLayerHandler *pHandle, LINKLAYER_IO_TYPE ioType)
{
	int retValue = 0;

	if (ioType == LINKLAYER_IO_READ)
	{
		pHandle->pReadConfirmReg = 1;
	}
	else
	{
		pHandle->pWriteConfirmReg = 1;
	}

	return (retValue);
}

int LinkLayer_ChangeBufferStatus(LinkLayerHandler *pHandle,
		LINKLAYER_IO_TYPE ioType)
{
	int retValue = 0;

	debug_printf("ioType=%d\n", ioType);
	if (ioType == LINKLAYER_IO_READ)
	{	// dsp can write.
		pHandle->pRegisterTable->readControl = PC_RD_READY;
	}
	if (ioType == LINKLAYER_IO_WRITE)
	{
		// PC set the register so dsp can read.
		pHandle->pRegisterTable->writeControl = PC_WT_OVER;
		debug_printf("change the writestatus in the dsp\n");
	}
	if (ioType == LINKLAYER_IO_READ_FIN)
	{
		//pHandle->pRegisterTable->writeControl = PC_WT_OVER;
	}
	if (ioType == LINKLAYER_IO_WRITE_FIN)
	{
		// PC set the register so dsp can't read.polling
		// TODO: change the PC_WT_READY to a meaning name.
		pHandle->pRegisterTable->writeControl = PC_WT_READY;
	}

	return (retValue);
}
