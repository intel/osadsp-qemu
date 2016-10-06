/* Core logging support for audio DSP.
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
#include "hw/boards.h"
#include "hw/loader.h"
#include "elf.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "exec/ram_addr.h"

#include "qemu/io-bridge.h"
#include "adsp.h"
#include "hw/adsp/shim.h"
#include "hw/adsp/log.h"

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
