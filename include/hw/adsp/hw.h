/* Core Audio DSP
 *
 * Copyright (C) 2016 Intel Corporation
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __HW_ADSP_H__
#define __HW_ADSP_H__

#include <sys/time.h>
#include <stdio.h>
#include <stdint.h>
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "exec/hwaddr.h"

/* Generic constants */
#define ADSP_MAX_SSP				6
#define ADSP_MAX_GP_DMAC			3
#define ADSP_MAILBOX_SIZE			0x00001000
#define ADSP_MMIO_SIZE				0x00200000
#define ADSP_PCI_SIZE				0x00001000

struct adsp_dev;
struct adsp_gp_dmac;
struct adsp_log;

struct adsp_mem_desc {
	hwaddr base;
	size_t size;
};

/* Register descriptor */
struct adsp_reg_desc {
	const char *name;	/* register name */
	int enable;		/* enable logging for this register */
	uint32_t offset;	/* register offset */
	size_t size;		/* register/area size */
};

/* Device register space */
struct adsp_reg_space {
	const char *name;	/* device name */
	int reg_count;		/* number of registers */
	int irq;
	const struct adsp_reg_desc *reg;	/* array of register descriptors */
	struct adsp_mem_desc desc;
};

struct adsp_desc {
	const char *name;	/* machine name */

	/* IRQs */
	int ia_irq;
	int ext_timer_irq;
	int pmc_irq;

	/* memory regions */
	struct adsp_mem_desc iram;
	struct adsp_mem_desc dram0;
	struct adsp_mem_desc rom;
	struct adsp_mem_desc pci;

	/* devices */
	int num_ssp;
	int num_dmac;
	struct adsp_reg_space ssp_dev[ADSP_MAX_SSP];
	struct adsp_reg_space mbox_dev;
	struct adsp_reg_space shim_dev;
	struct adsp_reg_space gp_dmac_dev[ADSP_MAX_GP_DMAC];
	struct adsp_reg_space pci_dev;
};

#endif
