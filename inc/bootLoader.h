/*
 * bootLoader.h
 *
 *  Created on: Nov 4, 2015
 *      Author: spark
 */

#ifndef INC_BOOTLOADER_H_
#define INC_BOOTLOADER_H_

// the DSPBootStaus Register value range.
#define DSP_INIT_READY 			(0x00000001U)
#define DSP_INIT_MASK 			(0x00000001U)

#define DSP_GETCODE_FINISH 		(0x00000010U)
#define DSP_GETCODE_FAIL		(0x00000000U)

#define DSP_CRCCHECK_SUCCESSFUL (0x00000100U)
#define DSP_CRCCHECK_FAIL		(0x00000000U)

#define DSP_GETENTRY_FINISH 	(0x00001000U)
#define DSP_GETENTRY_FAIL		(0x00000000U)

#define DSP_RUN_READY			(0x00010000U)
#define DSP_RUN_FAIL			(0x00000000U)

// the PC_PushCodeStatus Register value range.
#define PC_PUSHCODE_FINISH      (0x00000011U)
#define PC_PUSHCODE_FAIL		(0x00000000U)

// Register Length
#define REG_LEN					(2*1024*4U)



int bootLoader(struct pci_dev *pPciDev, pcieBarReg_t *pPcieBarReg);

#endif /* INC_BOOTLOADER_H_ */
