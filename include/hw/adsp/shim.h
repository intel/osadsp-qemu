/* Core SHIM support for audio DSP.
 *
 * Copyright (C) 2016 Intel Corporation
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ADSP_IO_H
#define ADSP_IO_H

#define SHIM_CSR		0x00
#define SHIM_PISR		0x08
#define SHIM_PIMR		0x10
#define SHIM_ISRX		0x18
#define SHIM_ISRD		0x20
#define SHIM_IMRX		0x28
#define SHIM_IMRD		0x30
#define SHIM_IPCX		0x38 /* IPC IA -> SST */
#define SHIM_IPCD		0x40 /* IPC SST -> IA */
#define SHIM_IPCXL		0x38 /* IPC IA -> SST */
#define SHIM_IPCXH		0x3c /* IPC IA -> SST */
#define SHIM_IPCDL		0x40 /* IPC SST -> IA */
#define SHIM_IPCDH		0x44 /* IPC SST -> IA */
#define SHIM_ISRSC		0x48
#define SHIM_ISRLPESC		0x50
#define SHIM_IMRSCL		0x58
#define SHIM_IMRSCH		0x5c
#define SHIM_IMRLPESC		0x60
#define SHIM_IPCSCL		0x68
#define SHIM_IPCSCH		0x6c
#define SHIM_IPCLPESCL		0x70
#define SHIM_IPCLPESCH		0x74
#define SHIM_CLKCTL		0x78
#define SHIM_FR_LAT_REQ		0x80
#define SHIM_EXT_TIMER_CNTLL	0xC0
#define SHIM_EXT_TIMER_CNTLH	0xC4
#define SHIM_EXT_TIMER_STAT	0xC8
#define SHIM_LTRC		0xE0
#define SHIM_HMDC		0xE8

#define SHIM_SHIM_BEGIN		SHIM_CSR
#define SHIM_SHIM_END		SHIM_HDMC

/* CSR 0x0 */
#define SHIM_CSR_RST		(0x1 << 0)
#define SHIM_CSR_VECTOR_SEL	(0x1 << 1)
#define SHIM_CSR_STALL		(0x1 << 2)
#define SHIM_CSR_PWAITMODE	(0x1 << 3)
#define SHIM_CSR_DCS(x)		(x << 4)
#define SHIM_CSR_DCS_MASK	(0x7 << 4)

#define SHIM_CSR_HSW_RST    (0x1 << 1)
#define SHIM_CSR_HSW_STALL      (0x1 << 10)


/* PISR 0x8 */
#define SHIM_PISR_EXTT		(0x1 << 10)

/* PIMR 0x10 */
#define SHIM_PIMR_EXTT		(0x1 << 10)

/*  ISRX 0x18 */
#define SHIM_ISRX_BUSY		(0x1 << 1)
#define SHIM_ISRX_DONE		(0x1 << 0)

/*  ISRD / ISD */
#define SHIM_ISRD_BUSY		(0x1 << 1)
#define SHIM_ISRD_DONE		(0x1 << 0)

/* IMRX / IMC */
#define SHIM_IMRX_BUSY		(0x1 << 1)
#define SHIM_IMRX_DONE		(0x1 << 0)

/* IMRD / IMD */
#define SHIM_IMRD_DONE		(0x1 << 0)
#define SHIM_IMRD_BUSY		(0x1 << 1)
#define SHIM_IMRD_SSP0		(0x1 << 16)
#define SHIM_IMRD_DMAC0		(0x1 << 21)
#define SHIM_IMRD_DMAC1		(0x1 << 22)
#define SHIM_IMRD_DMAC		(SHIM_IMRD_DMAC0 | SHIM_IMRD_DMAC1)

/*  IPCX / IPCC */
#define	SHIM_IPCX_DONE		(0x1 << 30)
#define	SHIM_IPCX_BUSY		(0x1 << 31)

/*  IPCD */
#define	SHIM_IPCD_DONE		(0x1 << 30)
#define	SHIM_IPCD_BUSY		(0x1 << 31)

/*  IPCX / IPCCH */
#define	SHIM_IPCXH_DONE		(0x1 << 30)
#define	SHIM_IPCXH_BUSY		(0x1 << 31)

/*  IPCDH */
#define	SHIM_IPCDH_DONE		(0x1 << 30)
#define	SHIM_IPCDH_BUSY		(0x1 << 31)

/* ISRLPESC */
#define SHIM_ISRLPESC_DONE	(0x1 << 0)
#define SHIM_ISRLPESC_BUSY	(0x1 << 1)

/* IMRLPESC */
#define	SHIM_IMRLPESC_BUSY	(0x1 << 1)
#define	SHIM_IMRLPESC_DONE	(0x1 << 0)

/* IPCSCH */
#define SHIM_IPCSCH_DONE	(0x1 << 30)
#define SHIM_IPCSCH_BUSY	(0x1 << 31)

/* IPCLPESCH */
#define SHIM_IPCLPESCH_DONE	(0x1 << 30)
#define SHIM_IPCLPESCH_BUSY	(0x1 << 31)

/* CLKCTL */
#define SHIM_CLKCTL_SSP2_EN	(1 << 18)
#define SHIM_CLKCTL_SSP1_EN	(1 << 17)
#define SHIM_CLKCTL_SSP0_EN	(1 << 16)
#define SHIM_CLKCTL_FRCHNGGO	(1 << 5)
#define SHIM_CLKCTL_FRCHNGACK	(1 << 4)

/* SHIM_FR_LAT_REQ */
#define SHIM_FR_LAT_CLK_MASK	0x7

/* ext timer */
#define SHIM_EXT_TIMER_RUN	(1 << 31)
#define SHIM_EXT_TIMER_CLEAR	(1 << 30)

/* shared memory IDs */
#define ADSP_IO_SHM_IRAM	0
#define ADSP_IO_SHM_DRAM	1
#define ADSP_IO_SHM_SHIM	2
#define ADSP_IO_SHM_MBOX	3
#define ADSP_IO_SHM_DMAC(dmac)			(4 + dmac)
#define ADSP_IO_SHM_DMA(c, chan)		((c + 1) * 8 + chan)

/* messages */
#define PMC_DDR_LINK_UP		0xc0	/* LPE req path to DRAM is up */
#define PMC_DDR_LINK_DOWN	0xc1	/* LPE req path to DRAM is down */
#define PMC_SET_LPECLK		0xc2	/* LPE req clock change to FR_LAT_REQ */
#define PMC_SET_SSP_19M2	0xc5	/* LPE req SSP clock to 19.2MHz */
#define PMC_SET_SSP_25M		0xc6	/* LPE req SSP clock to 25MHz  */

#endif
