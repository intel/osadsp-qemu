/* Core common support for audio DSP mailbox.
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

#ifndef __ADSP_MBOX_H__
#define __ADSP_MBOX_H__

struct adsp_dev;
struct adsp_gp_dmac;
struct adsp_log;

#define ADSP_MBOX_AREAS        6
extern const struct adsp_reg_desc adsp_mbox_map[ADSP_MBOX_AREAS];

void adsp_mbox_init(struct adsp_dev *adsp, const char *name);

#endif
