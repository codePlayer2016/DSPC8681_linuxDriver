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

struct pci_dev *g_pPcieDev[4];
//cyx add, for the using of interrupt+poll
LinkLayerHandler *pHandle[4];

//cyx 20160607
pcieDevNum_t pcieDevNum;
struct cdev * pPcieCdev[4];
extern dma_addr_t DMAPhyAddr;
extern uint8_t *DMAVirAddr;

int32_t gHostPciIrqNo = 0;
//pcieBarReg_t pPcieBarReginit;
//pcieBarReg_t *g_pPcieBarReg = &pPcieBarReginit;
pcieBarReg_t *g_pPcieBarReg[4];

ProcessorUnitDev_t *g_pProcessorUnitDev[4];
extern LinkLayerRegisterTable *gpRegisterTable;
static int DPU_probe(struct pci_dev *pci_dev, const struct pci_device_id *pci_id);
static int DPU_open(struct inode *node, struct file *file);
static int DPU_mmap(struct file *filp, struct vm_area_struct *vma);
static long DPU_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int DPU_release(struct pci_dev* pdev);
static irqreturn_t ISR_handler(int irq, void *arg);

static const struct file_operations DPU_fops =
{ .owner = THIS_MODULE, .open = DPU_open, .unlocked_ioctl = DPU_ioctl, .mmap = DPU_mmap };

static struct pci_driver DPU_pci_driver =
{ probe: DPU_probe, remove: DPU_release };

// todo: we should design a struct include all the chip status in the plateform.
int init_module(void)
{

	int retValue = 0;
	int pciDevCount = 0;
	uint32_t dummy = 0;

	sema_init(&writeSemaphore, 0);
	sema_init(&readSemaphore, 0);
	sema_init(&gDspDpmOverSemaphore, 0);

	retValue = PCI_FindPciDevices(g_pPcieDev, &pciDevCount);
	if (retValue != 0)
	{
		debug_printf("error:PCI_FindPciDevices\n");
		return (retValue);
	}

	int chipIndex = 0;
	for (chipIndex = 0; chipIndex < pciDevCount; chipIndex++)
	{
		g_pPcieBarReg[chipIndex] = (pcieBarReg_t *) kzalloc(sizeof(pcieBarReg_t), GFP_KERNEL);
	}

	pcieDevNum.count = pciDevCount;
	pcieDevNum.minor_first = 0;
	retValue = alloc_chrdev_region(&(pcieDevNum.devnum), pcieDevNum.minor_first, pcieDevNum.count, DEV_NAME);
	if (retValue)
	{
		debug_printf("error:alloc_chrdev_region\n");
		return (retValue);
	}

	pcieDevNum.major = MAJOR(pcieDevNum.devnum);

	for (chipIndex = pcieDevNum.minor_first; chipIndex < pcieDevNum.count; chipIndex++)
	{
		//pPcieCdev[chipIndex] = cdev_alloc();
		pPcieCdev[chipIndex] = &(g_pProcessorUnitDev[chipIndex]->charDev);
		cdev_init(pPcieCdev[chipIndex], &DPU_fops);
		pPcieCdev[chipIndex]->owner = THIS_MODULE;
		cdev_add(pPcieCdev[chipIndex], MKDEV(pcieDevNum.major, chipIndex), 1);

		debug_printf("dev_t=%x\n", MKDEV(pcieDevNum.major, chipIndex));

		retValue = PCI_readBAR(g_pPcieDev[chipIndex], g_pPcieBarReg[chipIndex]);
		if (0 != retValue)
		{
			debug_printf("error:PCI_readBAR\n");
			return (retValue);
		}

		pci_enable_device(g_pPcieDev[chipIndex]);
		pci_set_drvdata(g_pPcieDev[chipIndex], g_pPcieBarReg[chipIndex]->memVirt);
		retValue = PCI_setMaster(g_pPcieDev[chipIndex]);
		PCI_setInBound(g_pPcieDev[chipIndex], g_pPcieBarReg[chipIndex]);
		PCI_EnableDspInterrupt(g_pPcieBarReg[chipIndex]);
		//request_irq(16, ISR_handler, IRQF_SHARED, "TI 667x PCIE", &dummy);
		request_irq(g_pPcieDev[chipIndex]->irq, ISR_handler, IRQF_SHARED, "TI 667x PCIE", &dummy);
		//debug_printf("irq is %d\n", g_pPcieDev[chipIndex]->irq);

		g_pProcessorUnitDev[chipIndex] = (ProcessorUnitDev_t *) kzalloc(sizeof(ProcessorUnitDev_t), GFP_KERNEL);
		g_pProcessorUnitDev[chipIndex]->devMinor = chipIndex;
		//g_pProcessorUnitDev[chipIndex]->pCharDev = pPcieCdev[chipIndex];
		g_pProcessorUnitDev[chipIndex]->pPciBarReg = g_pPcieBarReg[chipIndex];
		g_pProcessorUnitDev[chipIndex]->pPciDev = g_pPcieDev[chipIndex];
		retValue = bootLoader(g_pPcieDev[chipIndex], g_pPcieBarReg[chipIndex], chipIndex);
		if (0 != retValue)
		{
			debug_printf("%dth processor boot error\n", chipIndex);
			continue;
		}
		debug_printf("*g_pProcessorUnitDev[%d]=%x,pciDev=%x,pciBarReg=%x\n", chipIndex, *g_pProcessorUnitDev[chipIndex], *g_pPcieDev[chipIndex],
				*g_pPcieBarReg[chipIndex]);
	}
	return (retValue);
}

