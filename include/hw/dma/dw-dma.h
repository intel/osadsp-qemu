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

#ifndef __HW_DW_DMA_H__
#define __HW_DW_DMA_H__

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "hw/acpi/aml-build.h"
#include "sysemu/sysemu.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/pci/pci.h"
#include "elf.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "qemu/thread.h"
#include "qemu/io-bridge.h"

struct adsp_dev;
struct adsp_host;
struct adsp_gp_dmac;
struct adsp_log;

#define DW_DMA_PCI_ID		0x9c60

#define NUM_CHANNELS        8

/* channel registers */
#define DW_MAX_CHAN         8
#define DW_CH_SIZE          0x58
#define BYT_CHAN_OFFSET(chan) \
    (DW_CH_SIZE * chan)

#define DW_SAR(chan)    \
    (0x0000 + BYT_CHAN_OFFSET(chan))
#define DW_DAR(chan) \
    (0x0008 + BYT_CHAN_OFFSET(chan))
#define DW_LLP(chan) \
    (0x0010 + BYT_CHAN_OFFSET(chan))
#define DW_CTRL_LOW(chan) \
    (0x0018 + BYT_CHAN_OFFSET(chan))
#define DW_CTRL_HIGH(chan) \
    (0x001C + BYT_CHAN_OFFSET(chan))
#define DW_CFG_LOW(chan) \
    (0x0040 + BYT_CHAN_OFFSET(chan))
#define DW_CFG_HIGH(chan) \
    (0x0044 + BYT_CHAN_OFFSET(chan))

/* registers */
#define DW_STATUS_TFR           0x02E8
#define DW_STATUS_BLOCK         0x02F0
#define DW_STATUS_SRC_TRAN      0x02F8
#define DW_STATUS_DST_TRAN      0x0300
#define DW_STATUS_ERR           0x0308
#define DW_RAW_TFR          0x02C0
#define DW_RAW_BLOCK            0x02C8
#define DW_RAW_SRC_TRAN         0x02D0
#define DW_RAW_DST_TRAN         0x02D8
#define DW_RAW_ERR          0x02E0
#define DW_MASK_TFR         0x0310
#define DW_MASK_BLOCK           0x0318
#define DW_MASK_SRC_TRAN        0x0320
#define DW_MASK_DST_TRAN        0x0328
#define DW_MASK_ERR         0x0330
#define DW_CLEAR_TFR            0x0338
#define DW_CLEAR_BLOCK          0x0340
#define DW_CLEAR_SRC_TRAN       0x0348
#define DW_CLEAR_DST_TRAN       0x0350
#define DW_CLEAR_ERR            0x0358
#define DW_INTR_STATUS          0x0360
#define DW_DMA_CFG          0x0398
#define DW_DMA_CHAN_EN          0x03A0
#define DW_FIFO_PART0_LO        0x0400
#define DW_FIFO_PART0_HI        0x0404
#define DW_FIFO_PART1_LO        0x0408
#define DW_FIFO_PART1_HI        0x040C
#define DW_CH_SAI_ERR           0x0410

/* channel bits */
#define INT_MASK(chan)          (0x100 << chan)
#define INT_UNMASK(chan)        (0x101 << chan)
#define CHAN_ENABLE(chan)       (0x101 << chan)
#define CHAN_DISABLE(chan)      (0x100 << chan)
#define CHAN_WRITE_ENABLE(chan) (0x100 << chan)
#define CHAN_RAW_ENABLE(chan)   (0x1 << chan)

#define DW_CFG_CH_SUSPEND       0x100
#define DW_CFG_CH_DRAIN         0x400
#define DW_CFG_CH_FIFO_EMPTY        0x200

