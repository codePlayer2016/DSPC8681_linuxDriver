#include "pcie.h"
#include "DPUdebug.h"
#include "DPURegs.h"

// TODO the pPcieDev should be a struct pci_dev array. eg struct pci_dev pciDevArray[n]
//		so we can find many pci device.
int PCI_FindPciDevices(struct pci_dev ** pPcieDev, int MaxCount)
{
#if 0
	struct pci_dev *dev = NULL;
	int retValue = 0;
	int cycCount = 0;
	int loopCondition = 0;

	dev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, NULL);
	loopCondition = ((dev->vendor == PCIE_TI_VENDOR)
			&& (dev->device == PCIE_TI_DEVICE));

	for (cycCount = 0; (dev != NULL) && (loopCondition != 1); cycCount++)
	{
		dev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, dev);
		loopCondition = ((dev->vendor == PCIE_TI_VENDOR)
				&& (dev->device == PCIE_TI_DEVICE) && (cycCount < MaxCount));
	}
	if (loopCondition == 1)
	{
		*pPcieDev = dev;
		pci_dev_put(*pPcieDev);
	}
	else
	{
		retValue = -1;
		DEBUG_INFOR("error:can't find pcieCard\n");
	}

	return (retValue);
#endif
#define TI667X_PCI_VENDOR_ID (0x104c)
#define TI667X_PCI_DEVICE_ID (0xb005)
	struct pci_dev *dev = NULL;
	struct pci_dev *devArray[6];
	int retValue = 0;
	int cycCount = 0;
	int loopCondition = 0;
	uint32_t pciDevCount = 0;
//	get the pci dev.
	//1. get all device ,include the pci bridge.
	//2. from all device just get the first dsp
	{
		do
		{
			dev = pci_get_device(TI667X_PCI_VENDOR_ID, TI667X_PCI_DEVICE_ID,
					dev);
			if (NULL != dev)
			{
				if ((dev->class >> 8) == PCI_CLASS_BRIDGE_PCI)
				{
					debug_printf("skipped pcie RC\n");
					continue;
				}
				else
				{
					debug_printf("find the pcie EP:pciDevCount\n", pciDevCount);
					devArray[pciDevCount] = dev;
					pciDevCount++;
				}
			}
			else
			{
				debug_printf("pciDevCount=%x\n", pciDevCount);
				break;
			}

		} while (1);
		//	debug for the dsp NO.
		int i = 0;
		for (i = 0; i < pciDevCount; i++)
		{
			debug_printf("the bar0[%x]=%x\n", i,
					pci_resource_start(devArray[i], 0));
		}
	}

	if (pciDevCount > 0)
	{
		*pPcieDev = devArray[0];//1
		//gHostPciIrqNo=pPcieDev->irq;
	}
	else
	{
		debug_printf("find no pci device\n");
	}

	return (retValue);
}

