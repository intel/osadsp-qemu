/* Core IA host PCI support for Baytrail audio DSP.
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
#include "hw/adsp/byt.h"
#include "hw/adsp/log.h"

struct byt_pci_data {
    int build_byt;
    int build_cht;
    int build_rt5640;
    int build_rt5641;
};

static struct byt_pci_data pdata = {0, 0, 1, 0};

static uint64_t byt_pci_read(void *opaque, hwaddr addr,
        unsigned size)
{
    struct adsp_host *adsp = opaque;
    printf("%s %d\n", __func__, __LINE__);
    log_read(adsp->log, &adsp->desc->pci_dev,
        addr, size, adsp->pci_io[addr >> 2]);

    return adsp->pci_io[addr >> 2];
}

static void byt_pci_write(void *opaque, hwaddr addr,
        uint64_t val, unsigned size)
{
    struct adsp_host *adsp = opaque;
    printf("%s %d\n", __func__, __LINE__);
    log_write(adsp->log, &adsp->desc->pci_dev,
            addr, val, size, adsp->pci_io[addr >> 2]);

    adsp->pci_io[addr >> 2] = val;

    /* TODO: pass on certain writes to DSP as MQ messages e.g. PM */
}

static const MemoryRegionOps byt_pci_ops = {
    .read = byt_pci_read,
    .write = byt_pci_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void build_acpi_rt5640_device(Aml *table)
{
    Aml *method;
    Aml *scope = aml_scope("_SB");
    Aml *dev = aml_device("RTEK");
    Aml *zero = aml_int(0);
    Aml *one = aml_int(1);

    /* device info */
    aml_append(dev, aml_name_decl("_ADR", zero));
    aml_append(dev, aml_name_decl("_HID", aml_string("10EC5640")));
    aml_append(dev, aml_name_decl("_CID", aml_string("10EC5640")));
    aml_append(dev, aml_name_decl("_DDN",
        aml_string("RTEK Codec RT5640")));
    aml_append(dev, aml_name_decl("_UID", one));

    /* _STA method */
    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_return(aml_int(0x0F)));
    aml_append(dev, method);

    aml_append(scope, dev);
    aml_append(table, scope);
}

static void build_acpi_rt5641_device(Aml *table)
{
    Aml *method;
    Aml *scope = aml_scope("_SB");
    Aml *dev = aml_device("RTEK");
    Aml *zero = aml_int(0);
    Aml *one = aml_int(1);

    /* device info */
    aml_append(dev, aml_name_decl("_ADR", zero));
    aml_append(dev, aml_name_decl("_HID", aml_string("10EC5651")));
    aml_append(dev, aml_name_decl("_CID", aml_string("10EC5651")));
    aml_append(dev, aml_name_decl("_DDN",
        aml_string("RTEK Codec RT5641")));
    aml_append(dev, aml_name_decl("_UID", one));

    /* _STA method */
    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_return(aml_int(0x0F)));
    aml_append(dev, method);

    aml_append(scope, dev);
    aml_append(table, scope);
}

static void build_acpi_byt_device(Aml *table)
{
    Aml *crs;
    Aml *method;
    Aml *scope = aml_scope("_SB");
    Aml *dev = aml_device("LPEA");
    Aml *zero = aml_int(0);
    Aml *one = aml_int(1);

    /* device info */
    aml_append(dev, aml_name_decl("_ADR", zero));
    aml_append(dev, aml_name_decl("_HID", aml_string("80860F28")));
    aml_append(dev, aml_name_decl("_CID", aml_string("80860F28")));
    aml_append(dev, aml_name_decl("_SUB", aml_string("80867270")));
    aml_append(dev, aml_name_decl("_DDN",
        aml_string("Intel(R) Low Power Audio Controller - 80860F28")));
    aml_append(dev, aml_name_decl("_UID", one));

    /* _STA method */
    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_return(aml_int(0x0F)));
    aml_append(dev, method);

    /* resources */
    crs = aml_resource_template();
    aml_append(crs,
    aml_memory32_fixed(ADSP_BYT_MMIO_BASE, ADSP_MMIO_SIZE, AML_READ_ONLY));
    aml_append(crs,
    aml_memory32_fixed(ADSP_BYT_PCI_BASE, ADSP_PCI_SIZE, AML_READ_ONLY));
    aml_append(crs, aml_memory32_fixed(0x55AA55AA, 0x00100000, AML_READ_ONLY));
    aml_append(crs, aml_irq_no_flags(0x8));
    aml_append(crs, aml_irq_no_flags(0x9));
    aml_append(crs, aml_irq_no_flags(0xa));
    aml_append(crs, aml_irq_no_flags(0xb));
    aml_append(crs, aml_irq_no_flags(0xc));
    aml_append(crs, aml_irq_no_flags(0xa)); /* used by upstream driver */
    aml_append(dev, aml_name_decl("_CRS", crs));

    aml_append(scope, dev);
    aml_append(table, scope);
}

