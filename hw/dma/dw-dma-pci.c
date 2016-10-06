/* Virtualization support for DesignWare DMA Engine PCI devices.
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
#include <mqueue.h>
#include "sysemu/sysemu.h"
#include "hw/acpi/aml-build.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "elf.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "hw/sysbus.h"
#include "qemu/error-report.h"
#include "qemu/io-bridge.h"
#include "hw/pci/pci.h"

#include <hw/i386/pc.h>
#include <hw/i386/ioapic.h>
#include <hw/i386/ioapic_internal.h>
#include <hw/adsp/shim.h>
#include <hw/adsp/log.h>
#include "hw/dma/dw-dma.h"
#include "hw/audio/adsp-host.h"

#define DW_DEVICE_NAME        "dw-dma"
#define dw_get_pdata(obj, type) \
    OBJECT_CHECK(struct dw_host, (obj), type)

static const struct dw_desc dw_dev = {

    .pci =  {.base = DWDMA_HOST_PCI_BASE, .size = DWDMA_HOST_PCI_SIZE},

    .gp_dmac_dev[0] = {
        .name = "dmac0",
        .reg_count = ARRAY_SIZE(adsp_gp_dma_map),
        .reg = adsp_gp_dma_map,
        .desc = {.base = DWDMA_HOST_DMAC_BASE(0), .size = DWDMA_HOST_DMAC_SIZE},
    },
    .gp_dmac_dev[1] = {
        .name = "dmac1",
        .reg_count = ARRAY_SIZE(adsp_gp_dma_map),
        .reg = adsp_gp_dma_map,
        .desc = {.base = DWDMA_HOST_DMAC_BASE(1), .size = DWDMA_HOST_DMAC_SIZE},
    },
    .gp_dmac_dev[2] = {
        .name = "dmac2",
        .reg_count = ARRAY_SIZE(adsp_gp_dma_map),
        .reg = adsp_gp_dma_map,
        .desc = {.base = DWDMA_HOST_DMAC_BASE(2), .size = DWDMA_HOST_DMAC_SIZE},
    },
    .gp_dmac_dev[3] = {
        .name = "dmac3",
        .reg_count = ARRAY_SIZE(adsp_gp_dma_map),
        .reg = adsp_gp_dma_map,
        .desc = {.base = DWDMA_HOST_DMAC_BASE(3), .size = DWDMA_HOST_DMAC_SIZE},
    },
    .gp_dmac_dev[4] = {
        .name = "dmac4",
        .reg_count = ARRAY_SIZE(adsp_gp_dma_map),
        .reg = adsp_gp_dma_map,
        .desc = {.base = DWDMA_HOST_DMAC_BASE(4), .size = DWDMA_HOST_DMAC_SIZE},
    },
    .gp_dmac_dev[5] = {
        .name = "dmac5",
        .reg_count = ARRAY_SIZE(adsp_gp_dma_map),
        .reg = adsp_gp_dma_map,
        .desc = {.base = DWDMA_HOST_DMAC_BASE(5), .size = DWDMA_HOST_DMAC_SIZE},
    },
};

static void dw_host_do_irq(struct adsp_gp_dmac *dmac, int enable)
{
        pci_set_irq(&dmac->dw_host->dev, enable);
}

static uint64_t dw_pci_read(void *opaque, hwaddr addr,
        unsigned size)
{
    struct dw_host *dw = opaque;

    log_read(dw->log, &dw->desc->pci_dev,
        addr, size, dw->pci_io[addr >> 2]);

    return dw->pci_io[addr >> 2];
}

static void dw_pci_write(void *opaque, hwaddr addr,
        uint64_t val, unsigned size)
{
    struct dw_host *dw = opaque;

    log_write(dw->log, &dw->desc->pci_dev,
            addr, val, size, dw->pci_io[addr >> 2]);

    dw->pci_io[addr >> 2] = val;

    /* TODO: pass on certain writes to DSP as MQ messages e.g. PM */
}

static const MemoryRegionOps dw_pci_ops = {
    .read = dw_pci_read,
    .write = dw_pci_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

void build_acpi_dwdma_device(Aml *table)
{
    Aml *crs;
    Aml *method;
    Aml *scope = aml_scope("_SB");
    Aml *dev = aml_device("DMA0");
    Aml *zero = aml_int(0);
    Aml *one = aml_int(1);

    /* device info */
    aml_append(dev, aml_name_decl("_ADR", zero));
    aml_append(dev, aml_name_decl("_HID", aml_string("INT9c60")));
    aml_append(dev, aml_name_decl("_CID", aml_string("INT9c60")));
    aml_append(dev, aml_name_decl("_SUB", aml_string("80869c60")));
    aml_append(dev, aml_name_decl("_DDN",
        aml_string("Designware DMA - INT9c60")));
    aml_append(dev, aml_name_decl("_UID", one));

    /* _STA method */
    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_return(aml_int(0x0F)));
    aml_append(dev, method);

    /* resources */
    crs = aml_resource_template();
    aml_append(crs,
        aml_memory32_fixed(DWDMA_HOST_PCI_BASE,
                DWDMA_HOST_PCI_SIZE, AML_READ_ONLY));
    aml_append(crs,
        aml_memory32_fixed(DWDMA_HOST_DMAC_BASE(0),
                DWDMA_HOST_DMAC_SIZE, AML_READ_ONLY));
    aml_append(crs, aml_irq_no_flags(0xa));
    aml_append(dev, aml_name_decl("_CRS", crs));

    aml_append(scope, dev);
    aml_append(table, scope);
}

