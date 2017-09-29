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

extern LinkLayerRegisterTable *gpRegisterTable;
extern struct semaphore readSemaphore;
extern struct semaphore writeSemaphore;
extern struct semaphore gDspDpmOverSemaphore;

int LinkLayer_Open(LinkLayerHandler **ppHandle, struct pci_dev *pPciDev, pcieBarReg_t *pPcieBarReg, struct semaphore *pWriteSemaphore)
{
	LinkLayerHandler *pHandle = NULL;
	uint32_t *regVirt = pPcieBarReg->regVirt;
	int retValue = 0;
	int retPollVal = -1;

	uint32_t addrMap2DSPPCIE = 0;

//  malloc for the LinkLayerHandle and init it.
	pHandle = (LinkLayerHandler *) kmalloc(sizeof(LinkLayerHandler), GFP_KERNEL);
	if (pHandle != NULL)
	{
		pHandle->pRegisterTable = gpRegisterTable;
		pHandle->outBufferLength = WTBUFLENGTH;
		pHandle->inBufferLength = RDBUFLENGTH;
		pHandle->pOutBuffer = (uint32_t *) ((uint8_t *) gpRegisterTable + sizeof(LinkLayerRegisterTable));
		pHandle->pInBuffer = (uint32_t *) ((uint8_t *) (pHandle->pOutBuffer) + (pHandle->outBufferLength));
		pHandle->pReadConfirmReg = (uint32_t *) ((uint8_t *) regVirt + LEGACY_A_IRQ_STATUS_RAW);
		pHandle->pWriteConfirmReg = (uint32_t *) ((uint8_t *) regVirt + LEGACY_B_IRQ_STATUS_RAW);

		*ppHandle = pHandle;
	}
	else
	{
		debug_printf("malloc for LinkLayerHandler error\n");
		retValue = -1;
		return (retValue);
	}

	debug_printf("LinkLayer_Open is successful\n");
	return (retValue);
}