//void cleanup_module(struct pci_dev *pPcieDev, pcieBarReg_t *pPcieBarReg)
void cleanup_module()
{
	//todo:
	// unloadLoader();

#if 0
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
		//cyx add 20160607
		kfree(pPcieBarReg);
		unregister_chrdev_region(pcieDevNum.devnum, 1);
		cdev_del(pPcieCdev);
		dma_free_coherent(&pPcieDev->dev, DMA_TRANSFER_SIZE, DMAVirAddr,
				DMAPhyAddr);

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
#endif
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

int DPU_open(struct inode *node, struct file *filp)
{
	int retVal = 0;
	if (NULL == node->i_cdev)
	{
		debug_printf("cdev==null\n");
	}

	debug_printf("dev_t=%x,devMinor=%x,fileName=%s cdev=%x\n", node->i_rdev, MINOR(node->i_rdev), filp->f_path.dentry->d_name.name, *(node->i_cdev));

	ProcessorUnitDev_t *pProcessorUnitDev = container_of(node->i_cdev, ProcessorUnitDev_t, charDev);

	if (NULL == pProcessorUnitDev->pPciDev)
	{
		debug_printf("pPciDev==NULL\n");
	}
	else
	{
		debug_printf("pPciDev=%d\n", pProcessorUnitDev->pPciDev);
	}

	if (NULL == pProcessorUnitDev->pPciBarReg)
	{
		debug_printf("pPciBarReg==NULL\n");
	}
	else
	{
		debug_printf("pPciBarReg=%d\n", pProcessorUnitDev->pPciBarReg);
	}

	debug_printf("minor=%d,pciDev=%x,pciBarReg=%x,pProcessorUnitDev=%x\n", pProcessorUnitDev->devMinor, *(pProcessorUnitDev->pPciDev),
			*(pProcessorUnitDev->pPciBarReg), *pProcessorUnitDev);
	debug_printf("minor=%d\n", (*pProcessorUnitDev).devMinor);

	int retLinkValue = 0;

	//retLinkValue = LinkLayer_Open(&pHandle, g_pPcieDev, g_pPcieBarReg, NULL);
	LinkLayerHandler *pHandle = (LinkLayerHandler *) kmalloc(sizeof(LinkLayerHandler), GFP_KERNEL);
	if (NULL == pHandle)
	{
		retVal = -1;
		debug_printf("error:kmalloc\n");
		return (retVal);
	}
	//retLinkValue = LinkLayer_Open(pHandle, pProcessorUnitDev->pPciDev, pProcessorUnitDev->pPciBarReg, NULL);
	retLinkValue = LinkLayer_Open(pHandle, pProcessorUnitDev);
	if (retLinkValue <= 0)
	{
		debug_printf("error:LinkLayer_Open\n");
		return (retLinkValue);
	}

	filp->private_data = pHandle;

#if 0
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
		debug_printf("dsp is ready:DPUBootStatus=%x\n", pHandle->pRegisterTable->DPUBootStatus);
	}
	else
	{
		debug_printf("wait dsp ready time out DPUBootStatus=0x%x\n", pHandle->pRegisterTable->DPUBootStatus);
		retValue = retLinkValue;
	}
#endif
	debug_printf("successful,openChipMask=%x\n", retLinkValue);
	return (retLinkValue);

}

