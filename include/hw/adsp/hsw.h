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

#ifndef __HW_ADSP_HSW_H__
#define __HW_ADSP_HSW_H__

#include <sys/time.h>
#include <stdio.h>
#include <stdint.h>
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "exec/hwaddr.h"
#include "hw.h"

/* Haswell and Broadwell */
#define ADSP_HSW_PCI_BASE           0xF0200000
#define ADSP_HSW_MMIO_BASE          0xF0400000
#define ADSP_HSW_HOST_IRAM_BASE     (ADSP_HSW_MMIO_BASE + 0x00080000)
#define ADSP_HSW_HOST_DRAM_BASE     (ADSP_HSW_MMIO_BASE + 0x00000000)
#define ADSP_HSW_HOST_SHIM_BASE     (ADSP_HSW_MMIO_BASE + 0x000E7000)
#define ADSP_HSW_HOST_MAILBOX_BASE  (ADSP_HSW_HOST_DRAM_BASE + 0x0007E000)

#define ADSP_BDW_PCI_BASE           0xF0600000
#define ADSP_BDW_MMIO_BASE          0xF0800000
#define ADSP_BDW_HOST_IRAM_BASE     (ADSP_BDW_MMIO_BASE + 0x000A0000)
#define ADSP_BDW_HOST_DRAM_BASE     (ADSP_BDW_MMIO_BASE + 0x00000000)
#define ADSP_BDW_HOST_SHIM_BASE     (ADSP_BDW_MMIO_BASE + 0x000FB000)
#define ADSP_BDW_HOST_MAILBOX_BASE  (ADSP_BDW_HOST_DRAM_BASE + 0x0009E000)

#define ADSP_HSW_DSP_SHIM_BASE      0xFFFE7000
#define ADSP_BDW_DSP_SHIM_BASE      0xFFFFB000
#define ADSP_HSW_SHIM_SIZE          0x00001000

#define ADSP_BDW_DSP_MAILBOX_BASE   0x0049E000
#define ADSP_HSW_DSP_MAILBOX_BASE   0x0047E000

#define ADSP_BDW_DMA0_BASE          0xFFFFE000
#define ADSP_BDW_DMA1_BASE          0xFFFFF000
#define ADSP_HSW_DMA0_BASE          0xFFFF0000
#define ADSP_HSW_DMA1_BASE          0xFFFF8000
#define ADSP_HSW_DMA0_SIZE          0x00001000
#define ADSP_HSW_DMA1_SIZE          0x00001000

#define ADSP_BDW_SSP0_BASE          0xFFFFC000
#define ADSP_BDW_SSP1_BASE          0xFFFFD000
#define ADSP_HSW_SSP0_BASE          0xFFFE8000
#define ADSP_HSW_SSP1_BASE          0xFFFE9000
#define ADSP_HSW_SSP0_SIZE          0x00001000
#define ADSP_HSW_SSP1_SIZE          0x00001000

#define ADSP_HSW_DSP_IRAM_BASE      0x00000000
#define ADSP_HSW_DSP_DRAM_BASE      0x00400000

#define ADSP_HSW_IRAM_SIZE          0x50000
#define ADSP_HSW_DRAM_SIZE          0x80000

#define ADSP_BDW_DSP_IRAM_BASE      0x00000000
#define ADSP_BDW_DSP_DRAM_BASE      0x00400000
#define ADSP_BDW_IRAM_SIZE          0x50000
#define ADSP_BDW_DRAM_SIZE          0xA0000

#endif