/* TODO: check with a CHT BIOS */
static void build_acpi_cht_device(Aml *table)
{
    Aml *crs;
    Aml *method;
    Aml *scope = aml_scope("_SB");
    Aml *dev = aml_device("LPEC");
    Aml *zero = aml_int(0);
    Aml *one = aml_int(1);

    /* device info */
    aml_append(dev, aml_name_decl("_ADR", zero));
    aml_append(dev, aml_name_decl("_HID", aml_string("808622A8")));
    aml_append(dev, aml_name_decl("_CID", aml_string("808622A8")));
    aml_append(dev, aml_name_decl("_SUB", aml_string("80867270")));
    aml_append(dev, aml_name_decl("_DDN",
        aml_string("Intel(R) Low Power Audio Controller - 808622A8")));
    aml_append(dev, aml_name_decl("_UID", one));

    /* _STA method */
    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_return(aml_int(0x0F)));
    aml_append(dev, method);

    /* resources */
    crs = aml_resource_template();
    aml_append(crs,
    aml_memory32_fixed(ADSP_CHT_MMIO_BASE, ADSP_MMIO_SIZE, AML_READ_ONLY));
    aml_append(crs,
    aml_memory32_fixed(ADSP_CHT_PCI_BASE, ADSP_PCI_SIZE, AML_READ_ONLY));
    aml_append(crs, aml_memory32_fixed(0x55AA55AA, 0x00100000, AML_READ_ONLY));
    aml_append(crs, aml_irq_no_flags(0x8));
    aml_append(crs, aml_irq_no_flags(0x9));
    aml_append(crs, aml_irq_no_flags(0xa));
    aml_append(crs, aml_irq_no_flags(0xb));
    aml_append(crs, aml_irq_no_flags(0xc));
    aml_append(crs, aml_irq_no_flags(0xa));
    aml_append(dev, aml_name_decl("_CRS", crs));

    aml_append(scope, dev);
    aml_append(table, scope);
}

void build_acpi_byt_adsp_devices(Aml *table)
{
    if (pdata.build_byt)
        build_acpi_byt_device(table);
    if (pdata.build_cht)
        build_acpi_cht_device(table);
    if (pdata.build_rt5640)
        build_acpi_rt5640_device(table);
    if (pdata.build_rt5641)
        build_acpi_rt5641_device(table);
}

void adsp_byt_pci_exit(PCIDevice *pci_dev)
{
}

static void adsp_byt_write_config(PCIDevice *pci_dev, uint32_t address,
                                uint32_t val, int len)
{
    pci_default_write_config(pci_dev, address, val, len);
}

void adsp_byt_pci_realize(PCIDevice *pci_dev, Error **errp)
{
    struct adsp_host *adsp = adsp_get_pdata(pci_dev,
        ADSP_HOST_BYT_NAME);
    uint8_t *pci_conf;

    pci_conf = pci_dev->config;
    pci_dev->config_write = adsp_byt_write_config;
    //pci_conf[PCI_INTERRUPT_PIN] = 3; /* interrupt pin A 0=11, A1=? B2=10 C3=? D4= ?*/

    pci_set_byte(&pci_conf[PCI_INTERRUPT_PIN], 2); /* interrupt pin A */
    pci_set_byte(&pci_conf[PCI_MIN_GNT], 0);
    pci_set_byte(&pci_conf[PCI_MAX_LAT], 0);

    adsp->irq = pci_allocate_irq(&adsp->dev);
    pdata.build_byt = 1;

    adsp_byt_host_init(adsp, "byt");
}

void adsp_cht_pci_realize(PCIDevice *pci_dev, Error **errp)
{
    struct adsp_host *adsp = adsp_get_pdata(pci_dev,
        ADSP_HOST_CHT_NAME);
    uint8_t *pci_conf;

    pci_conf = adsp->dev.config;
    pci_conf[PCI_INTERRUPT_PIN] = 1; /* interrupt pin A */

    adsp->irq = pci_allocate_irq(&adsp->dev);
    pdata.build_cht = 1;

    adsp_byt_host_init(adsp, "cht");
}

void adsp_byt_init_pci(struct adsp_host *adsp)
{
    MemoryRegion *pci;
    const struct adsp_desc *board = adsp->desc;

    /* PCI reg space  - shared via MSQ */
    pci = g_malloc(sizeof(*pci));
    adsp->pci_io = g_malloc(board->pci.size);
    memory_region_init_io(pci, OBJECT(adsp), &byt_pci_ops, adsp,
        "pci.io", board->pci.size);
    //memory_region_add_subregion(adsp->system_memory,
     //   board->pci.base, pci);

    //qemu_register_reset(pci_reset, adsp);
    pci_register_bar(&adsp->dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, pci);

}
