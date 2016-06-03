/******************************************************************************
 * Copyright (c) 2011 Texas Instruments Incorporated - http://www.ti.com
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *****************************************************************************/

/**************************************************************************************
 * FILE NAME: pciedemo.c
 *
 * DESCRIPTION: A sample code to load DSP boot image from a Linux machine into DSP.
 *
 * Revision History:
 *    - 1.0:  Initial version (6678 little endian PCIE boot POST demo)
 *    - 1.1:  Add a new PCIE boot demo for HelloWorld
 *    - 1.2:  support 6670 boot
 *    - 1.3:  support big endian boot for 6670/6678; support 32-bit/64-bit Linux;         
 *            support EDMA; support interrupt between host and DSP; 
 *            added PCIE over EDMA throughput measurement
 *    - 1.4:  Add a DSP local reset demo; fix pushData() to handle cases when 
 *            sections within different memory regions in a header file
 *    - 1.5:  Support 6657 boot
 ***************************************************************************************/

#include <linux/module.h>
//#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <asm/dma-mapping.h>
#include <linux/time.h>
#include <linux/semaphore.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/vmalloc.h>

#include "common.h"
#include "LinkLayer.h"
#include "DPUDriver.h"
#include "DPURegs.h"
//#include "LinkLayer.h"
#include "pcie.h"
#include "DPUdebug.h"
#include "bootLoader.h"

MODULE_LICENSE("GPL v2");

/* Must select the endianess */
#define BIG_ENDIAN          0
/* Must select a platform */
#define EVMC6678L           1

#if BIG_ENDIAN
#define myIoread32  ioread32be
#define myIowrite32 iowrite32be
#else
#define myIoread32  ioread32 
#define myIowrite32 iowrite32
#endif

#define DSP_RUN_READY			(0x00010000U)
#define DSP_RUN_FAIL			(0x00000000U)
/* Block size in bytes when r/w data between GPP and DSP via DSP CPU */

/* Data size in bytes when r/w data bewteen GPP and DSP via EDMA:
 GPP----PCIE link----PCIE data space----EDMA----DSP device memory (L2, DDR, ...) */
#define DMA_TRANSFER_SIZE            0x400000   /* 4MB */

/* Payload size in bytes over PCIE link. PCIe module supports 
 outbound payload size of 128 bytes and inbound payload size of 256 bytes */
#define PCIE_TRANSFER_SIZE           0x80               

/* For 1MB outbound translation window size */
#define PCIE_ADLEN_1MB               0x00100000
#define PCIE_1MB_BITMASK             0xFFF00000

#define PSC_SWRSTDISABLE             0x0
#define PSC_ENABLE                   0x3

#define LOC_RST_ASSERT               0x0
#define LOC_RST_DEASSERT             0x1

/*The address of the data in the dsp  */
#define DATA_ADDR_IN_DSP (0x80B00000)
#define IMG_SIZE4_DSP (0x100000)
#define MMAP_MEM_SIZE (16*1024*1024UL)
#define MMAP_PAGE_SIZE (0x1000)
#define MMAP_REG_START_OFFSET (MMAP_MEM_SIZE - MMAP_PAGE_SIZE)

#define TIME_HZ (250)
#define SEM_WAIT_JIFIIES (2*(TIME_HZ))

struct semaphore readSemaphore, writeSemaphore;
// for DSP ready.
struct semaphore gDspDpmOverSemaphore;

struct pci_dev *g_pPcieDev = NULL;
//cyx add, for the using of interrupt+poll
LinkLayerHandler *pHandle = NULL;

int32_t gHostPciIrqNo = 0;
//pcieBarReg_t pPcieBarReginit;
//pcieBarReg_t *g_pPcieBarReg = &pPcieBarReginit;
pcieBarReg_t *g_pPcieBarReg = NULL;
extern LinkLayerRegisterTable *gpRegisterTable;
static int DPU_probe(struct pci_dev *pci_dev,
		const struct pci_device_id *pci_id);