int PCI_readBAR(struct pci_dev *pPcieDev, pcieBarReg_t *pPcieBarReg)
{
	int retValue = 0;

	resource_size_t regBase = 0;
	resource_size_t memBase = 0;
	resource_size_t msmcBase = 0;
	resource_size_t ddrBase = 0;

	resource_size_t barStart[4];
	resource_size_t barLen[4];
	resource_size_t barFlags[4];

	barStart[0] = pci_resource_start(pPcieDev, 0); /* BAR0 4K for PCIE application registers */
	barLen[0] = pci_resource_len(pPcieDev, 0);
	barFlags[0] = pci_resource_flags(pPcieDev, 0);
	barStart[1] = pci_resource_start(pPcieDev, 1); /* BAR1 512K/1024K for 6678/6670 Local L2 */
	barLen[1] = pci_resource_len(pPcieDev, 1);
	barFlags[1] = pci_resource_flags(pPcieDev, 1);
	barStart[2] = pci_resource_start(pPcieDev, 2); /* BAR2 4M/2M for 6678/6670 Shared L2 */
	barLen[2] = pci_resource_len(pPcieDev, 2);
	barFlags[2] = pci_resource_flags(pPcieDev, 2);
	barStart[3] = pci_resource_start(pPcieDev, 3); /* BAR3 16M for DDR3 */
	barLen[3] = pci_resource_len(pPcieDev, 3);
	barFlags[3] = pci_resource_flags(pPcieDev, 3);

	/* ---------------------------------------------------------------------
	 * Map the REG memory region
	 * ---------------------------------------------------------------------
	 */
	if (barFlags[0] & IORESOURCE_MEM)
	{
		regBase = barStart[0];
		/* Map the memory region. */
		request_mem_region(regBase, barLen[0], "DSPLINK");
	}
	else
	{
		/* Map the memory region. */
		request_region(regBase, barLen[0], "DSPLINK");
	}

	if (regBase > 0)
	{
		pPcieBarReg->regVirt = ioremap(barStart[0], barLen[0]);
	}
	else
	{
		retValue = -1;
	}

	/* ---------------------------------------------------------------------
	 * Map the LL2RAM memory region
	 * ---------------------------------------------------------------------
	 */
	if (barFlags[1] & IORESOURCE_MEM)
	{
		memBase = barStart[1];
		/* Map the memory region. */
		request_mem_region(memBase, barLen[1], "DSPLINK");
	}
	else
	{
		/* Map the memory region. */
		request_region(memBase, barLen[1], "DSPLINK");
	}

	if (memBase > 0)
	{
		pPcieBarReg->memVirt = ioremap(barStart[1], barLen[1]);
	}
	else
	{
		retValue = -2;
	}

	/* ---------------------------------------------------------------------
	 * Map the MSMC memory region
	 * ---------------------------------------------------------------------
	 */
	if (barFlags[2] & IORESOURCE_MEM)
	{
		msmcBase = barStart[2];
		/* Map the memory region. */
		request_mem_region(msmcBase, barLen[2], "DSPLINK");
	}
	else
	{
		/* Map the memory region. */
		request_region(msmcBase, barLen[2], "DSPLINK");
	}

	if (msmcBase > 0)
	{
		pPcieBarReg->msmcVirt = ioremap(barStart[2], barLen[2]);
	}
	else
	{
		retValue = -3;
	}

	/* ---------------------------------------------------------------------
	 * Map the DDR memory region
	 * ---------------------------------------------------------------------
	 */
	if (barFlags[3] & IORESOURCE_MEM)
	{
		ddrBase = barStart[3];
		/* Map the memory region. */
		request_mem_region(ddrBase, barLen[3], "DSPLINK");
		printk("--------------->IO memory\n");
	}
	else
	{
		/* Map the memory region. */
		request_region(ddrBase, barLen[3], "DSPLINK");
		printk("--------------->IO space\n");
	}

	if (ddrBase > 0)
	{
		pPcieBarReg->ddrVirt = ioremap(barStart[3], barLen[3]);
	}
	else
	{
		retValue = -4;
	}

	pPcieBarReg->regBase = regBase;
	pPcieBarReg->memBase = memBase;
	pPcieBarReg->msmcBase = msmcBase;
	pPcieBarReg->ddrBase = ddrBase;

	pPcieBarReg->regLen = barLen[0];
	pPcieBarReg->memLen = barLen[1];
	pPcieBarReg->msmcLen = barLen[2];
	pPcieBarReg->ddrLen = barLen[3];

	if (retValue == 0)
	{
	}
	else
	{
		DEBUG_INFOR("error:PCI_readBAR\n");
	}

	//printk("LHS the value is %x\n", pPcieBarReg->regVirt);
	return (retValue);
}

/* =============================================================================
 *  @func   PCI_setMaster
 *
 *  @desc   This function makes the given device to be master.
 *
 *  @modif  None.
 *  ============================================================================
 */
