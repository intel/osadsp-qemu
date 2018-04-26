/* Core IA host PCI support for Haswell audio DSP.
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
#include "hw/acpi/aml-build.h"
#include "hw/audio/adsp-host.h"
#include "hw/adsp/shim.h"
#include "hw/adsp/hsw.h"
#include "hw/adsp/log.h"

struct hsw_pci_data {
    int build_hsw;
    int build_bdw;
    int build_rt286;
};

static struct hsw_pci_data pdata = {0, 0, 0};

static uint64_t hsw_pci_read(void *opaque, hwaddr addr,
        unsigned size)
{
    struct adsp_host *adsp = opaque;

    log_read(adsp->log, &adsp->desc->pci_dev,
        addr, size, adsp->pci_io[addr >> 2]);

    return adsp->pci_io[addr >> 2];
}

static void hsw_pci_write(void *opaque, hwaddr addr,
        uint64_t val, unsigned size)
{
    struct adsp_host *adsp = opaque;

    log_write(adsp->log, &adsp->desc->pci_dev,
            addr, val, size, adsp->pci_io[addr >> 2]);

    adsp->pci_io[addr >> 2] = val;

    /* TODO: pass on certain writes to DSP as MQ messages e.g. PM */
}

static const MemoryRegionOps hsw_pci_ops = {
    .read = hsw_pci_read,
    .write = hsw_pci_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void build_acpi_rt286_device(Aml *table)
{
    Aml *method;
    Aml *scope = aml_scope("_SB");
    Aml *dev = aml_device("RTE1");
    Aml *zero = aml_int(0);
    Aml *one = aml_int(1);

    /* device info */
    aml_append(dev, aml_name_decl("_ADR", zero));
    aml_append(dev, aml_name_decl("_HID", aml_string("INT33CA")));
    aml_append(dev, aml_name_decl("_CID", aml_string("INT33CA")));
    aml_append(dev, aml_name_decl("_DDN",
        aml_string("RTEK Codec Controller")));
    aml_append(dev, aml_name_decl("_UID", one));

    /* _STA method */
    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_return(aml_int(0x0F)));
    aml_append(dev, method);

    aml_append(scope, dev);
    aml_append(table, scope);
}

static void build_acpi_hsw_device(Aml *table)
{
    Aml *crs;
    Aml *method;
    Aml *scope = aml_scope("_SB");
    Aml *dev = aml_device("LPEB");
    Aml *zero = aml_int(0);
    Aml *one = aml_int(1);

    /* device info */
    aml_append(dev, aml_name_decl("_ADR", zero));
    aml_append(dev, aml_name_decl("_HID", aml_string("INT33C8")));
    aml_append(dev, aml_name_decl("_CID", aml_string("INT33C8")));
    aml_append(dev, aml_name_decl("_SUB", aml_string("80867270")));
    aml_append(dev, aml_name_decl("_DDN",
        aml_string("Intel(R) Low Power Audio Controller - INT33C8")));
    aml_append(dev, aml_name_decl("_UID", one));

    /* _STA method */
    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_return(aml_int(0x0F)));
    aml_append(dev, method);

    /* resources */
    crs = aml_resource_template();
    aml_append(crs,
    aml_memory32_fixed(ADSP_HSW_MMIO_BASE, ADSP_MMIO_SIZE, AML_READ_ONLY));
    aml_append(crs,
    aml_memory32_fixed(ADSP_HSW_PCI_BASE, ADSP_PCI_SIZE, AML_READ_ONLY));
    aml_append(crs, aml_memory32_fixed(0x55AA55AA, 0x00100000, AML_READ_ONLY));
    aml_append(crs, aml_irq_no_flags(0xb));
    aml_append(dev, aml_name_decl("_CRS", crs));

    aml_append(scope, dev);
    aml_append(table, scope);
}