static int DPU_open(struct inode *node, struct file *file);
static int DPU_mmap(struct file *filp, struct vm_area_struct *vma);
static long DPU_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int DPU_release(struct pci_dev* pdev);
static irqreturn_t ISR_handler(int irq, void *arg);

static const struct file_operations DPU_fops =
{ .owner = THIS_MODULE, .open = DPU_open, .unlocked_ioctl = DPU_ioctl, .mmap =
		DPU_mmap };

static struct pci_driver DPU_pci_driver =
{ probe: DPU_probe, remove: DPU_release };

int init_module(void)
{
	struct cdev * pPcieCdev = NULL;
	pcieDevNum_t pcieDevNum;

	int retValue = 0;
	uint32_t dummy = 0;

	g_pPcieBarReg = (pcieBarReg_t *) kzalloc(sizeof(pcieBarReg_t), GFP_KERNEL);

	pcieDevNum.count = 1;
	pcieDevNum.minor_first = 0;

	sema_init(&writeSemaphore, 0);
	sema_init(&readSemaphore, 0);
	sema_init(&gDspDpmOverSemaphore, 0);

	retValue = alloc_chrdev_region(&pcieDevNum.devnum, pcieDevNum.minor_first,
			pcieDevNum.count, DEV_NAME);
	if (retValue >= 0)
	{
		pcieDevNum.major = MAJOR(pcieDevNum.devnum);
	}
	else
	{
		retValue = -1;
		DEBUG_INFOR("error:alloc_chrdev_region\n");
	}

	if (retValue >= 0)
	{
		pPcieCdev = cdev_alloc();
		unsigned int devno = pcieDevNum.devnum;
		cdev_init(pPcieCdev, &DPU_fops);
		pPcieCdev->owner = THIS_MODULE;
		cdev_add(pPcieCdev, devno, 1);
	}
	else
	{
	}

	if (retValue >= 0)
	{
		retValue = PCI_FindPciDevices(&g_pPcieDev, FIND_PCIEDEV_MAXCOUNT);
	}
	else
	{
		retValue = -2;
	}

	// readBar
	if (retValue == 0)
	{
		retValue = PCI_readBAR(g_pPcieDev, g_pPcieBarReg);
	}
	else
	{
		retValue = -3;
		DEBUG_INFOR("error:PCI_FindPciDevices\n");
	}

	// enablepci
	if (retValue == 0)
	{
		pci_enable_device(g_pPcieDev);
		pci_set_drvdata(g_pPcieDev, g_pPcieBarReg->memVirt);
	}
	else
	{
		retValue = -4;
		DEBUG_INFOR("error:PCI_readBAR\n");
	}

	// setmaster
	if (retValue == 0)
	{
		retValue = PCI_setMaster(g_pPcieDev);
	}
	else
	{
	}

	// setinbound ,enableinterrupt,requestirq.
	if (retValue == 0)
	{
		PCI_setInBound(g_pPcieDev, g_pPcieBarReg);
		PCI_EnableDspInterrupt(g_pPcieBarReg);
		//request_irq(16, ISR_handler, IRQF_SHARED, "TI 667x PCIE", &dummy);
		request_irq(g_pPcieDev->irq, ISR_handler, IRQF_SHARED, "TI 667x PCIE",
				&dummy);
		printk("the host pci interrupt irq is %d\n", g_pPcieDev->irq);
	}
	else
	{
		retValue = -5;
		DEBUG_INFOR("error:PCI_setMaster\n");
	}
	// bootLoader.
	if (retValue == 0)
	{
		retValue = bootLoader(g_pPcieDev, g_pPcieBarReg);
	}
	else
	{
	}

	if (retValue == 0)
	{
		printk("LHS in %s DPU boot success\n", __func__);
	}
	else
	{
		printk("LHS in %s pci_driver failed:retValue=%d\n", __func__, retValue);
	}

	return (retValue);
}

