/*
 * DPURegs.h
 *
 *  Created on: Sep 30, 2015
 *      Author: spark
 */

#ifndef _INC_DPUREGS_H_
#define _INC_DPUREGS_H_

#define MAGIC_ADDR          (0x0087FFFC)
#define BLOCK_TRANSFER_SIZE          0x100

#define LL2_START                    (0x00800000U)
#define MSMC_START                   (0x0C000000U)
#define DDR_START                    (0x80000000U)
#define PCIE_DATA                    (0x60000000U)

/* PCIE registers */
#define PCIE_BASE_ADDRESS            0x21800000
#define OB_SIZE                      0x30
#define PRIORITY                     0x3C
#define EP_IRQ_CLR                   0x68
#define EP_IRQ_STATUS                0x6C

#define OB_OFFSET_INDEX(n)           (0x200 + (8 * (n)))
#define OB_OFFSET_HI(n)              (0x204 + (8 * (n)))
#define IB_BAR(n)                    (0x300 + (0x10 * (n)))
#define IB_START_LO(n)               (0x304 + (0x10 * (n)))
#define IB_START_HI(n)               (0x308 + (0x10 * (n)))
#define IB_OFFSET(n)                 (0x30C + (0x10 * (n)))

#define PCIE_TI_VENDOR               0x104C
#define PCIE_TI_DEVICE               0xB005

#define LEGACY_A_IRQ_STATUS_RAW      0x180
#define LEGACY_A_IRQ_ENABLE_SET      0x188
#define LEGACY_A_IRQ_ENABLE_CLR      0x18C

#define LEGACY_B_IRQ_STATUS_RAW      0x190
#define LEGACY_B_IRQ_ENABLE_SET      0x198
#define LEGACY_B_IRQ_ENABLE_CLR      0x19C

/* For 1MB outbound translation window size */
#define PCIE_ADLEN_1MB               0x00100000
#define PCIE_1MB_BITMASK             0xFFF00000

#define WAITTIME (0x0FFFFFFF)
//#define INBUF_READY (0x55aa55aa)
//#define OUTBUF_READY (0xaa55aa55)

// PC-side write(DSP-side read) buffer status.
#define PC_WT_READY		(0x000055aaU)
#define PC_WT_OVER		(0x55aa0000U)
#define PC_WT_BUSY		(0x55555555U)
// DSP-side read buffer status.
#define DSP_RD_INIT		(0x000055aaU)
#define DSP_RD_READY 	(0x55aa0000U)
#define DSP_RD_OVER 	(0x000055aaU)
#define DSP_RD_BUSY		(0x55555555U)

// PC-side read(DSP-side write) buffer status.
#define PC_RD_INIT		(0xaa000055U)
#define PC_RD_READY		(0x550000aaU)
#define PC_RD_OVER		(0xaa000055U)
#define PC_RD_BUSY		(0x55555555U)
// DSP-side write buffer status.
#define DSP_WT_READY 	(0xaa000055U)
#define DSP_WT_OVER 	(0x550000aaU)
#define DSP_WT_BUSY		(0x55555555U)

#define WTBUFLENGTH (2*4*1024U)
#define RDBUFLENGTH (4*1024*1024U-4*4*1024U)

#define PCIE_TI_VENDOR               0x104C
#define PCIE_TI_DEVICE               0xB005
#endif // _INC_DPUREGS_H_
