#ifndef _INC_LINKLAYER_H_
#define _INC_LINKLAYER_H_

#include <linux/types.h>
#include <linux/semaphore.h>
#include "pcie.h"

typedef enum __tagLINKLAYER_IO_TYPE
{
	LINKLAYER_IO_READ = 0,
	LINKLAYER_IO_WRITE = 1,
	LINKLAYER_IO_READ_FIN = 2,
	LINKLAYER_IO_WRITE_FIN = 3
} LINKLAYER_IO_TYPE;
// TODO: merge the structure in LinkLayer.h and common.h
typedef struct _tagLinkLayerRegisterTable
{
	// status registers. (4k)
	uint32_t DPUBootStatus;
	uint32_t writeStatus;
	uint32_t readStatus;
	uint32_t reserved0[0x1000 / 4 - 3];

	// control registers. (4k)
	uint32_t DPUBootControl;
	uint32_t writeControl;
	uint32_t readControl;
	uint32_t PC_urlNumsReg;
	uint32_t reserved1[0x1000 / 4 - 4];
} LinkLayerRegisterTable;
#if 0
typedef struct _tagRegisterTable
{
	// status registers. (4k)
	uint32_t DPUBootStatus;
	uint32_t writeStatus;
	uint32_t readStatus;
	uint32_t registerPhyAddrInPc;
	uint32_t reserved0[0x1000 / 4 - 4];

	// control registers. (4k)
	uint32_t DPUBootControl;
	uint32_t writeControl;
	uint32_t readControl;
	uint32_t reserved1[0x1000 / 4 - 3];
}registerTable;
#endif
typedef struct _tagLinkLayerHandler
{
	LinkLayerRegisterTable *pRegisterTable;
	uint32_t *pOutBuffer;
	uint32_t *pInBuffer;
	uint32_t outBufferLength;
	uint32_t inBufferLength;

	// try removing global var.
	uint32_t *pWriteConfirmReg;
	uint32_t *pReadConfirmReg;
} LinkLayerHandler, *LinkLayerHandlerPtr;

int LinkLayer_Open(LinkLayerHandler **ppHandle, struct pci_dev *pPciDev,
		pcieBarReg_t *pPcieBarReg, struct semaphore *pWriteSemaphore);

int LinkLayer_WaitBufferReady(LinkLayerHandler *pHandle,
		LINKLAYER_IO_TYPE ioType, uint32_t pendtime);

int LinkLayer_Confirm(LinkLayerHandler *pHandle, LINKLAYER_IO_TYPE ioType);

int LinkLayer_ChangeBufferStatus(LinkLayerHandler *pHandle,
		LINKLAYER_IO_TYPE ioType);

#endif // _INC_LINKLAYER_H_
