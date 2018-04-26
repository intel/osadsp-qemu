/* Core IA host PCI support for Broxton audio DSP.
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
#include "hw/adsp/bxt.h"
#include "hw/adsp/log.h"

static uint64_t bxt_pci_read(void *opaque, hwaddr addr,
        unsigned size)
{
    struct adsp_host *adsp = opaque;

    log_area_read(adsp->log, &adsp->desc->pci_dev,
        addr, size, adsp->pci_io[addr >> 2]);

    return adsp->pci_io[addr >> 2];
}

static void bxt_pci_write(void *opaque, hwaddr addr,
        uint64_t val, unsigned size)
{
    struct adsp_host *adsp = opaque;

    log_area_write(adsp->log, &adsp->desc->pci_dev,
            addr, val, size, adsp->pci_io[addr >> 2]);

    adsp->pci_io[addr >> 2] = val;

    /* TODO: pass on certain writes to DSP as MQ messages e.g. PM */
}

static const MemoryRegionOps bxt_pci_ops = {
    .read = bxt_pci_read,
    .write = bxt_pci_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

/* TODO: need to check this against real BIOS - HDA device */
void build_acpi_bxt_adsp_devices(Aml *table)
{
    Aml *crs;
    Aml *method;
    Aml *scope = aml_scope("_SB");
    Aml *dev = aml_device("LPEE");
    Aml *zero = aml_int(0);
    Aml *one = aml_int(1);
return;
    /* device info */
    aml_append(dev, aml_name_decl("_ADR", zero));
    aml_append(dev, aml_name_decl("_HID", aml_string("INT5a98")));
    aml_append(dev, aml_name_decl("_CID", aml_string("INT5a98")));
    aml_append(dev, aml_name_decl("_SUB", aml_string("80867270")));
    aml_append(dev, aml_name_decl("_DDN",
        aml_string("Intel(R) Low Power Audio Controller - INT5a98")));
    aml_append(dev, aml_name_decl("_UID", one));

    /* _STA method */
    method = aml_method("_STA", 0, AML_NOTSERIALIZED);
    aml_append(method, aml_return(aml_int(0x0F)));
    aml_append(dev, method);

    /* resources */
    crs = aml_resource_template();
    aml_append(crs,
    aml_memory32_fixed(ADSP_BXT_MMIO_BASE, ADSP_MMIO_SIZE, AML_READ_ONLY));
    aml_append(crs,
    aml_memory32_fixed(ADSP_BXT_PCI_BASE, ADSP_PCI_SIZE, AML_READ_ONLY));
    aml_append(crs, aml_memory32_fixed(0x55AA55AA, 0x00100000, AML_READ_ONLY));
    aml_append(crs, aml_irq_no_flags(0xb));
    aml_append(dev, aml_name_decl("_CRS", crs));

    aml_append(scope, dev);
    aml_append(table, scope);
}

void adsp_bxt_pci_exit(PCIDevice *pci_dev)
{
}

void adsp_bxt_pci_realize(PCIDevice *pci_dev, Error **errp)
{
    struct adsp_host *adsp = adsp_get_pdata(pci_dev,
        ADSP_HOST_BXT_NAME);
    uint8_t *pci_conf;

    pci_conf = adsp->dev.config;
    pci_conf[PCI_INTERRUPT_PIN] = 1; /* interrupt pin A */

    adsp->irq = pci_allocate_irq(&adsp->dev);

    adsp_bxt_host_init(adsp, "bxt");
}

void adsp_bxt_init_pci(struct adsp_host *adsp)
{
    MemoryRegion *pci;
    const struct adsp_desc *board = adsp->desc;

    /* PCI reg space  - shared via MSQ */
    pci = g_malloc(sizeof(*pci));
    adsp->pci_io = g_malloc(board->pci.size);
    memory_region_init_io(pci, NULL, &bxt_pci_ops, adsp,
        "pci.io", board->pci.size);
    memory_region_add_subregion(adsp->system_memory,
        board->pci.base, pci);
    //qemu_register_reset(pci_reset, adsp);
}