//void cleanup_module(struct pci_dev *pPcieDev, pcieBarReg_t *pPcieBarReg)
void cleanup_module()
{

	struct pci_dev *pPcieDev = g_pPcieDev;
	pcieBarReg_t *pPcieBarReg = g_pPcieBarReg;
	if (pPcieDev != NULL)
	{
		uint32_t *ddrVirt = pPcieBarReg->ddrVirt;
		uint32_t *msmcVirt = pPcieBarReg->msmcVirt;
		uint32_t *regVirt = pPcieBarReg->regVirt;
		uint32_t *memVirt = pPcieBarReg->memVirt;

		resource_size_t regBase = pPcieBarReg->regBase;
		resource_size_t memBase = pPcieBarReg->memBase;
		resource_size_t msmcBase = pPcieBarReg->msmcBase;
		resource_size_t ddrBase = pPcieBarReg->ddrBase;

		resource_size_t regLen = pPcieBarReg->regLen;
		resource_size_t memLen = pPcieBarReg->memLen;
		resource_size_t msmcLen = pPcieBarReg->msmcLen;
		resource_size_t ddrLen = pPcieBarReg->ddrLen;
		uint32_t dummy = 0;
		int32_t irqNo = pPcieDev->irq;
		PCI_DisableDspInterrupt(pPcieBarReg);
		/* ---------------------------------------------------------------------
		 * Unmap baseRegs region & release the reg region.
		 * ---------------------------------------------------------------------
		 */
		iounmap(regVirt);
		if (pci_resource_flags(pPcieDev, 0) & IORESOURCE_MEM)
		{
			/* Map the memory region. */
			release_mem_region(regBase, regLen);
		}
		else
		{
			/* Map the memory region. */
			release_region(regBase, regLen);
		}

		/* ---------------------------------------------------------------------
		 * Unmap LL2 region & release the reg region.
		 * ---------------------------------------------------------------------
		 */
		iounmap(memVirt);
		if (pci_resource_flags(pPcieDev, 1) & IORESOURCE_MEM)
		{
			/* Map the memory region. */
			release_mem_region(memBase, memLen);
		}
		else
		{
			/* Map the memory region. */
			release_region(memBase, memLen);
		}

		/* ---------------------------------------------------------------------
		 * Unmap MSMC region & release the reg region.
		 * ---------------------------------------------------------------------
		 */
		iounmap(msmcVirt);
		if (pci_resource_flags(pPcieDev, 2) & IORESOURCE_MEM)
		{
			/* Map the memory region. */
			release_mem_region(msmcBase, msmcLen);
		}
		else
		{
			/* Map the memory region. */
			release_region(msmcBase, msmcLen);
		}

		/* ---------------------------------------------------------------------
		 * Unmap DDR region & release the reg region.
		 * ---------------------------------------------------------------------
		 */
		iounmap(ddrVirt);
		if (pci_resource_flags(pPcieDev, 3) & IORESOURCE_MEM)
		{
			/* Map the memory region. */
			release_mem_region(ddrBase, ddrLen);
		}
		else
		{
			/* Map the memory region. */
			release_region(ddrBase, ddrLen);
		}

		free_irq(irqNo, &dummy);
	}
	printk("pcie remove successful\n");
}

int DPU_probe(struct pci_dev *pci_dev, const struct pci_device_id *pci_id)
{
	struct DPU_dev DPU_cdev;
	int DPU_dev_no = MKDEV(250, 0);

	pci_enable_device(pci_dev);
	cdev_init(&DPU_cdev, &DPU_fops);
	register_chrdev_region(DPU_dev_no, 1, "DPU_driver_linux");
	cdev_add(&DPU_cdev, DPU_dev_no, 1);

	return 0;
}

