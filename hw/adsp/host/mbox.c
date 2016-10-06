/* Core IA host support for  audio DSP mailbox.
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

#include "qemu/osdep.h"
#include "hw/audio/adsp-host.h"
#include "hw/adsp/shim.h"
#include "hw/adsp/log.h"

/* mailbox map - must be aligned with reef platform/mailbox.h */
const struct adsp_reg_desc adsp_host_mbox_map[ADSP_HOST_MBOX_COUNT] = {
    {.name = "outbox", .enable = LOG_MBOX_OUTBOX,
        .offset = 0x00000000, .size = 0x400},
    {.name = "inbox", .enable = LOG_MBOX_INBOX,
        .offset = 0x00000400, .size = 0x400},
    {.name = "exception", .enable = LOG_MBOX_EXCEPTION,
        .offset = 0x00000800, .size = 0x100},
    {.name = "debug", .enable = LOG_MBOX_DEBUG,
        .offset = 0x00000900, .size = 0x100},
    {.name = "stream", .enable = LOG_MBOX_STREAM,
        .offset = 0x00000a00, .size = 0x200},
    {.name = "trace", .enable = LOG_MBOX_TRACE,
        .offset = 0x00000c00, .size = 0x400},
};

static uint64_t adsp_mbox_read(void *opaque, hwaddr addr,
        unsigned size)
{
    struct adsp_host *adsp = opaque;

    log_area_read(adsp->log, &adsp->desc->mbox_dev,
            addr, size, adsp->mbox_io[addr >> 2]);

    return adsp->mbox_io[addr >> 2];
}

static void adsp_mbox_write(void *opaque, hwaddr addr,
        uint64_t val, unsigned size)
{
    struct adsp_host *adsp = opaque;

    log_area_write(adsp->log, &adsp->desc->mbox_dev,
                addr, val, size, adsp->mbox_io[addr >> 2]);

    adsp->mbox_io[addr >> 2] = val;
}

static const MemoryRegionOps adsp_mbox_ops = {
    .read = adsp_mbox_read,
    .write = adsp_mbox_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

void adsp_host_init_mbox(struct adsp_host *adsp, const char *name)
{
    MemoryRegion *mbox;
    void *ptr = NULL;
    char mbox_name[32];
    const struct adsp_desc *board = adsp->desc;
    int err;

    /* shim reg space  - shared via MSQ */
    mbox = g_malloc(sizeof(*mbox));

    sprintf(mbox_name, "%s-mbox", name);
    err = qemu_io_register_shm(mbox_name, ADSP_IO_SHM_MBOX,
        board->mbox_dev.desc.size, &ptr);
    if (err < 0)
        fprintf(stderr, "error: cant alloc SHIM SHM %d\n", err);

    adsp->mbox_io = ptr;
    memory_region_init_io(mbox, NULL, &adsp_mbox_ops, adsp,
        "mbox.io", board->mbox_dev.desc.size);
    memory_region_add_subregion(adsp->system_memory,
        board->mbox_dev.desc.base, mbox);
}
