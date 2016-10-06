/* Core IA host SHIM support for Baytrail audio DSP.
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

#include "hw/adsp/hw.h"
#include "hw/audio/adsp-host.h"
#include "hw/adsp/log.h"

/* driver reads from the SHIM */
static uint64_t adsp_shim_read(void *opaque, hwaddr addr,
        unsigned size)
{
    struct adsp_host *adsp = opaque;

    log_read(adsp->log, &adsp->desc->shim_dev,
                addr, size, adsp->shim_io[addr >> 2]);

    switch (size) {
    case 4:
        return adsp->shim_io[addr >> 2];
    case 8:
        return ((uint64_t*)adsp->shim_io)[addr >> 3];
    default:
        printf("shim.io invalid read size %d at 0x%8.8x\n",
            size, (unsigned int)addr);
        return 0;
    }
}

/* driver writes to the SHIM */
static void adsp_shim_write(void *opaque, hwaddr addr,
        uint64_t val, unsigned size)
{
    struct adsp_host *adsp = opaque;
    struct qemu_io_msg_reg32 reg32;
    struct qemu_io_msg_irq irq;
    uint32_t active, isrd;

    log_write(adsp->log, &adsp->desc->shim_dev,
                    addr, val, size, adsp->shim_io[addr >> 2]);

    /* write value via SHM */
    adsp->shim_io[addr >> 2] = val;

    /* most IO is handled by SHM, but there are some exceptions */
    switch (addr) {
    case SHIM_IPCXH:

        /* now set/clear status bit */
        isrd = adsp->shim_io[SHIM_ISRD >> 2] & ~(SHIM_ISRD_DONE | SHIM_ISRD_BUSY);
        isrd |= val & SHIM_IPCX_BUSY ? SHIM_ISRD_BUSY : 0;
        isrd |= val & SHIM_IPCX_DONE ? SHIM_ISRD_DONE : 0;
        adsp->shim_io[SHIM_ISRD >> 2] = isrd;

        /* do we need to send an IRQ ? */
        if (val & SHIM_IPCX_BUSY) {

            log_text(adsp->log, LOG_IRQ_BUSY,
                "irq: send busy interrupt 0x%8.8lx\n", val);

            /* send IRQ to child */
            irq.hdr.type = QEMU_IO_TYPE_IRQ;
            irq.hdr.msg = QEMU_IO_MSG_IRQ;
            irq.hdr.size = sizeof(irq);
            irq.irq = 0;

            qemu_io_send_msg(&irq.hdr);
        }
        break;
    case SHIM_IPCDH:

        /* set/clear status bit */
        isrd = adsp->shim_io[SHIM_ISRD >> 2] &
            ~(SHIM_ISRD_DONE | SHIM_ISRD_BUSY);
        isrd |= val & SHIM_IPCD_BUSY ? SHIM_ISRD_BUSY : 0;
        isrd |= val & SHIM_IPCD_DONE ? SHIM_ISRD_DONE : 0;
        adsp->shim_io[SHIM_ISRD >> 2] = isrd;

        /* do we need to send an IRQ ? */
        if (val & SHIM_IPCD_DONE) {

            log_text(adsp->log, LOG_IRQ_DONE,
                "irq: send done interrupt 0x%8.8lx\n", val);

            /* send IRQ to child */
            irq.hdr.type = QEMU_IO_TYPE_IRQ;
            irq.hdr.msg = QEMU_IO_MSG_IRQ;
            irq.hdr.size = sizeof(irq);
            irq.irq = 0;

            qemu_io_send_msg(&irq.hdr);
        }
        break;
    case SHIM_IMRX:
        /* write value via SHM */
        adsp->shim_io[addr >> 2] = val;

        active = adsp->shim_io[SHIM_ISRX >> 2] &
            ~(adsp->shim_io[SHIM_IMRX >> 2]);

        log_text(adsp->log, LOG_IRQ_ACTIVE,
            "irq: masking %x mask %x active %x\n",
            adsp->shim_io[SHIM_ISRD >> 2],
            adsp->shim_io[SHIM_IMRD >> 2], active);

        if (!active) {
            pci_set_irq(&adsp->dev, 0);
        }
        break;
    case SHIM_CSR:
        /* now send msg to DSP VM to notify register write */
        reg32.hdr.type = QEMU_IO_TYPE_REG;
        reg32.hdr.msg = QEMU_IO_MSG_REG32W;
        reg32.hdr.size = sizeof(reg32);
        reg32.reg = addr;
        reg32.val = val;
        qemu_io_send_msg(&reg32.hdr);
        break;
    default:
        break;
    }
}

static const MemoryRegionOps adsp_shim_ops = {
    .read = adsp_shim_read,
    .write = adsp_shim_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

void adsp_byt_init_shim(struct adsp_host *adsp, const char *name)
{
    MemoryRegion *shim;
    void *ptr = NULL;
    char shim_name[32];
    const struct adsp_desc *board = adsp->desc;
    int err;

    /* shim reg space  - shared via MSQ */
    shim = g_malloc(sizeof(*shim));

    sprintf(shim_name, "%s-shim", name);
    err = qemu_io_register_shm(shim_name, ADSP_IO_SHM_SHIM,
        board->shim_dev.desc.size, &ptr);
    if (err < 0)
        fprintf(stderr, "error: cant alloc SHIM SHM %d\n", err);

    adsp->shim_io = ptr;
    memory_region_init_io(shim, NULL, &adsp_shim_ops, adsp,
        "shim.io", board->shim_dev.desc.size);
    memory_region_add_subregion(adsp->system_memory,
        board->shim_dev.desc.base, shim);
}
