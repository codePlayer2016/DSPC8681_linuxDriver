#ifndef _INC_LINKLAYER_H_
#define _INC_LINKLAYER_H_

#include <linux/types.h>
#include <linux/semaphore.h>
#include "pcie.h"

/********************PC write to DSP.*/
// DSP-side read buffer status.(pc polling)
#define DSP_RD_RESET	(0x000055aaU)
#define DSP_RD_FINISH 	(0x550000aaU)
// PC-side write buffer status.(pc set)
#define PC_WT_RESET 	(0x0000aa55U)
#define PC_WT_FINISH 	(0xaa000055U)
/*************************************/

/********************DSP write to PC.*/
// PC-side read buffer status.(pc set)
#define PC_RD_RESET		(0x000077bbU)
#define PC_RD_FINISH	(0x770000bbU)
// DSP-side write buffer status.(pc polling)
#define DSP_WT_RESET	(0x0000bb77U)
#define DSP_WT_FINISH	(0xbb000077U)
/*************************************/

//dpm start and over flag
#define PC_DPM_STARTSTATUS   (0x0505aa00U)
#define PC_DPM_STARTCLR   (0x0a0a5500U)

#define PC_DPM_OVERSTATUS (0x00a5a500U)
#define PC_DPM_OVERCLR (0x005a5a00U)

#define PC_DPM_ALLOVER (0x00005a5aU)

typedef enum __tagLINKLAYER_IO_TYPE
{
	LINKLAYER_IO_RESERVER = 0,
	LINKLAYER_IO_WRITE_RESET = 1,
	LINKLAYER_IO_WRITE_QRESET = 2,
	LINKLAYER_IO_WRITE_FIN = 3,
	LINKLAYER_IO_WRITE_QFIN = 4,
	LINKLAYER_IO_READ_RESET = 5,
	LINKLAYER_IO_READ_QRESET = 6,
	LINKLAYER_IO_READ_FIN = 7,
	LINKLAYER_IO_READ_QFIN = 8,
} LINKLAYER_IO_TYPE;
// TODO: merge the structure in LinkLayer.h and common.h
#if 0
typedef struct _tagLinkLayerRegisterTable
{
	// status registers. (4k)
	uint32_t DPUBootStatus;
	uint32_t SetMultiCoreBootStatus;
	uint32_t MultiCoreBootStatus;
	uint32_t writeStatus;
	uint32_t readStatus;
	uint32_t getPicNumers;
	uint32_t failPicNumers;
	uint32_t dpmOverStatus;
	uint32_t dpmStartStatus;
	uint32_t dpmAllOverStatus;
	uint32_t reserved0[0x1000 / 4 - 10];

	// control registers. (4k)
	uint32_t DPUBootControl;
	uint32_t writeControl;
	uint32_t readControl;
	uint32_t PC_urlNumsReg;
	uint32_t modelType;//1:motor 2:car 3:person
	uint32_t dpmOverControl;
	uint32_t dpmStartControl;
	uint32_t reserved1[0x1000 / 4 - 6];
}LinkLayerRegisterTable;
#endif

typedef struct _tagLinkLayerRegisterTable
{
	// status registers. (4k)
	uint32_t DPUBootStatus;
	uint32_t SetMultiCoreBootStatus;
	uint32_t MultiCoreBootStatus;
	uint32_t writeStatus;
	uint32_t readStatus;
	uint32_t getPicNumers;
	uint32_t failPicNumers;
	uint32_t dpmOverStatus;
	uint32_t dpmStartStatus;
	uint32_t dpmAllOverStatus;
	uint32_t pushCodeStatus;
	uint32_t reserved0[0x1000/4 - 11];

	// control registers. (4k)
	uint32_t DPUBootControl;
	uint32_t writeControl;
	uint32_t readControl;
	uint32_t PC_urlNumsReg;
	uint32_t modelType; //1:motor 2:car 3:person
	uint32_t dpmOverControl;
	uint32_t dpmStartControl;
	uint32_t pushCodeControl;
	uint32_t reserved1[0x1000/4 - 8];
} LinkLayerRegisterTable;

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

//int LinkLayer_Open(LinkLayerHandler ***ppHandle, struct pci_dev **pPciDev, pcieBarReg_t **pPcieBarReg, struct semaphore **pWriteSemaphore);
int LinkLayer_Open(LinkLayerHandler *pLinkLayerHandle, ProcessorUnitDev_t *pProcessorUnitDev);
void LinkLayer_Close(LinkLayerHandler **ppHandle);
int LinkLayer_WaitBufferReady(LinkLayerHandler *pHandle, LINKLAYER_IO_TYPE ioType, uint32_t pendtime);

//int LinkLayer_Confirm(LinkLayerHandler *pHandle, LINKLAYER_IO_TYPE ioType);

int LinkLayer_ChangeBufferStatus(LinkLayerHandler *pHandle, LINKLAYER_IO_TYPE ioType);
//int LinkLayer_CheckStatus(LinkLayerRegisterTable *gpRegisterTable, int chipIndex);
//int LinkLayer_ChangeDpmReg(LinkLayerHandler *pHandle);
int LinkLayer_ClearInterrupt(LinkLayerHandler *pHandle);
int LinkLayer_WaitDpmOver(LinkLayerHandler *pHandle, uint32_t pendtime);
//int LinkLayer_CheckDpmStatus(LinkLayerHandler *pHandle);

#endif // _INC_LINKLAYER_H_
