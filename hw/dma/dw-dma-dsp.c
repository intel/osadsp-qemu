/* Virtualization support for DesignWare DMA Engine.
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

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "sysemu/sysemu.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "elf.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "qemu/thread.h"
#include "qemu/io-bridge.h"

#include "hw/pci/pci.h"
#include "hw/audio/adsp-dev.h"
#include "hw/audio/adsp-host.h"
#include "hw/adsp/shim.h"
#include "hw/adsp/log.h"
#include "hw/ssi/ssp.h"
#include "hw/dma/dw-dma.h"

static void dw_dsp_do_irq(struct adsp_gp_dmac *dmac, int enable)
{
    adsp_set_irq(dmac->adsp, dmac->desc->irq, enable);
}

void dw_dma_init_dev(struct adsp_dev *adsp, MemoryRegion *parent,
    const struct adsp_reg_space *dev, int num_dmac)
{
    MemoryRegion *reg_dmac;
    struct adsp_gp_dmac *dmac;
    char name[32];
    int i, j;

    for (i = 0; i < num_dmac; i++) {

        dmac = g_malloc(sizeof(*dmac));
        dmac->adsp = adsp;
        dmac->id = i;
        dmac->irq_assert = 0;
        dmac->is_pci_dev = 0;
        dmac->do_irq = dw_dsp_do_irq;
        dmac->log = log_init(NULL);
        dmac->desc = &dev[i];

        sprintf(name, "dmac%d.io", i);

        /* DMAC */
        reg_dmac = g_malloc(sizeof(*reg_dmac));
        dmac->io = g_malloc(dev[i].desc.size);
        memory_region_init_io(reg_dmac, NULL, &dw_dmac_ops, dmac,
            name, dev[i].desc.size);
        memory_region_add_subregion(parent, dev[i].desc.base, reg_dmac);
        qemu_register_reset(dw_dmac_reset, dmac);

        /* channels */
        for (j = 0; j < NUM_CHANNELS; j++) {
            dmac->dma_chan[j].dmac = dmac;
            dmac->dma_chan[j].fd = 0;
            dmac->dma_chan[j].chan = j;
            dmac->dma_chan[j].file_idx = 0;
            sprintf(dmac->dma_chan[j].thread_name, "dmac:%d.%d", i, j);
        }
    }
}

