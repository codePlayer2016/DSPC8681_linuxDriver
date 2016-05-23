#ifndef _INC_DPUDRIVER_H
#define _INC_DPUDRIVER_H

#include <linux/ioctl.h>

#define FIND_PCIEDEV_MAXCOUNT (255)

#define IMG_SIZE4_DSP (0x100000)
#define PCIEDRIVER_MAGIC ('x')
//#define DPU_IO_CMD_RUNDSP _IOW(PCIEDRIVER_MAGIC,0x101,int*)
//#define DPU_IO_CMD_SYNC _IO(PCIEDRIVER_MAGIC,0x102)
#define DPU_IO_CMD_WAITBUFFERREADY _IOWR(PCIEDRIVER_MAGIC,0x103,DPUDriver_WaitBufferReadyParam)
#define DPU_IO_CMD_CONFIRM _IOW(PCIEDRIVER_MAGIC,0x104,LINKLAYER_IO_TYPE)
#define DPU_IO_CMD_CHANGEBUFFERSTATUS _IOW(PCIEDRIVER_MAGIC,0x105,LINKLAYER_IO_TYPE)

#define DPU_IO_CMD_INTERRUPT _IOWR(PCIEDRIVER_MAGIC,0x106,interruptAndPollParam)
#define DPU_IO_CMD_WAITDPM _IOWR(PCIEDRIVER_MAGIC,0x107,interruptAndPollParam)

#define DPU_IO_CMD_WRITE_TIMEOUT _IOWR(PCIEDRIVER_MAGIC,0x108,interruptAndPollParam)
#define DPU_IO_CMD_READ_TIMEOUT _IOWR(PCIEDRIVER_MAGIC,0x109,interruptAndPollParam)
#define DPU_IO_CMD_WAITDPMSTART _IOWR(PCIEDRIVER_MAGIC,0x110,DPUDriver_WaitBufferReadyParam)

#define DPU_IO_CMD_CHANGEREG _IOW(PCIEDRIVER_MAGIC,0x111,LINKLAYER_IO_TYPE)
#define DPU_IO_CMD_CHANGOVERREG _IOWR(PCIEDRIVER_MAGIC,0x112,LINKLAYER_IO_TYPE)

#define DPU_IO_CMD_CHECKDPMALLOVER _IOWR(PCIEDRIVER_MAGIC,0x113,DPUDriver_WaitBufferReadyParam)

#define DEV_NAME "DPU_driver_linux"

struct DPU_dev
{
	struct cdev cdev;
};

typedef struct _tagDPUDriver_WaitBufferReadyParam
{
	int waitType;
	uint32_t pendTime;
	int32_t *pBufStatus;
} DPUDriver_WaitBufferReadyParam, *DPUDriver_WaitBufferReadyParamPtr;

typedef struct _tagInterruptAndPollParam
{
	int interruptAndPollDirect;
	int interruptAndPollResult;
	uint32_t pollTime;
} interruptAndPollParam, *pInterruptAndPollParam;

int init_module(void);
void cleanup_module(void);

#endif // _INC_DPUDRIVER_H
