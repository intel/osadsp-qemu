/* Core DSP support for Broxton audio DSP.
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
#include "hw/ssi/ssp.h"
#include "hw/dma/dw-dma.h"
#include "hw/adsp/bxt.h"
#include "mbox.h"
#include "bxt.h"
#include "broxton.h"
#include "common.h"

static void adsp_reset(void *opaque)
{
}

static uint64_t io_read(void *opaque, hwaddr addr,
        unsigned size)
{
    struct adsp_dev *adsp = opaque;

    log_read(adsp->log, &adsp->desc->shim_dev, addr, size,
        adsp->shim_io[addr >> 2]);

    return adsp->common_io[addr >> 2];
}

/* SHIM IO from ADSP */
static void io_write(void *opaque, hwaddr addr,
        uint64_t val, unsigned size)
{
    struct adsp_dev *adsp = opaque;

    log_write(adsp->log, &adsp->desc->shim_dev, addr, val, size,
        adsp->common_io[addr >> 2]);
}

static const MemoryRegionOps io_ops = {
    .read = io_read,
    .write = io_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void init_memory(struct adsp_dev *adsp, const char *name)
{
    MemoryRegion *iram, *dram0, *lp_sram, *rom, *io;
    const struct adsp_desc *board = adsp->desc;
    void *ptr = NULL;
    char shm_name[32];
    int err, i;
    uint32_t *uptr;

    /* SRAM -shared via SHM (not shared on real HW) */
    sprintf(shm_name, "%s-l2-sram", name);
    err = qemu_io_register_shm(shm_name, ADSP_IO_SHM_IRAM,
        board->iram.size, &ptr);
    if (err < 0)
        fprintf(stderr, "error: cant alloc L2 SRAM SHM %d\n", err);
    iram = g_malloc(sizeof(*iram));
    memory_region_init_ram_ptr(iram, NULL, "lpe.l2_sram", board->iram.size, ptr);
    vmstate_register_ram_global(iram);
    memory_region_add_subregion(adsp->system_memory,
        board->iram.base, iram);

    /* set memory to non zero values */
    uptr = ptr;
    for (i = 0; i < board->iram.size >> 2; i++)
        uptr[i] = 0x5a5a5a5a;

    /* HP SRAM - shared via SHM (not shared on real HW) */
    sprintf(shm_name, "%s-hp-sram", name);
    err = qemu_io_register_shm(shm_name, ADSP_IO_SHM_DRAM,
        board->dram0.size, &ptr);
    if (err < 0)
        fprintf(stderr, "error: cant alloc HP SRAM SHM %d\n", err);
    dram0 = g_malloc(sizeof(*dram0));
    memory_region_init_ram_ptr(dram0, NULL, "lpe.hp_sram", board->dram0.size, ptr);
    vmstate_register_ram_global(dram0);
    memory_region_add_subregion(adsp->system_memory,
        board->dram0.base, dram0);

    /* set memory to non zero values */
    uptr = ptr;
    for (i = 0; i < board->dram0.size >> 2; i++)
        uptr[i] = 0x6b6b6b6b;

    /* LP SRAM - shared via SHM (not shared on real HW) */
    sprintf(shm_name, "%s-lp-sram", name);
    err = qemu_io_register_shm(shm_name, ADSP_IO_SHM_LP_SRAM,
        board->lp_sram.size, &ptr);
    if (err < 0)
        fprintf(stderr, "error: cant alloc LP SRAM SHM %d\n", err);
    lp_sram = g_malloc(sizeof(*lp_sram));
    memory_region_init_ram_ptr(lp_sram, NULL, "lpe.lp_sram", board->lp_sram.size, ptr);
    vmstate_register_ram_global(lp_sram);
    memory_region_add_subregion(adsp->system_memory,
        board->lp_sram.base, lp_sram);

   /* set memory to non zero values */
    uptr = ptr;
    for (i = 0; i < board->lp_sram.size >> 2; i++)
        uptr[i] = 0x7c7c7c7c;

    /* ROM - shared via SHM (not shared on real HW) */
    sprintf(shm_name, "%s-rom", name);
    err = qemu_io_register_shm(shm_name, ADSP_IO_SHM_ROM,
        board->rom.size, &ptr);
    if (err < 0)
        fprintf(stderr, "error: cant alloc ROM SHM %d\n", err);
    rom = g_malloc(sizeof(*rom));
    memory_region_init_ram_ptr(rom, NULL, "lpe.rom", board->rom.size, ptr);
    vmstate_register_ram_global(rom);
    memory_region_add_subregion(adsp->system_memory,
        board->rom.base, rom);

   /* set memory to non zero values */
    uptr = ptr;
    for (i = 0; i < board->rom.size >> 2; i++)
        uptr[i] = 0;

    /* SHIM and all IO  - shared via SHM */
    io = g_malloc(sizeof(*io));

    sprintf(shm_name, "%s-io", name);
    err = qemu_io_register_shm(shm_name, ADSP_IO_SHM_IO,
            board->io_dev.desc.size, &ptr);
    if (err < 0)
        fprintf(stderr, "error: cant alloc IO %s SHM %d\n", name, err);

    adsp->common_io = ptr;
    memory_region_init_io(io, NULL, &io_ops, adsp,
        "common.io", board->io_dev.desc.size);
    memory_region_add_subregion(adsp->system_memory,
        board->io_dev.desc.base, io);
}

static void adsp_pm_msg(struct adsp_dev *adsp, struct qemu_io_msg *msg)
{
}

static int bridge_cb(void *data, struct qemu_io_msg *msg)
{
    struct adsp_dev *adsp = (struct adsp_dev *)data;

    switch (msg->type) {
    case QEMU_IO_TYPE_REG:
        /* mostly handled by SHM, some exceptions */
        adsp_bxt_shim_msg(adsp, msg);
        break;
    case QEMU_IO_TYPE_IRQ:
        adsp_bxt_irq_msg(adsp, msg);
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
    struct fw_image_manifest *man;
    struct module *mod;
    struct adsp_fw_header *hdr;
    unsigned long foffset, soffset, ssize;
    int n, i, j;
    void *rom;

    adsp = g_malloc(sizeof(*adsp));
    adsp->log = log_init(NULL);    /* TODO: add log name to cmd line */
    adsp->desc = board;
    adsp->system_memory = get_system_memory();
    adsp->machine_opts = qemu_get_machine_opts();
    adsp->cpu_model = machine->cpu_model;
    adsp->kernel_filename = qemu_opt_get(adsp->machine_opts, "kernel");
    adsp->rom_filename = qemu_opt_get(adsp->machine_opts, "rom");

    /* initialise CPU */
    if (!adsp->cpu_model) {
        adsp->cpu_model = XTENSA_DEFAULT_CPU_MODEL;
    }

    for (n = 0; n < smp_cpus; n++) {
        adsp->xtensa[n] = g_malloc(sizeof(struct adsp_xtensa));

        adsp->xtensa[n]->cpu = XTENSA_CPU(cpu_create(machine->cpu_type));

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
    adsp_bxt_shim_init(adsp, name);
    adsp_mbox_init(adsp, name);
    dw_dma_init_dev(adsp, adsp->system_memory, board->gp_dmac_dev, 2);
    adsp_ssp_init(adsp->system_memory, board->ssp_dev, 2);

    /* reset all devices to init state */
    qemu_devices_reset();

    /* initialise bridge to x86 host driver */
    qemu_io_register_child(name, &bridge_cb, (void*)adsp);

    /* load binary file if one is specified on cmd line otherwise finish */
    if (adsp->kernel_filename == NULL) {
        printf(" ** Broxton Xtensa HiFi3 DSP initialised.\n"
            " ** Waiting for host to load firmware...\n");
        return adsp;
    }


    printf("now loading:\n kernel %s\n ROM %s\n",
        adsp->kernel_filename, adsp->rom_filename);

    /* load ROM image and copy to ROM */
    rom = g_malloc(ADSP_BXT_DSP_ROM_SIZE);
    load_image_size(adsp->rom_filename, rom,
        ADSP_BXT_DSP_ROM_SIZE);
    cpu_physical_memory_write(board->rom.base, rom, ADSP_BXT_DSP_ROM_SIZE);

    /* load the binary image and copy to SRAM */
    man = g_malloc(board->iram.size);
    load_image_size(adsp->kernel_filename, man,
        board->iram.size);

    // HACK for ext manifest
    //man = (void*)man + 0x708; // HACK for ext manifest

    hdr = &man->desc.header;

    /* copy module to SRAM */
   for (i = 0; i < hdr->num_module_entries; i++) {

	mod = &man->desc.module[i];
        printf("checking module %d\n", i);

        for (j = 0; j < 3; j++) {

            if (mod->segment[j].flags.r.load == 0)
                continue;

            foffset = mod->segment[j].file_offset;
            ssize = mod->segment[j].flags.r.length * 4096;

            /* L2 cache */
            if (mod->segment[j].v_base_addr >= ADSP_BXT_DSP_SRAM_BASE &&
                mod->segment[j].v_base_addr < ADSP_BXT_DSP_SRAM_BASE + ADSP_BXT_DSP_SRAM_SIZE) {
	    	soffset = mod->segment[j].v_base_addr - ADSP_BXT_DSP_SRAM_BASE;
 
                printf(" L2 segment %d file offset 0x%lx SRAM addr 0x%x offset 0x%lx size 0x%lx\n",
                    j, foffset, mod->segment[j].v_base_addr, soffset, ssize);

                /* copy text to SRAM */
                cpu_physical_memory_write(board->iram.base + soffset,
                    (void*)man + foffset, ssize);
                continue;
            }

            /* HP SRAM */
            if (mod->segment[j].v_base_addr >= ADSP_BXT_DSP_HP_SRAM_BASE &&
                mod->segment[j].v_base_addr < ADSP_BXT_DSP_HP_SRAM_BASE + ADSP_BXT_DSP_HP_SRAM_SIZE) {
	    	soffset = mod->segment[j].v_base_addr - ADSP_BXT_DSP_HP_SRAM_BASE;
 
                printf(" HP segment %d file offset 0x%lx SRAM addr 0x%x offset 0x%lx size 0x%lx\n",
                    j, foffset, mod->segment[j].v_base_addr, soffset, ssize);

                /* copy text to SRAM */
                cpu_physical_memory_write(board->dram0.base + soffset,
                    (void*)man + foffset, ssize);
                continue;
            }

            /* LP SRAM */
            if (mod->segment[j].v_base_addr >= ADSP_BXT_DSP_LP_SRAM_BASE &&
                mod->segment[j].v_base_addr < ADSP_BXT_DSP_LP_SRAM_BASE + ADSP_BXT_DSP_LP_SRAM_SIZE) {
	    	soffset = mod->segment[j].v_base_addr - ADSP_BXT_DSP_LP_SRAM_BASE;
 
                printf(" LP segment %d file offset 0x%lx SRAM addr 0x%x offset 0x%lx size 0x%lx\n",
                    j, foffset, mod->segment[j].v_base_addr, soffset, ssize);

                /* copy text to SRAM */
                cpu_physical_memory_write(board->lp_sram.base + soffset,
                    (void*)man + foffset, ssize);
                continue;
            }

        }
    }

    return adsp;
}

/* hardware memory map */
static const struct adsp_desc bxt_dsp_desc = {
    .ia_irq = IRQ_NUM_EXT_IA,
    .ext_timer_irq = IRQ_NUM_EXT_TIMER,
    .pmc_irq = IRQ_NUM_EXT_PMC,

    .num_ssp = 3,
    .num_dmac = 2,
    .iram = {.base = ADSP_BXT_DSP_SRAM_BASE, .size = ADSP_BXT_DSP_SRAM_SIZE},
    .dram0 = {.base = ADSP_BXT_DSP_HP_SRAM_BASE, .size = ADSP_BXT_DSP_HP_SRAM_SIZE},
    .lp_sram = {.base = ADSP_BXT_DSP_LP_SRAM_BASE, .size = ADSP_BXT_DSP_LP_SRAM_SIZE},
    .rom = {.base = ADSP_BXT_DSP_ROM_BASE, .size = ADSP_BXT_DSP_ROM_SIZE},

    .mbox_dev = {
        .name = "mbox",
        .reg_count = ARRAY_SIZE(adsp_mbox_map),
        .reg = adsp_mbox_map,
        .desc = {.base = ADSP_BXT_DSP_MAILBOX_BASE, .size = ADSP_BXT_DSP_MAILBOX_SIZE},
    },

    .shim_dev = {
        .name = "shim",
        .reg_count = ARRAY_SIZE(adsp_bxt_shim_map),
        .reg = adsp_bxt_shim_map,
        .desc = {.base = ADSP_BXT_DSP_SHIM_BASE, .size = ADSP_BXT_SHIM_SIZE},
    },

    .gp_dmac_dev[0] = {
        .name = "dmac0",
        .reg_count = ARRAY_SIZE(adsp_gp_dma_map),
        .reg = adsp_gp_dma_map,
        .desc = {.base = ADSP_BXT_DSP_LP_GP_DMA_BASE(0), .size = ADSP_BXT_DSP_LP_GP_DMA_SIZE},
    },
    .gp_dmac_dev[1] = {
        .name = "dmac1",
        .reg_count = ARRAY_SIZE(adsp_gp_dma_map),
        .reg = adsp_gp_dma_map,
        .desc = {.base = ADSP_BXT_DSP_HP_GP_DMA_BASE(0), .size = ADSP_BXT_DSP_HP_GP_DMA_SIZE},
    },
    .ssp_dev[0] = {
        .name = "ssp0",
        .reg_count = ARRAY_SIZE(adsp_ssp_map),
        .reg = adsp_ssp_map,
        .desc = {.base = ADSP_BXT_DSP_SSP_BASE(0), .size = ADSP_BXT_DSP_SSP_SIZE},
    },
    .ssp_dev[1] = {
        .name = "ssp1",
        .reg_count = ARRAY_SIZE(adsp_ssp_map),
        .reg = adsp_ssp_map,
        .desc = {.base = ADSP_BXT_DSP_SSP_BASE(1), .size = ADSP_BXT_DSP_SSP_SIZE},
    },
    .ssp_dev[2] = {
        .name = "ssp2",
        .reg_count = ARRAY_SIZE(adsp_ssp_map),
        .reg = adsp_ssp_map,
        .desc = {.base = ADSP_BXT_DSP_SSP_BASE(2), .size = ADSP_BXT_DSP_SSP_SIZE},
    },
    .io_dev = {
        .name = "io",
        .reg_count = 0,
        .reg = NULL,
        .desc = {.base = ADSP_BXT_DSP_SHIM_BASE, .size = 0x10000},
    },
};

static void bxt_adsp_init(MachineState *machine)
{
    struct adsp_dev *adsp;

    adsp = adsp_init(&bxt_dsp_desc, machine, "bxt");

    adsp->ext_timer = timer_new_ns(QEMU_CLOCK_VIRTUAL, &bxt_ext_timer_cb, adsp);
    adsp->ext_clk_kHz = 2500;
}

static void xtensa_bxt_machine_init(MachineClass *mc)
{
    mc->desc = "Broxton HiFi3";
    mc->is_default = true;
    mc->init = bxt_adsp_init;
    mc->max_cpus = 2;
    mc->default_cpu_type = XTENSA_DEFAULT_CPU_TYPE;
}

DEFINE_MACHINE("adsp_bxt", xtensa_bxt_machine_init)
