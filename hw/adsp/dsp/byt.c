/* Core DSP support for Baytrail audio DSP.
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
#include "qemu/timer.h"
#include "sysemu/sysemu.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/hw.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "hw/sysbus.h"
#include "qemu/error-report.h"
#include "qemu/io-bridge.h"

#include "hw/audio/adsp-dev.h"
#include "hw/adsp/shim.h"
#include "hw/adsp/log.h"
#include "hw/adsp/byt.h"
#include "hw/ssi/ssp.h"
#include "hw/dma/dw-dma.h"
#include "mbox.h"
#include "byt.h"
#include "common.h"

static void adsp_reset(void *opaque)
{

}

static void init_memory(struct adsp_dev *adsp, const char *name)
{
    MemoryRegion *iram, *dram0;
    const struct adsp_desc *board = adsp->desc;
    void *ptr = NULL;
    char shm_name[32];
    int err, i;
    uint32_t *uptr;

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

    /* set memory to non zero values */
    uptr = ptr;
    for (i = 0; i < board->iram.size >> 2; i++)
        uptr[i] = 0x5a5a5a5a;

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

    /* set memory to non zero values */
    uptr = ptr;
    for (i = 0; i < board->dram0.size >> 2; i++)
        uptr[i] = 0x6b6b6b6b;
}

static void adsp_pm_msg(struct adsp_dev *adsp, struct qemu_io_msg *msg)
{
}

static int bridge_cb(void *data, struct qemu_io_msg *msg)
{
    struct adsp_dev *adsp = (struct adsp_dev *)data;

    log_text(adsp->log, LOG_MSGQ,
            "msg: id %d msg %d size %d type %d\n",
            msg->id, msg->msg, msg->size, msg->type);

    switch (msg->type) {
    case QEMU_IO_TYPE_REG:
        /* mostly handled by SHM, some exceptions */
        adsp_byt_shim_msg(adsp, msg);
        break;
    case QEMU_IO_TYPE_IRQ:
        adsp_byt_irq_msg(adsp, msg);
        break;
    case QEMU_IO_TYPE_PM:
        adsp_pm_msg(adsp, msg);
        break;
    case QEMU_IO_TYPE_DMA:
        dw_dma_msg(msg);
        break;
    case QEMU_IO_TYPE_MEM:
    default:
        break;
    }

    return 0;
}

static struct adsp_dev *adsp_init(const struct adsp_desc *board,
    MachineState *machine, const char *name)
{
    struct adsp_dev *adsp;
    uint8_t *ldata;
    size_t lsize;
    int n;

    adsp = g_malloc(sizeof(*adsp));
    adsp->log = log_init(NULL);    /* TODO: add log name to cmd line */
    adsp->desc = board;
    adsp->system_memory = get_system_memory();
    adsp->machine_opts = qemu_get_machine_opts();
    adsp->cpu_model = machine->cpu_model;
    adsp->kernel_filename = qemu_opt_get(adsp->machine_opts, "kernel");

    /* initialise CPU */
    if (!adsp->cpu_model) {
        adsp->cpu_model = XTENSA_DEFAULT_CPU_MODEL;
    }

    for (n = 0; n < smp_cpus; n++) {
        adsp->xtensa[n] = g_malloc(sizeof(struct adsp_xtensa));
        adsp->xtensa[n]->cpu = cpu_xtensa_init(adsp->cpu_model);

        if (adsp->xtensa[n]->cpu == NULL) {
            error_report("unable to find CPU definition '%s'",
                adsp->cpu_model);
            exit(EXIT_FAILURE);
        }

        adsp->xtensa[n]->env = &adsp->xtensa[n]->cpu->env;
        adsp->xtensa[n]->env->sregs[PRID] = n;
        qemu_register_reset(adsp_reset, adsp->xtensa[n]->cpu);

        /* Need MMU initialized prior to ELF loading,
        * so that ELF gets loaded into virtual addresses
        */
        cpu_reset(CPU(adsp->xtensa[n]->cpu));
    }

    init_memory(adsp, name);

    /* init peripherals */
    adsp_byt_shim_init(adsp, name);
    adsp_mbox_init(adsp, name);
    dw_dma_init_dev(adsp, adsp->system_memory, board->gp_dmac_dev, board->num_dmac);
    adsp_ssp_init(adsp->system_memory, board->ssp_dev, board->num_ssp);

    /* reset all devices to init state */
    qemu_devices_reset();

    /* initialise bridge to x86 host driver */
    qemu_io_register_child(name, &bridge_cb, (void*)adsp);

    /* load binary file if one is specified on cmd line otherwise finish */
    if (adsp->kernel_filename == NULL) {
        printf(" ** Baytrail Xtensa HiFi2 EP DSP initialised.\n"
            " ** Waiting for host to load firmware...\n");
        return adsp;
    }

    /* load the binary image and copy to IRAM */
    ldata = g_malloc(0x40000 + board->dram0.size);
    lsize = load_image_size(adsp->kernel_filename, ldata,
        0x40000 + board->dram0.size);

    /* copy to IRAM */
    cpu_physical_memory_write(board->iram.base, ldata, board->iram.size);

    /* copy DRAM section */
    cpu_physical_memory_write(board->dram0.base, ldata + 0x40000,
        lsize - 0x40000);

    return adsp;
}