static void build_acpi_bdw_device(Aml *table)
{
    Aml *crs;
    Aml *method;
    Aml *scope = aml_scope("_SB");
    Aml *dev = aml_device("LPED");
    Aml *zero = aml_int(0);
    Aml *one = aml_int(1);
    printf("%s %d\n", __func__, __LINE__);
    /* device info */
    aml_append(dev, aml_name_decl("_ADR", zero));
    aml_append(dev, aml_name_decl("_HID", aml_string("INT3438")));
    aml_append(dev, aml_name_decl("_CID", aml_string("INT3438")));
    aml_append(dev, aml_name_decl("_SUB", aml_string("80867270")));
    aml_append(dev, aml_name_decl("_DDN",
        aml_string("Intel(R) Low Power Audio Controller - INT3438")));
    aml_append(dev, aml_name_decl("_UID", one));

    /* _STA method */
    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_return(aml_int(0x0F)));
    aml_append(dev, method);

    /* resources */
    crs = aml_resource_template();
    aml_append(crs,
    aml_memory32_fixed(ADSP_BDW_MMIO_BASE, ADSP_MMIO_SIZE, AML_READ_ONLY));
    aml_append(crs,
    aml_memory32_fixed(ADSP_BDW_PCI_BASE, ADSP_PCI_SIZE, AML_READ_ONLY));
    aml_append(crs, aml_memory32_fixed(0x55AA55AA, 0x00100000, AML_READ_ONLY));
    aml_append(crs, aml_irq_no_flags(0xb));
    aml_append(dev, aml_name_decl("_CRS", crs));

    aml_append(scope, dev);
    aml_append(table, scope);
}

void build_acpi_hsw_adsp_devices(Aml *table)
{
    if (pdata.build_hsw)
        build_acpi_hsw_device(table);
    if (pdata.build_bdw)
        build_acpi_bdw_device(table);
    if (pdata.build_rt286)
        build_acpi_rt286_device(table);
}

void adsp_hsw_pci_exit(PCIDevice *pci_dev)
{
}

static void adsp_hsw_write_config(PCIDevice *pci_dev, uint32_t address,
                                uint32_t val, int len)
{
    pci_default_write_config(pci_dev, address, val, len);
}

void adsp_hsw_pci_realize(PCIDevice *pci_dev, Error **errp)
{
    struct adsp_host *adsp = adsp_get_pdata(pci_dev,
        ADSP_HOST_HSW_NAME);
    uint8_t *pci_conf;

    pci_conf = pci_dev->config;
    pci_dev->config_write = adsp_hsw_write_config;

    pci_set_byte(&pci_conf[PCI_INTERRUPT_PIN], 1); /* interrupt pin A */
    pci_set_byte(&pci_conf[PCI_MIN_GNT], 0);
    pci_set_byte(&pci_conf[PCI_MAX_LAT], 0);

    adsp->irq = pci_allocate_irq(&adsp->dev);
    pdata.build_hsw = 1;

    adsp_hsw_host_init(adsp, "hsw");
}

void adsp_bdw_pci_realize(PCIDevice *pci_dev, Error **errp)
{
    struct adsp_host *adsp = adsp_get_pdata(pci_dev,
        ADSP_HOST_BDW_NAME);
    uint8_t *pci_conf;

    pci_conf = pci_dev->config;
    pci_dev->config_write = adsp_hsw_write_config;

    pci_set_byte(&pci_conf[PCI_INTERRUPT_PIN], 1); /* interrupt pin A */
    pci_set_byte(&pci_conf[PCI_MIN_GNT], 0);
    pci_set_byte(&pci_conf[PCI_MAX_LAT], 0);

    adsp->irq = pci_allocate_irq(&adsp->dev);
    pdata.build_bdw = 1;

    adsp_bdw_host_init(adsp, "bdw");
}

void adsp_hsw_init_pci(struct adsp_host *adsp)
{
    MemoryRegion *pci;
    const struct adsp_desc *board = adsp->desc;

    /* PCI reg space  - shared via MSQ */
    pci = g_malloc(sizeof(*pci));
    adsp->pci_io = g_malloc(board->pci.size);
    memory_region_init_io(pci, NULL, &hsw_pci_ops, adsp,
        "pci.io", board->pci.size);
    memory_region_add_subregion(adsp->system_memory,
        board->pci.base, pci);
    //qemu_register_reset(pci_reset, adsp);
}
