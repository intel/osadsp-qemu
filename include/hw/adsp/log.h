/*
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

#ifndef __ADSP_LOG_H__
#define __ADSP_LOG_H__

#include <sys/time.h>
#include <stdio.h>
#include <glib.h>
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu-common.h"
#include "sysemu/sysemu.h"
#include "hw/boards.h"
#include "hw/loader.h"

#include "qemu/io-bridge.h"
#include "hw/adsp/hw.h"
#include "hw/adsp/shim.h"

#define TRACE_CLASS_IRQ		(1 << 24)
#define TRACE_CLASS_IPC		(2 << 24)
#define TRACE_CLASS_PIPE	(3 << 24)
#define TRACE_CLASS_HOST	(4 << 24)
#define TRACE_CLASS_DAI		(5 << 24)
#define TRACE_CLASS_DMA		(6 << 24)
#define TRACE_CLASS_SSP		(7 << 24)
#define TRACE_CLASS_COMP	(8 << 24)
#define TRACE_CLASS_WFI		(9 << 24)

/* enable generic logging */
#define LOG_ENABLED		1

/* mailbox debug options - LOG_MBOX must be 1 to enable other options */
#define LOG_MBOX		1
#define LOG_MBOX_OUTBOX	1
#define LOG_MBOX_INBOX	1
#define LOG_MBOX_DEBUG	0
#define LOG_MBOX_EXCEPTION	1
#define LOG_MBOX_TRACE	1
#define LOG_MBOX_STREAM	0

/* shim debug options - LOG_SHIM must be 1 to enable other options */
#define LOG_SHIM		1
#define LOG_SHIM_CSR		1
#define LOG_SHIM_PISR		1
#define LOG_SHIM_PIMR		1
#define LOG_SHIM_ISRX		1
#define LOG_SHIM_ISRD		1
#define LOG_SHIM_IMRX		1
#define LOG_SHIM_IMRD		1
#define LOG_SHIM_IPCXL	1
#define LOG_SHIM_IPCXH	1
#define LOG_SHIM_IPCDL	1
#define LOG_SHIM_IPCDH	1
#define LOG_SHIM_ISRSC	1
#define LOG_SHIM_ISRLPESC	1
#define LOG_SHIM_IMRSCL	1
#define LOG_SHIM_IMRSCH	1
#define LOG_SHIM_IMRLPESC	1
#define LOG_SHIM_IPCSCL	1
#define LOG_SHIM_IPCSCH	1
#define LOG_SHIM_IPCLPESCL	1
#define LOG_SHIM_IPCLPESCH	1
#define LOG_SHIM_CLKCTL	1
#define LOG_SHIM_FR_LAT_REQ	1
#define LOG_SHIM_MISC		1
#define LOG_SHIM_EXTTL	1
#define LOG_SHIM_EXTTH	1
#define LOG_SHIM_EXTST	0

/* IRQ debug options */
#define LOG_IRQ_BUSY		1
#define LOG_IRQ_DONE		1
#define LOG_IRQ_ACTIVE	1

/* CPU status debug options */
#define LOG_CPU_RESET		1

/* CPU status debug options */
#define LOG_DMA		1
#define LOG_DMA_M2M		1
#define LOG_DMA_P2M		1
#define LOG_DMA_M2P		1
#define LOG_DMA_IRQ		1

/* MSG Q logging */
#define LOG_MSGQ		1

/* SSP Logging */
#define LOG_SSP 1

struct adsp_log {
	GMutex mutex;
	FILE *file;
	time_t timestamp;	/* last trace timestamp */
	time_t tv_sec_start;	/* start of tv_secs */
};

#define BYT_SHIM_REGS   25
extern const struct adsp_reg_desc adsp_byt_shim_map[BYT_SHIM_REGS];
#define HSW_SHIM_REGS   9
extern const struct adsp_reg_desc adsp_hsw_shim_map[HSW_SHIM_REGS];
#define BXT_SHIM_REGS   25
extern const struct adsp_reg_desc adsp_bxt_shim_map[BXT_SHIM_REGS];


#if LOG_SHIM

static inline void log_print(struct adsp_log *log, const char *fmt, ...)
{
	va_list va;
	struct timeval tv;

	if (log == NULL)
		return;

	va_start(va, fmt);
	gettimeofday(&tv, NULL);

	g_mutex_lock(&log->mutex);

	fprintf(log->file, "%6.6ld:%6.6ld: ", tv.tv_sec - log->tv_sec_start,
		tv.tv_usec);
	vfprintf(log->file, fmt, va);
	g_mutex_unlock(&log->mutex);

	va_end(va);
}

static inline char log_get_char(uint32_t val, int idx)
{
	char c = (val >> (idx * 8)) & 0xff;
	if (c < '0' || c > 'z')
		return '.';
	else
		return c;
}