/* hardware memory map */
static const struct adsp_desc byt_dsp_desc = {
    .ia_irq = IRQ_NUM_EXT_IA,
    .ext_timer_irq = IRQ_NUM_EXT_TIMER,
    .pmc_irq = IRQ_NUM_EXT_PMC,

    .num_ssp = 3,
    .num_dmac = 2,
    .iram = {.base = ADSP_BYT_DSP_IRAM_BASE, .size = ADSP_BYT_IRAM_SIZE},
    .dram0 = {.base = ADSP_BYT_DSP_DRAM_BASE, .size = ADSP_BYT_DRAM_SIZE},

    .mbox_dev = {
        .name = "mbox",
        .reg_count = ARRAY_SIZE(adsp_mbox_map),
        .reg = adsp_mbox_map,
        .desc = {.base = ADSP_BYT_DSP_MAILBOX_BASE,
                 .size = ADSP_MAILBOX_SIZE},
    },
    .shim_dev = {
        .name = "shim",
        .reg_count = ARRAY_SIZE(adsp_byt_shim_map),
        .reg = adsp_byt_shim_map,
        .desc = {.base = ADSP_BYT_DSP_SHIM_BASE,
                .size = ADSP_BYT_SHIM_SIZE},
    },
    .gp_dmac_dev[0] = {
        .name = "dmac0",
        .irq = IRQ_NUM_EXT_DMAC0,
        .reg_count = ARRAY_SIZE(adsp_gp_dma_map),
        .reg = adsp_gp_dma_map,
        .desc = {.base = ADSP_BYT_DMA0_BASE,
                .size = ADSP_BYT_DMA0_SIZE},
    },
    .gp_dmac_dev[1] = {
        .name = "dmac1",
        .irq = IRQ_NUM_EXT_DMAC1,
        .reg_count = ARRAY_SIZE(adsp_gp_dma_map),
        .reg = adsp_gp_dma_map,
        .desc = {.base = ADSP_BYT_DMA1_BASE,
                .size = ADSP_BYT_DMA1_SIZE},
    },
    .ssp_dev[0] = {
        .name = "ssp0",
        .irq = IRQ_NUM_EXT_SSP0,
        .reg_count = ARRAY_SIZE(adsp_ssp_map),
        .reg = adsp_ssp_map,
        .desc = {.base = ADSP_BYT_SSP0_BASE,
                .size = ADSP_BYT_SSP0_SIZE},
    },
    .ssp_dev[1] = {
        .name = "ssp1",
        .irq = IRQ_NUM_EXT_SSP1,
        .reg_count = ARRAY_SIZE(adsp_ssp_map),
        .reg = adsp_ssp_map,
        .desc = {.base = ADSP_BYT_SSP1_BASE,
                .size = ADSP_BYT_SSP1_SIZE},
    },
    .ssp_dev[2] = {
        .name = "ssp2",
        .irq = IRQ_NUM_EXT_SSP2,
        .reg_count = ARRAY_SIZE(adsp_ssp_map),
        .reg = adsp_ssp_map,
        .desc = {.base = ADSP_BYT_SSP2_BASE,
                .size = ADSP_BYT_SSP2_SIZE},
    },
};


static void byt_adsp_init(MachineState *machine)
{
    struct adsp_dev *adsp;

    adsp = adsp_init(&byt_dsp_desc, machine, "byt");

    adsp->ext_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, &byt_ext_timer_cb, adsp);
    adsp->ext_clk_kHz = 2500;
}

static void cht_adsp_init(MachineState *machine)
{
    struct adsp_dev *adsp;

    adsp = adsp_init(&byt_dsp_desc, machine, "cht");

    adsp->ext_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, &byt_ext_timer_cb, adsp);
    adsp->ext_clk_kHz = 2500;
}

static void xtensa_byt_machine_init(MachineClass *mc)
{
    mc->desc = "Baytrail HiFi2";
    mc->is_default = true;
    mc->init = byt_adsp_init;
    mc->max_cpus = 1;
}

DEFINE_MACHINE("adsp_byt", xtensa_byt_machine_init)

static void xtensa_cht_machine_init(MachineClass *mc)
{
    mc->desc = "Cherrytrail HiFi2";
    mc->is_default = true;
    mc->init = cht_adsp_init;
    mc->max_cpus = 1;
}

DEFINE_MACHINE("adsp_cht", xtensa_cht_machine_init)
