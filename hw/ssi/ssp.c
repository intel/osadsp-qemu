/* Virtualization support for SSP.
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

#include "qemu/io-bridge.h"
#include "hw/adsp/shim.h"
#include "hw/adsp/log.h"
#include "hw/ssi/ssp.h"

static struct adsp_ssp *ssp_port[ADSP_MAX_SSP];

const struct adsp_reg_desc adsp_ssp_map[ADSP_SSP_REGS] = {
    {.name = "ssp", .enable = LOG_SSP,
        .offset = 0x00000000, .size = 0x4000},
};

static void ssp_reset(void *opaque)
{
    struct adsp_ssp *ssp = opaque;
    const struct adsp_reg_space *ssp_dev = ssp->ssp_dev;

    memset(ssp->io, 0, ssp_dev->desc.size);
}

static uint64_t ssp_read(void *opaque, hwaddr addr,
        unsigned size)
{
    struct adsp_ssp *ssp = opaque;
    const struct adsp_reg_space *ssp_dev = ssp->ssp_dev;

    /* only print IO from guest */
    log_read(ssp->log, ssp_dev, addr, size,
            ssp->io[addr >> 2]);

    return ssp->io[addr >> 2];
}

static void ssp_write(void *opaque, hwaddr addr,
        uint64_t val, unsigned size)
{
    struct adsp_ssp *ssp = opaque;
    const struct adsp_reg_space *ssp_dev = ssp->ssp_dev;
    uint32_t set, clear;

    switch (addr) {
    case SSCR1:
        log_write(ssp->log, ssp_dev, addr, val, size,
                ssp->io[addr >> 2]);

        set = val & ~ssp->io[addr >> 2];
        clear = ~val & ssp->io[addr >> 2];

        ssp->io[addr >> 2] = val;

        /* open file if playback has been enabled */
        if (set & SSCR1_TSRE) {

            /* create filename */
            sprintf(ssp->tx.file_name, "/tmp/%s-play%d.wav",
                ssp->name, ssp->tx.index++);

            unlink(ssp->tx.file_name);
            ssp->tx.fd = open(ssp->tx.file_name, O_WRONLY | O_CREAT,
                S_IRUSR | S_IWUSR | S_IXUSR);

            if (ssp->tx.fd < 0) {
                fprintf(stderr, "cant open file %s %d\n",
                    ssp->tx.file_name, -errno);
                return;
            } else
                printf("%s opened %s for playback\n",
                    ssp->name, ssp->tx.file_name);

            ssp->tx.total_frames = 0;
        }

        /* close file if playback has finished */
        if (clear & SSCR1_TSRE) {
            printf("%s closed %s for playback at %d frames\n",
                ssp->name,
                ssp->tx.file_name, ssp->tx.total_frames);
            close(ssp->tx.fd);
            ssp->tx.fd = 0;
        }

        /* open file if capture has been enabled */
        if (set & SSCR1_RSRE) {

            /* create filename */
            sprintf(ssp->rx.file_name, "/tmp/%s-capture.wav",
                ssp->name);

            ssp->rx.fd = open(ssp->tx.file_name, O_RDONLY,
                S_IRUSR | S_IWUSR | S_IXUSR);

            if (ssp->rx.fd < 0) {
                fprintf(stderr, "cant open file %s %d\n",
                    ssp->rx.file_name, -errno);
                return;
            } else
                printf("%s opened %s for capture\n",
                    ssp->name, ssp->rx.file_name);

            ssp->rx.total_frames = 0;
        }

        /* close file if capture has finished */
        if (clear & SSCR1_RSRE) {
            printf("%s closed %s for capture at %d frames\n", ssp->name,
                ssp->rx.file_name, ssp->rx.total_frames);
            close(ssp->rx.fd);
            ssp->rx.fd = 0;
        }
        break;
    case SSDR:
        /* update counters */
        ssp->tx.total_frames =+ size;
        ssp->io[addr >> 2] = val;

        break;
    default:
        log_area_write(ssp->log, ssp_dev, addr, val, size,
                ssp->io[addr >> 2]);

        ssp->io[addr >> 2] = val;
        break;
    }
}

struct adsp_ssp *ssp_get_port(int port)
{
    return ssp_port[port];
}

static const MemoryRegionOps ssp_ops = {
    .read = ssp_read,
    .write = ssp_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

void adsp_ssp_init(MemoryRegion *system_memory,
    const struct adsp_reg_space *ssp_dev, int num_ssp)
{
    MemoryRegion *reg_ssp;
    struct adsp_ssp *ssp;
    int i;

    for (i = 0; i < num_ssp; i++) {
        ssp = g_malloc(sizeof(*ssp));

        ssp->tx.level = 0;
        ssp->rx.level = 0;
        ssp->ssp_dev = &ssp_dev[i];
        sprintf(ssp->name, "%s.io", ssp_dev[i].name);

        ssp->log = log_init(NULL);

        /* SSP */
        reg_ssp = g_malloc(sizeof(*reg_ssp));
        ssp->io = g_malloc(ssp_dev[i].desc.size);
        memory_region_init_io(reg_ssp, NULL, &ssp_ops, ssp,
            ssp->name, ssp_dev[i].desc.size);
        memory_region_add_subregion(system_memory,
            ssp_dev[i].desc.base, reg_ssp);
        qemu_register_reset(ssp_reset, ssp);
        ssp_port[i] = ssp;
    }
}