static inline void log_read(struct adsp_log *log,
	const struct adsp_reg_space *space,
	hwaddr addr, unsigned size, uint64_t value)
{
	const struct adsp_reg_desc *reg = space->reg;
	int i;

	/* TODO: have a faster direct array mapping to register to addr */
	for (i = 0; i < space->reg_count; i++) {

		if (addr != reg[i].offset)
			continue;

		if (!reg[i].enable)
			continue;

		log_print(log, "%s.io: read at %x val 0x%8.8lx\n", space->name,
			(unsigned int)addr, value);

		return;

	}
}

static inline void log_write(struct adsp_log *log,
	const struct adsp_reg_space *space,
	hwaddr addr, uint64_t val, unsigned size, uint64_t old_val)
{
	const struct adsp_reg_desc *reg = space->reg;
	int i;

	/* TODO: have a faster direct array mapping to register to addr */
	for (i = 0; i < space->reg_count; i++) {

		if (addr != reg[i].offset)
			continue;

		if (!reg[i].enable)
			continue;

		log_print(log, "%s.io: write 0x%x = \t0x%8.8lx (old 0x%lx) \t(%8.8ld) \t|%c%c%c%c|\n",
			space->name, (unsigned int)addr, val,
			old_val, val,
			log_get_char(val, 3), log_get_char(val, 2),
			log_get_char(val, 1), log_get_char(val, 0));
		return;
	}
}
#else
#define log_read(log, space, addr, size, value)
#define log_write(log, space, addr, val, size, old_val)
#endif

#if LOG_MBOX
static inline void log_area_read(struct adsp_log *log,
	const struct adsp_reg_space *space,
	hwaddr addr, unsigned size, uint64_t value)
{
	const struct adsp_reg_desc *reg = space->reg;
	int i;

	/* TODO: have a faster direct array mapping to register to addr */
	for (i = 0; i < space->reg_count; i++) {

		if (!reg[i].enable)
			continue;

		if (addr >= reg[i].offset &&
			addr < reg[i].offset + reg[i].size) {

			log_print(log, "%s.io: read at 0x%x val 0x%8.8x\n",
				space->name, (unsigned int)addr, value);
			return;
		}
	}
}

static inline void log_area_write(struct adsp_log *log,
	const struct adsp_reg_space *space,
	hwaddr addr, uint64_t val, unsigned size, uint64_t old_val)
{
	const struct adsp_reg_desc *reg = space->reg;
	uint32_t class;
	const char *trace;
	int i;

	/* ignore writes of 0 atm - used in mbox clear and init */
	if (val == 0 && addr >= 0xc00)
	    return;

	/* TODO: have a faster direct array mapping to register to addr */
	for (i = 0; i < space->reg_count; i++) {

		//if (!reg[i].enable)
		//	continue;

		/* TODO: get trace offset - do trace differently */
		if (addr >= 0xc00) {

			/* timestamp or value ? */
			if ((addr % 8) == 0) {
				log_print(log, "trace.io: timestamp 0x%8.8x delta 0x%8.8x ",
					(uint32_t)val, (uint32_t)val - log->timestamp);
				log->timestamp = val;
				return;
			}

			class = val & 0xff000000;
			if (class == TRACE_CLASS_IRQ)
				trace = "irq";
			else if (class == TRACE_CLASS_IPC)
				trace = "ipc";
			else if (class == TRACE_CLASS_PIPE)
				trace = "pipe";
			else if (class == TRACE_CLASS_HOST)
				trace = "host";
			else if (class == TRACE_CLASS_DAI)
				trace = "dai";
			else if (class == TRACE_CLASS_DMA)
				trace = "dma";
			else if (class == TRACE_CLASS_SSP)
				trace = "ssp";
			else if (class == TRACE_CLASS_COMP)
				trace = "comp";
			else if (class == TRACE_CLASS_WFI)
				trace = "wfi";
			else {
				log_print(log, "value 0x%8.8x\n", (uint32_t)val);
				return;
			}

			log_print(log, "%s %c%c%c\n", trace,
				(char)(val >> 16), (char)(val >> 8), (char)val);
			return;
		}

		if (addr >= reg[i].offset &&
			addr < reg[i].offset + reg[i].size) {

			log_print(log, "%s.io: write 0x%x = \t0x%8.8lx \t(%8.8ld) \t|%c%c%c%c|\n",
				space->name, (unsigned int)addr, val, val,
				log_get_char(val, 3), log_get_char(val, 2),
				log_get_char(val, 1), log_get_char(val, 0));
			return;
		}
	}
}
#else
#define log_area_read(log, space, addr, size, value)
#define log_area_write(log, space, addr, val, size, old_val)
#endif

#if LOG_ENABLED
#define log_text(log, enable, fmt, ...)	\
	if (enable) {log_print(log, fmt, ## __VA_ARGS__);}
#else
#define log_text(log, enable fmt, ...)
#endif

struct adsp_log *log_init(const char *log_name);

#endif