static void dw_pci_exit(PCIDevice *pci_dev)
{
}

static void dw_dmac_init(struct dw_host *dw, int id)
{
    MemoryRegion *mmio;
    char dmac_name[16];
    struct adsp_gp_dmac *dmac;
    const struct dw_desc *board = dw->desc;
    MemoryRegion *reg_dmac;
    char name[32];
    int j;
    void *ptr;
    int err;

    sprintf(dmac_name, "dmac%d", id);

    /* DMAC MMIO -shared via SHM */
    err = qemu_io_register_shm(dmac_name, ADSP_IO_SHM_DMAC(id),
            board->gp_dmac_dev[id].desc.size, &ptr);
    if (err < 0)
        fprintf(stderr, "error: cant alloc DMAC%d SHM %d\n", id, err);

    mmio = g_malloc(sizeof(*mmio));
    memory_region_init_ram_ptr(mmio, NULL, dmac_name,
        board->gp_dmac_dev[id].desc.size, ptr);
    //vmstate_register_ram_global(iram);
    memory_region_add_subregion(dw->system_memory,
            board->gp_dmac_dev[id].desc.base, mmio);

    dmac = g_malloc(sizeof(*dmac));
    dmac->id = id;
    dmac->irq_assert = 0;
    dmac->is_pci_dev = 1;
    dmac->do_irq = dw_host_do_irq;
    dmac->dw_host = dw;

    sprintf(name, "dmac%d.io", id);

    /* DMAC */
    reg_dmac = g_malloc(sizeof(*reg_dmac));
    dmac->io = g_malloc(dw->desc->gp_dmac_dev[id].desc.size);
    memory_region_init_io(reg_dmac, NULL, &dw_dmac_ops, dmac,
            name, dw->desc->gp_dmac_dev[id].desc.size);
    memory_region_add_subregion(dw->system_memory,
            dw->desc->gp_dmac_dev[id].desc.base, reg_dmac);
    qemu_register_reset(dw_dmac_reset, dmac);

    /* channels */
    for (j = 0; j < NUM_CHANNELS; j++) {
        dmac->dma_chan[j].dmac = dmac;
        dmac->dma_chan[j].fd = 0;
        dmac->dma_chan[j].chan = j;
        dmac->dma_chan[j].file_idx = 0;
        sprintf(dmac->dma_chan[j].thread_name, "dmac:%d.%d", id, j);
    }
}

static void dw_pci_realize(PCIDevice *pci_dev, Error **errp)
{
    struct dw_host *dw = dw_get_pdata(pci_dev,DW_DEVICE_NAME);
    MemoryRegion *pci;
    uint8_t *pci_conf;
    int i;

    pci_conf = dw->dev.config;
    pci_conf[PCI_INTERRUPT_PIN] = 2; /* interrupt pin A */

    dw->irq = pci_allocate_irq(&dw->dev);

    dw->desc = &dw_dev;
    dw->system_memory = get_system_memory();
    //dw->machine_opts = qemu_get_machine_opts();

    dw->log = log_init(NULL);    /* TODO: add log name to cmd line */

    /* PCI reg space  */
    pci = g_malloc(sizeof(*pci));
    dw->pci_io = g_malloc(dw_dev.pci.size);

    memory_region_init_io(pci, NULL, &dw_pci_ops, dw,
        "pci.io", dw_dev.pci.size);

    memory_region_add_subregion(dw->system_memory,
            dw_dev.pci.base, pci);
    //qemu_register_reset(pci_reset, dw);

    for (i = 0; i < dw_dev.num_dmac; i++)
        dw_dmac_init(dw, i);

    /* initialise bridge to x86 host driver */
    //qemu_io_register_parent(&dw_bridge_cb, (void*)dw);
}

static void dw_reset(DeviceState *dev)
{
}

static Property dw_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void dw_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = dw_pci_realize;
    k->exit = dw_pci_exit;

    k->vendor_id = PCI_VENDOR_ID_INTEL;
    k->device_id = DW_DMA_PCI_ID;
    k->revision = 1;

    k->class_id = PCI_CLASS_SYSTEM_OTHER;
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);

    dc->desc = "Designware DMA Engine";
    dc->reset = dw_reset;
    dc->props = dw_properties;
}

static void dw_instance_init(Object *obj)
{

}

static const TypeInfo dw_base_info = {
    .name          = DW_DEVICE_NAME,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(struct dw_host),
    .instance_init = dw_instance_init,
    .class_init    = dw_class_init,
};

static void dw_register_types(void)
{
    type_register_static(&dw_base_info);
}

type_init(dw_register_types)