/* CTL_LO */
#define DW_CTLL_INT_EN          (1 << 0)
#define DW_CTLL_DST_WIDTH(x)        (x << 1)
#define DW_CTLL_SRC_WIDTH(x)        (x << 4)
#define DW_CTLL_DST_INC         (0 << 7)
#define DW_CTLL_DST_DEC         (1 << 7)
#define DW_CTLL_DST_FIX         (2 << 7)
#define DW_CTLL_SRC_INC         (0 << 9)
#define DW_CTLL_SRC_DEC         (1 << 9)
#define DW_CTLL_SRC_FIX         (2 << 9)
#define DW_CTLL_DST_MSIZE(x)        (x << 11)
#define DW_CTLL_SRC_MSIZE(x)        (x << 14)
#define DW_CTLL_S_GATH_EN       (1 << 17)
#define DW_CTLL_D_SCAT_EN       (1 << 18)
#define DW_CTLL_FC(x)           (x << 20)
#define DW_CTLL_FC_M2M          (0 << 20)
#define DW_CTLL_FC_M2P          (1 << 20)
#define DW_CTLL_FC_P2M          (2 << 20)
#define DW_CTLL_FC_P2P          (3 << 20)
#define DW_CTLL_DMS(x)          (x << 23)
#define DW_CTLL_SMS(x)          (x << 25)
#define DW_CTLL_LLP_D_EN        (1 << 27)
#define DW_CTLL_LLP_S_EN        (1 << 28)

/* CTL_HI */
#define DW_CTLH_DONE            0x00020000
#define DW_CTLH_BLOCK_TS_MASK       0x0001ffff

/* CFG_LO */
#define DW_CFGL_SRC_RELOAD      (1 << 30)
#define DW_CFGL_DST_RELOAD      (1 << 31)
#define DW_CFGL_CTL_HI_UPD_EN   (1 << 5)
#define DW_CFGL_DS_UPD_EN       (1 << 6)
#define DW_CFGL_SS_UPD_EN       (1 << 7)


/* CFG_HI */
#define DW_CFGH_SRC_PER(x)      (x << 7)
#define DW_CFGH_DST_PER(x)      (x << 11)

#define DW_INTR_STATUS_BLOCK    (1 << 1)
#define DW_INTR_STATUS_TFR      (1 << 0)

/* DW DMA on Host */
#define DWDMA_HOST_PCI_BASE             0xF3000000
#define DWDMA_HOST_PCI_SIZE             0x00001000
#define DWDMA_HOST_DMAC_SIZE            0x00001000
#define DWDMA_HOST_DMAC_BASE(x)         \
    (DWDMA_HOST_PCI_BASE + x * DWDMA_HOST_DMAC_SIZE)

/* DMA descriptor used by HW version 2 */
struct dw_lli2 {
    uint32_t sar;
    uint32_t dar;
    uint32_t llp;
    uint32_t ctrl_lo;
    uint32_t ctrl_hi;
    uint32_t sstat;
    uint32_t dstat;
} __attribute__ ((packed));

/* context pointer used by timer callbacks */
struct dma_chan {
    struct adsp_gp_dmac *dmac;
    int chan;

    /* buffer */
    uint32_t bytes;
    void *ptr;
    void *base;
    uint32_t tbytes;

    /* endpoint */
    struct qemu_io_msg_dma32 dma_msg;
    int ssp;

    /* file output/input */
    int fd;
    int file_idx;

    /* threading */
    QemuThread thread;
    char thread_name[32];
    uint32_t stop;
};

struct adsp_gp_dmac {
    int id;
    int num_chan;
    uint32_t *io;
    int irq_assert;
    int irq;
    int is_pci_dev;
    void (*do_irq)(struct adsp_gp_dmac *dmac, int enable);
    struct adsp_log *log;

    const struct adsp_reg_space *desc;
    struct adsp_dev *adsp;
    struct dw_host *dw_host;
    struct dma_chan dma_chan[NUM_CHANNELS];
};

struct dw_desc {
    const char *name;    /* device name */
    int ia_irq;
    struct adsp_mem_desc pci;

    /* devices */
    int num_dmac;
    struct adsp_reg_space gp_dmac_dev[6];
    struct adsp_reg_space pci_dev;
};

struct dw_host {
    PCIDevice dev;
    MemoryRegion *system_memory;
    qemu_irq irq;
    struct adsp_log *log;
    const struct dw_desc *desc;
    uint32_t *pci_io;
};

#define ADSP_GP_DMA_REGS		1
extern const struct adsp_reg_desc adsp_gp_dma_map[ADSP_GP_DMA_REGS];

extern const MemoryRegionOps dw_dmac_ops;

void build_acpi_dwdma_device(Aml *table);
void dw_dma_init_dev(struct adsp_dev *adsp, MemoryRegion *parent,
    const struct adsp_reg_space *dev, int num_dmac);
void dw_dma_msg(struct qemu_io_msg *msg);
void dw_dmac_reset(void *opaque);

#endif
