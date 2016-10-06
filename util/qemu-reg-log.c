/* Core register IO tracing
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

#include <sys/time.h>
#include <stdio.h>
#include <glib.h>
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "sysemu/sysemu.h"

#include "qemu/io-bridge.h"
#include "hw/adsp/shim.h"
#include "hw/adsp/log.h"

const struct adsp_reg_desc adsp_byt_shim_map[] = {
    {.name = "csr", .enable = LOG_SHIM_CSR, .offset = 0x0},
    {.name = "pisr", .enable = LOG_SHIM_PISR, .offset = 0x8},
    {.name = "pimr", .enable = LOG_SHIM_PIMR, .offset = 0x10},
    {.name = "isrx", .enable = LOG_SHIM_ISRX, .offset = 0x18},
    {.name = "isrd", .enable = LOG_SHIM_ISRD, .offset = 0x20},
    {.name = "imrx", .enable = LOG_SHIM_IMRX, .offset = 0x28},
    {.name = "imrd", .enable = LOG_SHIM_IMRD, .offset = 0x30},
    {.name = "ipcxl", .enable = LOG_SHIM_IPCXL, .offset = 0x38},
    {.name = "ipcxh", .enable = LOG_SHIM_IPCXH, .offset = 0x3c},
    {.name = "ipcdl", .enable = LOG_SHIM_IPCDL, .offset = 0x40},
    {.name = "ipclh", .enable = LOG_SHIM_IPCDH, .offset = 0x44},
    {.name = "isrsc", .enable = LOG_SHIM_ISRSC, .offset = 0x48},
    {.name = "isrlpesc", .enable = LOG_SHIM_ISRLPESC, .offset = 0x50},
    {.name = "imrscl", .enable = LOG_SHIM_IMRSCL, .offset = 0x58},
    {.name = "imrsch", .enable = LOG_SHIM_IMRSCH, .offset = 0x5c},
    {.name = "imrlpesc", .enable = LOG_SHIM_IMRLPESC, .offset = 0x60},
    {.name = "ipcscl", .enable = LOG_SHIM_IPCSCL, .offset = 0x68},
    {.name = "ipcsch", .enable = LOG_SHIM_IPCSCH, .offset = 0x6c},
    {.name = "ipclpescl", .enable = LOG_SHIM_IPCLPESCL, .offset = 0x70},
    {.name = "ipclpesch", .enable = LOG_SHIM_IPCLPESCH, .offset = 0x74},
    {.name = "clkctl", .enable = LOG_SHIM_CLKCTL, .offset = 0x78},
    {.name = "frlatreq", .enable = LOG_SHIM_FR_LAT_REQ, .offset = 0x80},
    {.name = "exttl", .enable = LOG_SHIM_EXTTL, .offset = 0xc0},
    {.name = "extth", .enable = LOG_SHIM_EXTTH, .offset = 0xc4},
    {.name = "extst", .enable = LOG_SHIM_EXTST, .offset = 0xc8},
};

const struct adsp_reg_desc adsp_hsw_shim_map[] = {
    {.name = "csr", .enable = LOG_SHIM_CSR, .offset = 0x0},
    {.name = "isrx", .enable = LOG_SHIM_ISRX, .offset = 0x18},
    {.name = "isrd", .enable = LOG_SHIM_ISRD, .offset = 0x20},
    {.name = "imrx", .enable = LOG_SHIM_IMRX, .offset = 0x28},
    {.name = "imrd", .enable = LOG_SHIM_IMRD, .offset = 0x30},
    {.name = "ipcxl", .enable = LOG_SHIM_IPCXL, .offset = 0x38},
    {.name = "ipcxh", .enable = LOG_SHIM_IPCXH, .offset = 0x3c},
    {.name = "ipcdl", .enable = LOG_SHIM_IPCDL, .offset = 0x40},
    {.name = "ipclh", .enable = LOG_SHIM_IPCDH, .offset = 0x44},
};

const struct adsp_reg_desc adsp_bxt_shim_map[] = {
    {.name = "csr", .enable = LOG_SHIM_CSR, .offset = 0x0},
    {.name = "pisr", .enable = LOG_SHIM_PISR, .offset = 0x8},
    {.name = "pimr", .enable = LOG_SHIM_PIMR, .offset = 0x10},
    {.name = "isrx", .enable = LOG_SHIM_ISRX, .offset = 0x18},
    {.name = "isrd", .enable = LOG_SHIM_ISRD, .offset = 0x20},
    {.name = "imrx", .enable = LOG_SHIM_IMRX, .offset = 0x28},
    {.name = "imrd", .enable = LOG_SHIM_IMRD, .offset = 0x30},
    {.name = "ipcxl", .enable = LOG_SHIM_IPCXL, .offset = 0x38},
    {.name = "ipcxh", .enable = LOG_SHIM_IPCXH, .offset = 0x3c},
    {.name = "ipcdl", .enable = LOG_SHIM_IPCDL, .offset = 0x40},
    {.name = "ipclh", .enable = LOG_SHIM_IPCDH, .offset = 0x44},
    {.name = "isrsc", .enable = LOG_SHIM_ISRSC, .offset = 0x48},
    {.name = "isrlpesc", .enable = LOG_SHIM_ISRLPESC, .offset = 0x50},
    {.name = "imrscl", .enable = LOG_SHIM_IMRSCL, .offset = 0x58},
    {.name = "imrsch", .enable = LOG_SHIM_IMRSCH, .offset = 0x5c},
    {.name = "imrlpesc", .enable = LOG_SHIM_IMRLPESC, .offset = 0x60},
    {.name = "ipcscl", .enable = LOG_SHIM_IPCSCL, .offset = 0x68},
    {.name = "ipcsch", .enable = LOG_SHIM_IPCSCH, .offset = 0x6c},
    {.name = "ipclpescl", .enable = LOG_SHIM_IPCLPESCL, .offset = 0x70},
    {.name = "ipclpesch", .enable = LOG_SHIM_IPCLPESCH, .offset = 0x74},
    {.name = "clkctl", .enable = LOG_SHIM_CLKCTL, .offset = 0x78},
    {.name = "frlatreq", .enable = LOG_SHIM_FR_LAT_REQ, .offset = 0x80},
    {.name = "exttl", .enable = LOG_SHIM_EXTTL, .offset = 0xc0},
    {.name = "extth", .enable = LOG_SHIM_EXTTH, .offset = 0xc4},
    {.name = "extst", .enable = LOG_SHIM_EXTST, .offset = 0xc8},
};

struct adsp_log *log_init(const char *log_name)
{
    struct adsp_log *log;
    struct timeval tv;

    log = g_malloc(sizeof(*log));
    g_mutex_init(&log->mutex);

    if (log_name == NULL)
        log->file = stdout;
    else
        log->file = fopen(log_name, "w");

    if (log->file == NULL) {
        fprintf(stderr, "error: can't open %s err %d\n", log_name, -errno);
        exit(0);
    }

    gettimeofday(&tv, NULL);
    log->tv_sec_start = tv.tv_sec;

    return log;
}