int DPU_open(struct inode *node, struct file *file)
{
	int retValue = 0;
	int retLinkValue = 0;
	debug_printf("Open \n");
	// init linklayer.
	retLinkValue = LinkLayer_Open(&pHandle, g_pPcieDev, g_pPcieBarReg,
			&writeSemaphore);
	if (retLinkValue != 0)
	{
		debug_printf("LinkLayer_Open fail:%x\n", retValue);

		retValue = retLinkValue;
	}
	else
	{
		file->private_data = pHandle;
	}

	// wait DSP ready.
	// TODO: polling in the app.
	if (retLinkValue == 0)
	{
		retLinkValue = pollValue(&(pHandle->pRegisterTable->DPUBootStatus),
		DSP_RUN_READY, 0x07ffffff);
	}
	else
	{
	}

	if (retLinkValue == 0)
	{
		debug_printf("dsp is ready:DPUBootStatus=%x\n",
				pHandle->pRegisterTable->DPUBootStatus);
	}
	else
	{
		debug_printf("wait dsp ready time out DPUBootStatus=0x%x\n",
				pHandle->pRegisterTable->DPUBootStatus);
		retValue = retLinkValue;

	}
	return (retValue);
}

int DPU_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int retValue = 0;
	int stateCode = 0;
	uint32_t phyAddrFrameNO;
	uint32_t rangeLength_vma;
	intptr_t *pAlignDMAVirtAddr =
			((intptr_t *) (((LinkLayerHandler *) (filp->private_data))->pRegisterTable));
	phyAddrFrameNO = virt_to_phys(pAlignDMAVirtAddr);
	debug_printf("phyAddrFrameNO=0x%x\n", phyAddrFrameNO);
	//phyAddrFrameNO =
	//		((LinkLayerHandler *) (filp->private_data))->pRegisterTable->registerPhyAddrInPc;
	//debug_printf("phyAddrFrameNO=0x%x\n", phyAddrFrameNO);
	phyAddrFrameNO = (phyAddrFrameNO >> PAGE_SHIFT);
	rangeLength_vma = (vma->vm_end - vma->vm_start);

	debug_printf("mmap length=0x%x\n", rangeLength_vma);

	stateCode = remap_pfn_range(vma, vma->vm_start, phyAddrFrameNO,
			rangeLength_vma, PAGE_SHARED);
	if (stateCode < 0)
	{
		debug_printf("lhs remap register page  error\n");
	}
	else
	{
		debug_printf("mmap finished\n");
		retValue = stateCode;
	}

	return (retValue);
}