int PCI_setMaster(struct pci_dev *pPcieDev)
{
	int32_t retValue = 0;
	uint16_t cmdVal;
	struct pci_dev *dev = pPcieDev;

#if 0
	/* set the DMA mask */
	if (pci_set_dma_mask(dev, 0xfffffff0ULL))
	{
	}
#endif
	/* set the desired PCI dev to be master, this internally sets the latency timer */
	pci_set_master(dev);
	pci_write_config_byte(dev, PCI_LATENCY_TIMER, 0x80);

	/* Add support memory write back and invalidate */
	retValue = pci_set_mwi(dev);
	if (retValue == 0)
	{
		pci_read_config_word(dev, PCI_COMMAND, (u16 *) &cmdVal);
		/* and set the master bit in command register. */
		cmdVal |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER | PCI_COMMAND_SERR;
		/* and clear the interrupt disable bit in command register. */
		cmdVal &= ~PCI_COMMAND_INTX_DISABLE;
		pci_write_config_word(dev, PCI_COMMAND, cmdVal);
	}
	else
	{
		retValue = -1;
	}

	return (retValue);
}

/* =============================================================================
 *  @func   HAL_CheckPciInterrupt
 *
 *  @desc   This function check whether interrupt is generated by C667x or not.
 *
 *  @modif  None.
 *  ============================================================================
 */
bool HAL_CheckPciInterrupt(pcieBarReg_t *pPcieBarReg)
{
	uint32_t *pReg = (uint32_t *) pPcieBarReg->regVirt;
	return ioread32(pReg + EP_IRQ_STATUS / 4);
}

/** ============================================================================
 *  @func   PCI_ClearDspInterrupt
 *
 *  @desc   Clear pending interrupt from DSP to Host.
 *
 *  @modif  None.
 *  ============================================================================
 */
void PCI_ClearDspInterrupt(pcieBarReg_t *pPcieBarReg)
{
	uint32_t *pReg = (uint32_t *) pPcieBarReg->regVirt;
	iowrite32(1, pReg + EP_IRQ_CLR / 4);
}

/** ============================================================================
 *  @func   PCI_EnableDspInterrupt
 *
 *  @desc   Allow the DSP to generate interrupts to the Host.
 *
 *  @modif  None.
 *  ============================================================================
 */
void PCI_EnableDspInterrupt(pcieBarReg_t *pPcieBarReg)
{
	uint32_t *pReg = (uint32_t *) pPcieBarReg->regVirt;

	iowrite32(1, pReg + LEGACY_A_IRQ_ENABLE_SET / 4);
	iowrite32(1, pReg + LEGACY_B_IRQ_ENABLE_SET / 4);
}

/** ============================================================================
 *  @func   PCI_DisableDspInterrupt
 *
 *  @desc   Disable the DSP to generate interrupts to the Host.
 *
 *  @modif  None.
 *  ============================================================================
 */
void PCI_DisableDspInterrupt(pcieBarReg_t *pPcieBarReg)
{
	uint32_t *pReg = (uint32_t *) pPcieBarReg->regVirt;
	iowrite32(1, pReg + LEGACY_A_IRQ_ENABLE_CLR / 4);
	iowrite32(1, pReg + LEGACY_B_IRQ_ENABLE_CLR / 4);
}

void PCI_setInBound(struct pci_dev *pPcieDev, pcieBarReg_t *pPcieBarReg)
{
	uint32_t i = 0;
	/* Pointing to the beginning of the application registers */
	uint32_t *ptrReg = (uint32_t *) pPcieBarReg->regVirt;

	/* Configure IB_BAR0 to BAR0 for PCIE registers; Configure IB_BAR1 to BAR1 for LL2 for core 0
	 Configure IB_BAR2 to BAR2 for MSMC; Configure IB_BAR3 to BAR3 for DDR */
	for (i = 0; i < 4; i++)
	{
		iowrite32(i, ptrReg + IB_BAR(i) / 4);
		iowrite32(pPcieDev->resource[i].start, ptrReg + IB_START_LO(i) / 4);
		iowrite32(0, ptrReg + IB_START_HI(i) / 4);
	}
	iowrite32(PCIE_BASE_ADDRESS, ptrReg + IB_OFFSET(0) / 4);
	iowrite32(LL2_START + (1 << 28), ptrReg + IB_OFFSET(1) / 4);
	iowrite32(MSMC_START, ptrReg + IB_OFFSET(2) / 4);
	iowrite32(DDR_START, ptrReg + IB_OFFSET(3) / 4);

}
