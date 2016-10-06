/* Core register IO tracing
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

#ifndef __ADSP_XTENSA_H__
#define __ADSP_XTENSA_H__

#include "hw/adsp/hw.h"

struct adsp_xtensa;
#define ADSP_MAX_CORES	4

struct adsp_dev {

	/* IO - memory space */
	uint32_t *mbox_io;
	uint32_t *shim_io;

	/* runtime CPU */
	struct adsp_xtensa *xtensa[ADSP_MAX_CORES];
	bool cpu_stalled;
	bool in_reset;
	MemoryRegion *system_memory;
	QemuOpts *machine_opts;

	/* machine init data */
	const struct adsp_desc *desc;
	const char *cpu_model;
	const char *kernel_filename;

	/* logging options */
	struct adsp_log *log;

	/* ext timer */
	QEMUTimer *ext_timer;
	uint32_t ext_clk_kHz;
	int64_t ext_timer_start;

	/* GP DMA */
	struct adsp_gp_dmac *gp_dmac[ADSP_MAX_GP_DMAC];

	/* SSP */
	struct adsp_ssp *ssp[ADSP_MAX_SSP];

	/* PMC */
	QemuThread pmc_thread;
	uint32_t pmc_cmd;
};

void adsp_set_irq(struct adsp_dev *adsp, int irq, int active);

#endif
