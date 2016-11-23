/*
 * Copyright (c) 2015, Intel Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Open Source and Linux Lab nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/exec-all.h"
#include "exec/gdbstub.h"
#include "qemu/host-utils.h"

#include "core-baytrail/core-isa.h"
#include "overlay_tool.h"

typedef struct
{
  int reg_num;
  int bit_start;
  int bit_size;
} xtensa_reg_mask_t;

typedef struct
{
  int count;
  xtensa_reg_mask_t *mask;
} xtensa_mask_t;

#if 0
/* Masked registers.  */
static xtensa_reg_mask_t xtensa_submask0[] = { { 43, 0, 1 } };
static const xtensa_mask_t xtensa_mask0 = { 1, xtensa_submask0 };
static xtensa_reg_mask_t xtensa_submask1[] = { { 43, 1, 1 } };
static const xtensa_mask_t xtensa_mask1 = { 1, xtensa_submask1 };
static xtensa_reg_mask_t xtensa_submask2[] = { { 43, 2, 1 } };
static const xtensa_mask_t xtensa_mask2 = { 1, xtensa_submask2 };
static xtensa_reg_mask_t xtensa_submask3[] = { { 43, 3, 1 } };
static const xtensa_mask_t xtensa_mask3 = { 1, xtensa_submask3 };
static xtensa_reg_mask_t xtensa_submask4[] = { { 43, 4, 1 } };
static const xtensa_mask_t xtensa_mask4 = { 1, xtensa_submask4 };
static xtensa_reg_mask_t xtensa_submask5[] = { { 43, 5, 1 } };
static const xtensa_mask_t xtensa_mask5 = { 1, xtensa_submask5 };
static xtensa_reg_mask_t xtensa_submask6[] = { { 43, 6, 1 } };
static const xtensa_mask_t xtensa_mask6 = { 1, xtensa_submask6 };
static xtensa_reg_mask_t xtensa_submask7[] = { { 43, 7, 1 } };
static const xtensa_mask_t xtensa_mask7 = { 1, xtensa_submask7 };
static xtensa_reg_mask_t xtensa_submask8[] = { { 43, 8, 1 } };
static const xtensa_mask_t xtensa_mask8 = { 1, xtensa_submask8 };
static xtensa_reg_mask_t xtensa_submask9[] = { { 43, 9, 1 } };
static const xtensa_mask_t xtensa_mask9 = { 1, xtensa_submask9 };
static xtensa_reg_mask_t xtensa_submask10[] = { { 43, 10, 1 } };
static const xtensa_mask_t xtensa_mask10 = { 1, xtensa_submask10 };
static xtensa_reg_mask_t xtensa_submask11[] = { { 43, 11, 1 } };
static const xtensa_mask_t xtensa_mask11 = { 1, xtensa_submask11 };
static xtensa_reg_mask_t xtensa_submask12[] = { { 43, 12, 1 } };
static const xtensa_mask_t xtensa_mask12 = { 1, xtensa_submask12 };
static xtensa_reg_mask_t xtensa_submask13[] = { { 43, 13, 1 } };
static const xtensa_mask_t xtensa_mask13 = { 1, xtensa_submask13 };
static xtensa_reg_mask_t xtensa_submask14[] = { { 43, 14, 1 } };
static const xtensa_mask_t xtensa_mask14 = { 1, xtensa_submask14 };
static xtensa_reg_mask_t xtensa_submask15[] = { { 43, 15, 1 } };
static const xtensa_mask_t xtensa_mask15 = { 1, xtensa_submask15 };
static xtensa_reg_mask_t xtensa_submask16[] = { { 42, 0, 4 } };
static const xtensa_mask_t xtensa_mask16 = { 1, xtensa_submask16 };
static xtensa_reg_mask_t xtensa_submask17[] = { { 42, 5, 1 } };
static const xtensa_mask_t xtensa_mask17 = { 1, xtensa_submask17 };
static xtensa_reg_mask_t xtensa_submask18[] = { { 42, 18, 1 } };
static const xtensa_mask_t xtensa_mask18 = { 1, xtensa_submask18 };
static xtensa_reg_mask_t xtensa_submask19[] = { { 42, 4, 1 } };
static const xtensa_mask_t xtensa_mask19 = { 1, xtensa_submask19 };
static xtensa_reg_mask_t xtensa_submask20[] = { { 42, 16, 2 } };
static const xtensa_mask_t xtensa_mask20 = { 1, xtensa_submask20 };
static xtensa_reg_mask_t xtensa_submask21[] = { { 42, 8, 4 } };
static const xtensa_mask_t xtensa_mask21 = { 1, xtensa_submask21 };
static xtensa_reg_mask_t xtensa_submask22[] = { { 102, 8, 4 } };
static const xtensa_mask_t xtensa_mask22 = { 1, xtensa_submask22 };
static xtensa_reg_mask_t xtensa_submask23[] = { { 58, 6, 1 } };
static const xtensa_mask_t xtensa_mask23 = { 1, xtensa_submask23 };
static xtensa_reg_mask_t xtensa_submask24[] = { { 58, 0, 6 } };
static const xtensa_mask_t xtensa_mask24 = { 1, xtensa_submask24 };
static xtensa_reg_mask_t xtensa_submask25[] = { { 60, 0, 4 } };
static const xtensa_mask_t xtensa_mask25 = { 1, xtensa_submask25 };
static xtensa_reg_mask_t xtensa_submask26[] = { { 60, 4, 4 } };
static const xtensa_mask_t xtensa_mask26 = { 1, xtensa_submask26 };
static xtensa_reg_mask_t xtensa_submask27[] = { { 60, 12, 4 } };
static const xtensa_mask_t xtensa_mask27 = { 1, xtensa_submask27 };
static xtensa_reg_mask_t xtensa_submask28[] = { { 60, 8, 4 } };
static const xtensa_mask_t xtensa_mask28 = { 1, xtensa_submask28 };
static xtensa_reg_mask_t xtensa_submask29[] = { { 61, 0, 27 } };
static const xtensa_mask_t xtensa_mask29 = { 1, xtensa_submask29 };
static xtensa_reg_mask_t xtensa_submask30[] = { { 61, 27, 1 } };
static const xtensa_mask_t xtensa_mask30 = { 1, xtensa_submask30 };
#endif


static XtensaConfig baytrail __attribute__((unused)) = {
    .name = "baytrail",
    .gdb_regmap = {
        .num_regs = 158,
        .num_core_regs = 52,
        .reg = {
#include "core-baytrail/gdb-config.c"
        }
    },
    .clock_freq_khz = 5000,
    DEFAULT_SECTIONS
};

REGISTER_CORE(baytrail)
