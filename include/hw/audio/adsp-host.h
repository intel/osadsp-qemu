/* Core IA host support for Intel audio DSPs.
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

#ifndef __HW_I386_ADSP_HOST_H__
#define __HW_I386_ADSP_HOST_H__

#include "qemu/osdep.h"
#include <mqueue.h>
#include "sysemu/sysemu.h"
#include "hw/acpi/aml-build.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "elf.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "exec/hwaddr.h"
#include "hw/char/serial.h"
#include "net/net.h"
#include "hw/sysbus.h"
#include "hw/block/flash.h"
#include "sysemu/block-backend.h"
#include "sysemu/char.h"
#include "sysemu/device_tree.h"
#include "qemu/error-report.h"
#include "qemu/io-bridge.h"

#include "hw/i386/pc.h"
#include "hw/i386/ioapic.h"
#include "hw/i386/ioapic_internal.h"
#include "hw/adsp/hw.h"
#include "hw/adsp/log.h"

#define SST_DEV_ID_BAYTRAIL          0x0F28
#define SST_DEV_ID_CHERRYTRAIL       0x22A8

struct adsp_dma_buffer {
    int chan;
    uint32_t *io;
    struct adsp_mem_desc shm_desc;
    char *name;
};

struct adsp_host {

    PCIDevice dev;

    /* IO mapped from ACPI tables */
    uint32_t *pci_io;
    uint32_t *ram;

    /* shim IO, offsets derived */
    uint32_t *shim_io;

    uint32_t *mbox_io;

    struct adsp_dma_buffer dma_shm_buffer[ADSP_MAX_GP_DMAC][8];

    /* runtime CPU */
    MemoryRegion *system_memory;
    QemuOpts *machine_opts;
    qemu_irq irq;

    /* logging options */
    struct adsp_log *log;

    /* machine init data */
    const struct adsp_desc *desc;
    const char *cpu_model;
    const char *kernel_filename;
};

#define adsp_get_pdata(obj, type) \
    OBJECT_CHECK(struct adsp_host, (obj), type)

#define ADSP_HOST_MBOX_COUNT    6
extern const struct adsp_reg_desc adsp_host_mbox_map[ADSP_HOST_MBOX_COUNT];

void adsp_host_init(struct adsp_host *adsp, const struct adsp_desc *board);
void adsp_host_do_dma(struct adsp_host *adsp, struct qemu_io_msg *msg);
void adsp_host_init_mbox(struct adsp_host *adsp, const char *name);

#define ADSP_HOST_BYT_NAME        "adsp-byt"
#define ADSP_HOST_CHT_NAME        "adsp-cht"

void adsp_byt_init_pci(struct adsp_host *adsp);
void adsp_byt_pci_realize(PCIDevice *pci_dev, Error **errp);
void adsp_cht_pci_realize(PCIDevice *pci_dev, Error **errp);
void adsp_byt_pci_exit(PCIDevice *pci_dev);
void adsp_byt_init_shim(struct adsp_host *adsp, const char *name);
void adsp_byt_host_init(struct adsp_host *adsp, const char *name);
void build_acpi_byt_adsp_devices(Aml *table);

#endif
