/*
 * Pcie.h
 *
 *  Created on: Sep 30, 2015
 *      Author: spark
 */

#ifndef INC_PCIE_H_
#define INC_PCIE_H_

//#include <stdint.h>
#include <linux/pci.h>
//#include <asm/dma-mapping.h>
//#include <linux/types.h>
#include <linux/kernel.h>
#include "common.h"



typedef struct __tagPcieDevNum
{
	dev_t devnum;
	unsigned int major;
	unsigned int minor;
	unsigned int minor_first;
	unsigned int count;
} pcieDevNum_t;

int PCI_FindPciDevices(struct pci_dev **pPcieDev, int MaxCount);
int PCI_readBAR(struct pci_dev *pPcieDev, pcieBarReg_t *pPcieBarReg);
int PCI_setMaster(struct pci_dev *pPcieDev);
void PCI_EnableDspInterrupt(pcieBarReg_t *pPcieDevReg);
void PCI_setInBound(struct pci_dev *pPcieDev, pcieBarReg_t *pPcieBarReg);

void PCI_DisableDspInterrupt(pcieBarReg_t *pPcieBarReg);
void PCI_ClearDspInterrupt(pcieBarReg_t *pPcieBarReg);

#endif /* INC_PCIE_H_ */
