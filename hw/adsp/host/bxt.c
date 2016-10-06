/* Core IA host support for Broxton audio DSP.
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

#include "hw/audio/adsp-host.h"


/* hardware memory map */
static const struct adsp_desc bxt_board = {
    .iram = {.base = 0xFE4c0000, .size = 0x14000},
    .dram0 = {.base = 0xFE500000, .size = 0x28000},
    .pci =  {.base = 0xFE830000, .size = 0x1000},

    .shim_dev = {
        .name = "shim",
        .reg_count = ARRAY_SIZE(adsp_byt_shim_map),
        .reg = adsp_byt_shim_map,
        .desc = {.base = 0xFE540000, .size = 0x4000},
    },

    .mbox_dev = {
        .name = "mbox",
        .reg_count = ARRAY_SIZE(adsp_host_mbox_map),
        .reg = adsp_host_mbox_map,
        .desc = {.base = 0xFE544000, .size = 0x1000},
    },
};

static void do_irq(struct adsp_host *adsp, struct qemu_io_msg *msg)
{
    uint32_t active;

    active = adsp->shim_io[SHIM_ISRX >> 2] & ~(adsp->shim_io[SHIM_IMRX >> 2]);

    log_text(adsp->log, LOG_IRQ_ACTIVE,
        "DSP IRQ: status %x mask %x active %x cmd %x\n",
        adsp->shim_io[SHIM_ISRX >> 2],
        adsp->shim_io[SHIM_IMRX >> 2], active,
        adsp->shim_io[SHIM_IPCD >> 2]);

    if (active) {
        pci_set_irq(&adsp->dev, 1);
    }
}

static void do_pm(struct qemu_io_msg *msg)
{
}

static int bxt_bridge_cb(void *data, struct qemu_io_msg *msg)
{
    struct adsp_host *adsp = (struct adsp_host *)data;

    switch (msg->type) {
    case QEMU_IO_TYPE_REG:
        /* mostly handled by SHM, some exceptions */
        break;
    case QEMU_IO_TYPE_IRQ:
        do_irq(adsp, msg);
        break;
    case QEMU_IO_TYPE_PM:
        do_pm(msg);
        break;
    case QEMU_IO_TYPE_DMA:
        adsp_host_do_dma(adsp, msg);
        break;
    case QEMU_IO_TYPE_MEM:
    default:
        break;
    }
    return 0;
}

static void init_memory(struct adsp_host *adsp, const char *name)
{
    MemoryRegion *iram, *dram0;
    const struct adsp_desc *board = adsp->desc;
    char shm_name[32];
    void *ptr;
    int err;

    /* IRAM -shared via SHM */
    sprintf(shm_name, "%s-iram", name);
    err = qemu_io_register_shm(shm_name, ADSP_IO_SHM_IRAM,
        board->iram.size, &ptr);
    if (err < 0)
        fprintf(stderr, "error: cant alloc IRAM SHM %d\n", err);
    iram = g_malloc(sizeof(*iram));
    memory_region_init_ram_ptr(iram, NULL, "lpe.iram", board->iram.size, ptr);
    vmstate_register_ram_global(iram);
    memory_region_add_subregion(adsp->system_memory,
        board->iram.base, iram);

    /* DRAM0 - shared via SHM */
    sprintf(shm_name, "%s-dram", name);
    err = qemu_io_register_shm(shm_name, ADSP_IO_SHM_DRAM,
        board->dram0.size, &ptr);
    if (err < 0)
        fprintf(stderr, "error: cant alloc DRAM SHM %d\n", err);
    dram0 = g_malloc(sizeof(*dram0));
    memory_region_init_ram_ptr(dram0, NULL, "lpe.dram0", board->dram0.size, ptr);
    vmstate_register_ram_global(dram0);
    memory_region_add_subregion(adsp->system_memory,
        board->dram0.base, dram0);

}

void adsp_bxt_host_init(struct adsp_host *adsp, const char *name)
{
    adsp->desc = &bxt_board;
    adsp->system_memory = get_system_memory();
    adsp->machine_opts = qemu_get_machine_opts();

    adsp->log = log_init(NULL);    /* TODO: add log name to cmd line */

    init_memory(adsp, name);
    adsp_bxt_init_pci(adsp);
    adsp_bxt_init_shim(adsp, name);
    adsp_host_init_mbox(adsp, name);

    /* initialise bridge to x86 host driver */
    qemu_io_register_parent(name, &bxt_bridge_cb, (void*)adsp);
}

static void bxt_reset(DeviceState *dev)
{
}

static Property bxt_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void bxt_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

    k->realize = adsp_bxt_pci_realize;
    k->exit = adsp_bxt_pci_exit;

    k->vendor_id = PCI_VENDOR_ID_INTEL;
    k->device_id = SST_DEV_ID_BROXTON_P;
    k->revision = 1;

    k->class_id = PCI_CLASS_MULTIMEDIA_AUDIO;
    set_bit(DEVICE_CATEGORY_SOUND, dc->categories);

    dc->desc = "Intel Audio DSP Broxton";
    dc->reset = bxt_reset;
    dc->props = bxt_properties;
}

static void bxt_instance_init(Object *obj)
{

}

static const TypeInfo adsp_base_info = {
    .name          = ADSP_HOST_BXT_NAME,
    .parent        = TYPE_PCI_DEVICE,
    .instance_size = sizeof(struct adsp_host),
    .instance_init = bxt_instance_init,
    .class_init    = bxt_class_init,
};

static void adsp_bxt_register_types(void)
{
    type_register_static(&adsp_base_info);
}

type_init(adsp_bxt_register_types)
