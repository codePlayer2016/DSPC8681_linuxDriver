#include <linux/ioport.h>
#include <linux/pci.h>
#include <asm/dma-mapping.h>

//#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include "LinkLayer.h"

//#include "DSP_TBL_6678.h"
#include "DPURegs.h"
//#include "dspCodeImg.h"
#include "pcie.h"
#include "crc32.h"
#include "common.h"



int LinkLayer_Open(LinkLayerHandler **ppHandle, struct pci_dev *pPciDev,
		pcieBarReg_t *pPcieBarReg, struct semaphore *pWriteSemaphore)
{
	LinkLayerHandler *pHandle = NULL;
	uint32_t *regVirt = pPcieBarReg->regVirt;
	int retValue = 0;
	int retPollVal = -1;

	uint32_t addrMap2DSPPCIE = 0;
	/*
	 pHandle = (LinkLayerHandler *) kmalloc(sizeof(LinkLayerHandler),
	 GFP_KERNEL);
	 if (pHandle != NULL)
	 {
	 pHandle->pRegisterTable = pRegisterTable;
	 pHandle->outBufferLength = WTBUFLENGTH;
	 pHandle->inBufferLength = RDBUFLENGTH;
	 pHandle->pOutBuffer = (uint32_t *) ((uint8_t *) pRegisterTable
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
	 retValue = -1;
	 }

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
	 */
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
		pBufferStatus = (uint32_t *) &(pHandle->pRegisterTable->readStatus);
		readyValue = INBUF_READY;
	}
	else
	{
		pBufferStatus = (uint32_t *) &(pHandle->pRegisterTable->writeStatus);
		readyValue = OUTBUF_READY;
	}

	for (pollCount = 0; retValue == 1; pollCount++)
	{
		retValue = ((*pBufferStatus != readyValue) && (pollCount < pendtime));
		printk("");
	}

	if (pollCount < pendtime)
	{
		retValue = 0;
	}
	else
	{
		printk("<%s>:time out\n", __func__);
		retValue = -1;
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