static void DPU_close(struct file *pFile)
{
	LinkLayerHandler **pHandle;
	pHandle = (LinkLayerHandler **) (pFile->private_data);
	LinkLayer_Close(pHandle);
}

// problem: one map function. how to solution other map.
// may be wait and solution one chip
// can only test for U0 and chipIndex MUST BE 0
int DPU_mmap(struct file *filp, struct vm_area_struct *vma)
{
#if 0
	int retVal = 0;
	int stateCode = 0;
	uint32_t phyAddrFrameNO;
	uint32_t rangeLength_vma;
	intptr_t *pAlignDMAVirtAddr;
	LinkLayerHandler **chipComHandle;
	chipComHandle = (LinkLayerHandler **) (filp->private_data);
	int chipIndex = 0;
	int maxChipNum = 4;
	int openChipMask = 0;
	for (chipIndex = 0; chipIndex < maxChipNum; chipIndex++)
	{
		if (NULL == chipComHandle[chipIndex])
		{
			continue;
		}
		else
		{
			openChipMask |= (1 << chipIndex);
			// TODO: solution the problem.
			pAlignDMAVirtAddr = ((intptr_t *) (chipComHandle[chipIndex]->pRegisterTable));
			phyAddrFrameNO = virt_to_phys(pAlignDMAVirtAddr);
			phyAddrFrameNO = (phyAddrFrameNO >> PAGE_SHIFT);
			rangeLength_vma = (vma->vm_end - vma->vm_start);
			stateCode = remap_pfn_range(vma, vma->vm_start, phyAddrFrameNO, rangeLength_vma, PAGE_SHARED);
			if (stateCode < 0)
			{
				debug_printf("error:remap_pfn_range\n");
				return (stateCode);
			}
		}

	}
	if (0 == openChipMask)
	{
		debug_printf("maybeError:no chip be open\n");
		return (0);
	}
	else
	{
		retVal = openChipMask;
		return (retVal);
	}
#if 0
	intptr_t *pAlignDMAVirtAddr = ((intptr_t *) (((LinkLayerHandler *) (filp->private_data))->pRegisterTable));
	phyAddrFrameNO = virt_to_phys(pAlignDMAVirtAddr);
	debug_printf("phyAddrFrameNO=0x%x\n", phyAddrFrameNO);
//phyAddrFrameNO =
//		((LinkLayerHandler *) (filp->private_data))->pRegisterTable->registerPhyAddrInPc;
//debug_printf("phyAddrFrameNO=0x%x\n", phyAddrFrameNO);
	phyAddrFrameNO = (phyAddrFrameNO >> PAGE_SHIFT);
	rangeLength_vma = (vma->vm_end - vma->vm_start);

	debug_printf("mmap length=0x%x\n", rangeLength_vma);

	stateCode = remap_pfn_range(vma, vma->vm_start, phyAddrFrameNO, rangeLength_vma, PAGE_SHARED);
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
#endif
#endif
}

long DPU_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
#if 0
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
#endif
}

int DPU_release(struct pci_dev* pdev)
{
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	return 0;
}

static irqreturn_t ISR_handler(int irq, void *arg)
{
#if 0
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
#endif
}
