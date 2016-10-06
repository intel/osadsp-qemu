/* Core DSP support for Baytrail audio DSP.
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

#ifndef __ADSP_BYT_H__
#define __ADSP_BYT_H__

/* IRQ numbers */
#define IRQ_NUM_SOFTWARE0    0
#define IRQ_NUM_TIMER1       1
#define IRQ_NUM_SOFTWARE1    2
#define IRQ_NUM_SOFTWARE2    3
#define IRQ_NUM_TIMER2       5
#define IRQ_NUM_SOFTWARE3    6
#define IRQ_NUM_TIMER3       7
#define IRQ_NUM_SOFTWARE4    8
#define IRQ_NUM_SOFTWARE5    9
#define IRQ_NUM_EXT_IA       10
#define IRQ_NUM_EXT_PMC      11
#define IRQ_NUM_SOFTWARE6    12
#define IRQ_NUM_EXT_DMAC0    13
#define IRQ_NUM_EXT_DMAC1    14
#define IRQ_NUM_EXT_TIMER    15
#define IRQ_NUM_EXT_SSP0     16
#define IRQ_NUM_EXT_SSP1     17
#define IRQ_NUM_EXT_SSP2     18
#define IRQ_NUM_NMI          20

struct adsp_dev;

void adsp_byt_shim_init(struct adsp_dev *adsp, const char *name);
void adsp_byt_shim_msg(struct adsp_dev *adsp, struct qemu_io_msg *msg);
void adsp_byt_irq_msg(struct adsp_dev *adsp, struct qemu_io_msg *msg);
void byt_ext_timer_cb(void *opaque);

#endif