int LinkLayer_WaitBufferReady(LinkLayerHandler *pHandle, LINKLAYER_IO_TYPE ioType, uint32_t pendtime)
{
#if 0
	int retValue = 1;
	int pollCount = 0;
	uint32_t *pBufferStatus = NULL;
	uint32_t readyValue = 0;

	// PC polling for read.  wait the DSP to wirte finshed.
	if (ioType == LINKLAYER_IO_READ)
	{
		// wait dsp write over.PC can read.
		pBufferStatus = (uint32_t *) &(pHandle->pRegisterTable->readStatus);
		readyValue = PC_WAIT_RD;
	}
	if (ioType == LINKLAYER_IO_WRITE)
	{
		// PC wait dsp read zone empty.so PC can write.
		pBufferStatus = (uint32_t *) &(pHandle->pRegisterTable->writeStatus);
		readyValue = PC_WAIT_WT;
	}
	if (ioType == LINKLAYER_IO_START)
	{
		// wait dsp write over.PC can read.
		pBufferStatus = (uint32_t *) &(pHandle->pRegisterTable->dpmStartStatus);
		readyValue = PC_DPM_STARTSTATUS;
	}
	if (ioType == LINKLAYER_IO_OVER)
	{
		// PC wait dsp read zone empty.so PC can write.
		pBufferStatus = (uint32_t *) &(pHandle->pRegisterTable->dpmOverStatus);
		readyValue = PC_DPM_OVERSTATUS;
	}
	retValue = pollValue(pBufferStatus, readyValue, pendtime);
	if (retValue == 0)
	{
		if (ioType == LINKLAYER_IO_READ)
		{
			debug_printf("read buffer ready\n");
		}
		if (ioType == LINKLAYER_IO_WRITE)
		{
			debug_printf("write buffer ready\n");
		}
		if (ioType == LINKLAYER_IO_START)
		{
			debug_printf("DPM start ready\n");
		}
		if (ioType == LINKLAYER_IO_OVER)
		{
			debug_printf("dpm over\n");
		}
	}
	else
	{
		if (ioType == LINKLAYER_IO_READ)
		{
			debug_printf("read buffer ready timeout\n");
		}
		if (ioType == LINKLAYER_IO_WRITE)
		{
			debug_printf("write buffer ready timeout\n");
		}
		if (ioType == LINKLAYER_IO_START)
		{
			debug_printf("DPM start ready timeout\n");
		}
		if (ioType == LINKLAYER_IO_OVER)
		{
			debug_printf("dpm over timeout\n");
		}
	}
	//return (retValue);
#endif
	///////////////////////////////////////////////////////////////////////
	int retValue = 0;
	int pollCount = 0;
	uint32_t *pBufferStatus = NULL;
	uint32_t readyValue = 0;
	if (ioType == LINKLAYER_IO_START)
	{
		// wait dsp write over.PC can read.
		pBufferStatus = (uint32_t *) &(pHandle->pRegisterTable->dpmStartStatus);
		readyValue = PC_DPM_STARTSTATUS;
	}
	//int retValue = 1;
	//int pollCount = 0;
	//uint32_t *pBufferStatus = NULL;
	//uint32_t readyValue = 0;
	//debug_printf("ioType=%d\n", ioType);
	/**********************PC Write***************************/
	// PC polling for new write.
	if (ioType == LINKLAYER_IO_WRITE_QRESET)
	{
		pBufferStatus = (uint32_t *) &(pHandle->pRegisterTable->writeStatus);
		readyValue = DSP_RD_RESET;
	}
	// PC polling for dsp read finished.
	if (ioType == LINKLAYER_IO_WRITE_QFIN)
	{
		pBufferStatus = (uint32_t *) &(pHandle->pRegisterTable->writeStatus);
		readyValue = DSP_RD_FINISH;
	}
	/**********************PC read***************************/
	// pc polling for new read start.
	if (ioType == LINKLAYER_IO_READ_QRESET)
	{
		pBufferStatus = (uint32_t *) &(pHandle->pRegisterTable->readStatus);
		readyValue = DSP_WT_RESET;
	}
	// PC polling for read.  wait the DSP to wirte finshed.
	if (ioType == LINKLAYER_IO_READ_QFIN)
	{
		pBufferStatus = (uint32_t *) &(pHandle->pRegisterTable->readStatus);
		readyValue = DSP_WT_FINISH;
	}

	//retValue = pollValue(pBufferStatus, readyValue, pendtime);
	retValue = pollEqualValue(pBufferStatus, readyValue, pendtime);
	if (retValue == 0)
	{
		if (ioType == LINKLAYER_IO_WRITE_QRESET)
		{
			debug_printf("pc can write to dsp\n");
		}
		if (ioType == LINKLAYER_IO_WRITE_QFIN)
		{
			debug_printf("pc write and dsp read finished\n");
		}
		if (ioType == LINKLAYER_IO_READ_QRESET)
		{
			debug_printf("pc can't read before dsp write\n");
		}
		if (ioType == LINKLAYER_IO_READ_QFIN)
		{
			debug_printf("pc can read\n");
		}
		if(ioType==LINKLAYER_IO_START)
		{
			debug_printf("dpm in dsp is ready\n");
		}
	}
	else
	{
		if (ioType == LINKLAYER_IO_WRITE_QRESET)
		{
			debug_printf("timeout:pc can write to dsp\n");
		}
		if (ioType == LINKLAYER_IO_WRITE_QFIN)
		{
			debug_printf("timeout:pc write and dsp read finished\n");
		}
		if (ioType == LINKLAYER_IO_READ_QRESET)
		{
			debug_printf("timeout:pc can't read before dsp write\n");
		}
		if (ioType == LINKLAYER_IO_READ_QFIN)
		{
			debug_printf("timeout:pc can read\n");
		}
		if(ioType==LINKLAYER_IO_START)
		{
			debug_printf("timeout:dpm in dsp is not ready\n");
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

int LinkLayer_ChangeBufferStatus(LinkLayerHandler *pHandle, LINKLAYER_IO_TYPE ioType)
{
	int retValue = 0;
#if 0
	// TODO: change to switch-case style.
	debug_printf("ioType=%d\n", ioType);
	if (ioType == LINKLAYER_IO_READ)
	{
		pHandle->pRegisterTable->readControl = PC_RD_ENABLE;
	}
	if (ioType == LINKLAYER_IO_WRITE)
	{
		// PC set the register so dsp can read.
		pHandle->pRegisterTable->writeControl = PC_WT_OVER;
		debug_printf("change the writestatus in the dsp,writeNumer=%x\n", (pHandle->pRegisterTable->writeControl));
		//5.13
		pHandle->pRegisterTable->dpmStartControl = 0x00000000;
	}
	if (ioType == LINKLAYER_IO_READ_FIN)
	{
		// PC read the DSP process result. and dsp need to polling.
		pHandle->pRegisterTable->readControl = PC_RD_OVER;
	}
	if (ioType == LINKLAYER_IO_WRITE_FIN)
	{
		// PC set the register so dsp can't read.polling
		// TODO: change the PC_WT_READY to a meaning name.
		pHandle->pRegisterTable->writeControl = PC_WT_READY;

	}
	if (ioType == LINKLAYER_IO_START)
	{
		// PC read the DSP process result. and dsp need to polling.
		pHandle->pRegisterTable->dpmStartControl = PC_DPM_STARTCLR;
	}
	if (ioType == LINKLAYER_IO_OVER)
	{
		// PC set the register so dsp can't read.polling
		// TODO: change the PC_WT_READY to a meaning name.
		debug_printf("%%%%%%pHandle->pRegisterTable->dpmOverControl %x\n", pHandle->pRegisterTable->dpmOverControl);
		pHandle->pRegisterTable->dpmOverControl = PC_DPM_OVERCLR;
		//2016.5.20
		pHandle->pRegisterTable->dpmStartControl = 0x00000000;
	}
#endif
////////////////////////////////////////////////////////////////////////////////
	/************************pc write************************/
	if (ioType == LINKLAYER_IO_WRITE_RESET)
	{
		// PC writeBuffer reset.DSP can't read.
		pHandle->pRegisterTable->writeControl = PC_WT_RESET;

	}
	if (ioType == LINKLAYER_IO_WRITE_FIN)
	{
		// PC writeBuffer reset.DSP can't read.
		pHandle->pRegisterTable->writeControl = PC_WT_FINISH;
#if 1
		debug_printf("urlItemNum=%d\n", pHandle->pRegisterTable->PC_urlNumsReg);
		//debug_printf("urs=%s\n",pHandle->pRegisterTable->PC_urlNumsReg);
#endif

	}
	/************************pc read************************/
	if (ioType == LINKLAYER_IO_READ_RESET)
	{
		pHandle->pRegisterTable->readControl = PC_RD_RESET;
	}
	if (ioType == LINKLAYER_IO_READ_FIN)
	{
		// PC set the register so dsp can read.
		pHandle->pRegisterTable->readControl = PC_RD_FINISH;
	}
	return (retValue);
}

int LinkLayer_CheckStatus(LinkLayerRegisterTable *gpRegisterTable)
{
	int retValue = 0;

	// dsp readctl is set init means:dsp receive buffer is empty. so pc can write to send buffer.
	if ((gpRegisterTable->writeStatus) & PC_WAIT_WT)
	{
		up(&writeSemaphore);
	}
	if ((gpRegisterTable->readStatus) & PC_WAIT_RD)

	{
		up(&readSemaphore);
	}
	if ((gpRegisterTable->dpmOverStatus) & PC_DPM_OVERSTATUS)

	{
		up(&gDspDpmOverSemaphore);
		//clear reg
		gpRegisterTable->dpmOverControl |= 0x00000000;
	}

//	if ((pHandle->pRegisterTable->writeStatus) & PC_WAIT_WT)
//	{
//		up(&writeSemaphore);
//	}
//	if ((pHandle->pRegisterTable->readStatus) & PC_WAIT_RD)
//
//	{
//		up(&readSemaphore);
//	}
//	if ((pHandle->pRegisterTable->dpmOverStatus) & PC_DPM_OVERSTATUS)
//
//	{
//		up(&gDspDpmOverSemaphore);
//		//clear reg
//		pHandle->pRegisterTable->dpmOverControl |= 0x00000000;
//	}

	return retValue;
}
int LinkLayer_CheckDpmStatus(LinkLayerHandler *pHandle)
{
	if ((pHandle->pRegisterTable->dpmAllOverStatus) & PC_DPM_ALLOVER)
	{
		debug_printf("pc have check dpm all over\n");
		return 0;
	}
	else
	{
		return -1;
	}
}