long DPU_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long retValue = 0;
	LinkLayerHandler *pLinkLayer = (LinkLayerHandler *) (filp->private_data);

	int32_t stateCode = 0;

	switch (cmd)
	{
	case DPU_IO_CMD_CONFIRM:
	{
		stateCode = LinkLayer_Confirm(pLinkLayer, (LINKLAYER_IO_TYPE) arg);

		if (stateCode != 0)
		{
			printk("LinkLayer_Confirm failed: %x\n", stateCode);
		}
		else
		{
		}

		break;
	}

	case DPU_IO_CMD_WAITBUFFERREADY:
	{
		DPUDriver_WaitBufferReadyParam *pParam =
				(DPUDriver_WaitBufferReadyParam *) arg;
		stateCode = LinkLayer_WaitBufferReady(pLinkLayer, pParam->waitType,
				pParam->pendTime);
		if (stateCode != 0)
		{
			debug_printf("LinkLayer_WaitBufferReady timeout: %x\n", stateCode);
		}
		else
		{
			debug_printf("linklayer wait buffer finished\n");
		}
		copy_to_user((pParam->pBufStatus), &stateCode, sizeof(int));
		break;
	}

	case DPU_IO_CMD_CHANGEBUFFERSTATUS:
	{
		int *flag = (int *) arg;
		debug_printf("DPU_IO_CMD_CHANGEBUFFERSTATUS:the *flag=%d\n", *flag);
		stateCode = LinkLayer_ChangeBufferStatus(pLinkLayer, *flag);

		if (stateCode != 0)
		{
			printk("LinkLayer_Confirm failed: %x\n", stateCode);
		}
		else
		{
		}

		break;
	}
		// trigger the interrupt to DSP,make the DSP to start DPM process the picture.
	case DPU_IO_CMD_INTERRUPT:
	{
		printk("trigger a interrupt to DSP\n");
		uint32_t *pDspMappedRegVirt = g_pPcieBarReg->regVirt;
		iowrite32(1, pDspMappedRegVirt + LEGACY_A_IRQ_STATUS_RAW / 4);
		break;
	}
//////////////////////////////////////////////////////////////////////////
	case DPU_IO_CMD_WAITDPM:
	{
		printk("wait the DSP trigger the host\n");
		down(&gDspDpmOverSemaphore);
		break;

	}
	case DPU_IO_CMD_WAITDPMSTART:
	{
		printk("wait the DSP dsp start\n");
		DPUDriver_WaitBufferReadyParam *pParam =
				(DPUDriver_WaitBufferReadyParam *) arg;
		stateCode = LinkLayer_WaitBufferReady(pLinkLayer, pParam->waitType,
				pParam->pendTime);
		if (stateCode != 0)
		{
			debug_printf("LinkLayer_WaitDpmReady timeout: %x\n", stateCode);
		}
		else
		{
			debug_printf("LinkLayer_WaitDpmReady finished\n");
		}
		break;

	}
	case DPU_IO_CMD_WRITE_TIMEOUT:
	{
		debug_printf("poll write timeout,we start to use interrupt for pc\n");
		down(&writeSemaphore);
		break;
	}
	case DPU_IO_CMD_READ_TIMEOUT:
	{
		debug_printf("poll read timeout,we start to use interrupt for pc\n");
		down(&readSemaphore);
		break;
	}
	case DPU_IO_CMD_CHANGEREG:
	{
		int *flag = (int *) arg;
		debug_printf("DPU_IO_CMD_CHANGEREG:the *flag=%d\n", *flag);
		stateCode = LinkLayer_ChangeBufferStatus(pLinkLayer, *flag);

		if (stateCode != 0)
		{
			printk("LinkLayer_ChangeBufferStatus failed: %x\n", stateCode);
		}
		else
		{
			printk("LinkLayer_ChangeBufferStatus success\n");
		}
		break;
	}
	case DPU_IO_CMD_CHANGOVERREG:
	{
		int *flag = (int *) arg;
		debug_printf("DPU_IO_CMD_CHANGOVERREG:the *flag=%d\n", *flag);
		stateCode = LinkLayer_ChangeBufferStatus(pLinkLayer, *flag);
		if (stateCode != 0)
		{
			printk("LinkLayer_ChangeBufferStatus failed: %x\n", stateCode);
		}
		else
		{
			printk("LinkLayer_ChangeBufferStatus success\n");
		}
		break;
	}
	case DPU_IO_CMD_CHECKDPMALLOVER:
	{
		DPUDriver_WaitBufferReadyParam *pParam =
						(DPUDriver_WaitBufferReadyParam *) arg;
		stateCode = LinkLayer_CheckDpmStatus(pLinkLayer);
		if (stateCode != 0)
		{
			debug_printf("LinkLayer_CheckDpmStatus timeout: %x\n", stateCode);
		}
		else
		{
			debug_printf("LinkLayer_CheckDpmStatus finished: %x\n", stateCode);
		}
		copy_to_user((pParam->pBufStatus), &stateCode, sizeof(int));
		break;

	}
	default:
	{
		retValue = 0;
		printk("other operation cmd is %d\n", cmd);

		break;
	}

	}

	return (retValue);
}

int DPU_release(struct pci_dev* pdev)
{
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	return 0;
}

static irqreturn_t ISR_handler(int irq, void *arg)
{
	uint32_t status;
	uint32_t retValue;
	debug_printf("ISR_handler function irq is %d\n", irq);

	status = HAL_CheckPciInterrupt(g_pPcieBarReg);
	if (status == 1)
	{
		printk("cyx receive interrupt from dsp\n");
	}
	//cheak zone status
	retValue = LinkLayer_CheckStatus(gpRegisterTable);
	if (retValue == 0)
	{
		debug_printf("LinkLayer_CheckStatus success\n");
	}
	else
	{
		debug_printf("LinkLayer_CheckStatus error\n");
	}

	PCI_ClearDspInterrupt(g_pPcieBarReg);

}
