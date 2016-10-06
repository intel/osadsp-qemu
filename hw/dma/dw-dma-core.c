/* Virtualization support for DesignWare DMA Engine.
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
#include "sysemu/sysemu.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "elf.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"
#include "qemu/thread.h"
#include "qemu/io-bridge.h"

#include "hw/pci/pci.h"
#include "hw/audio/adsp-dev.h"
#include "hw/audio/adsp-host.h"
#include "hw/adsp/shim.h"
#include "hw/adsp/log.h"
#include "hw/ssi/ssp.h"
#include "hw/dma/dw-dma.h"


const struct adsp_reg_desc adsp_gp_dma_map[] = {
    {.name = "dma", .enable = LOG_DMA,
        .offset = 0x00000000, .size = 0x4000},
};

static inline void dma_sleep(long nsec)
{
    struct timespec req;

    req.tv_sec = 0;
    req.tv_nsec = nsec;

    if (nanosleep(&req, NULL) < 0)
        fprintf(stderr, "failed to sleep %d\n", -errno);
}

static void dmac_reg_sync(struct adsp_gp_dmac *dmac, hwaddr addr)
{
    uint32_t val = 0;

    switch (addr) {
    case DW_STATUS_BLOCK:
        val = 0xff & dmac->io[DW_RAW_BLOCK >> 2] & ~dmac->io[DW_MASK_BLOCK >> 2];
        dmac->io[DW_STATUS_BLOCK >> 2] = val;
        dmac_reg_sync(dmac, DW_INTR_STATUS);
        break;
    case DW_STATUS_TFR:
        val = 0xff & dmac->io[DW_RAW_TFR >> 2] & ~dmac->io[DW_MASK_TFR >> 2];
        dmac->io[DW_STATUS_TFR >> 2] = val;
        dmac_reg_sync(dmac, DW_INTR_STATUS);
        break;
    case DW_INTR_STATUS:
        if (dmac->io[DW_STATUS_BLOCK >> 2] & 0xff)
            dmac->io[DW_INTR_STATUS >> 2] |= DW_INTR_STATUS_BLOCK;
        else
            dmac->io[DW_INTR_STATUS >> 2] &= ~DW_INTR_STATUS_BLOCK;
        if (dmac->io[DW_STATUS_TFR >> 2] & 0xff)
            dmac->io[DW_INTR_STATUS >> 2] |= DW_INTR_STATUS_TFR;
        else
            dmac->io[DW_INTR_STATUS >> 2] &= ~DW_INTR_STATUS_TFR;

        log_text(dmac->log, LOG_DMA_IRQ,
            "IRQ: from DMAC %d status %x block %x tfr %x\n", dmac->id,
            dmac->io[DW_INTR_STATUS >> 2],
            dmac->io[DW_STATUS_BLOCK >> 2],
            dmac->io[DW_STATUS_TFR >> 2]);

        if (dmac->io[DW_INTR_STATUS >> 2] && !dmac->irq_assert) {
            dmac->irq_assert = 1;

            log_text(dmac->log, LOG_DMA_IRQ, "IRQ: DMAC %d set\n",
                dmac->id);

            dmac->do_irq(dmac, 1);

        } else if (dmac->io[DW_INTR_STATUS >> 2] == 0 && dmac->irq_assert) {
            dmac->irq_assert = 0;

            log_text(dmac->log, LOG_DMA_IRQ,"IRQ: DMAC %d clear\n",
                dmac->id);
            dmac->do_irq(dmac, 0);
        }
        break;
    default:
        break;
    }
}

static void dma_host_req(struct adsp_gp_dmac *dmac, uint32_t chan,
    uint32_t direction)
{
    struct dma_chan *dma_chan = &dmac->dma_chan[chan];
    struct qemu_io_msg_dma32 *dma_msg = &dma_chan->dma_msg;

    /* send IRQ to parent */
    dma_msg->hdr.type = QEMU_IO_TYPE_DMA;
    dma_msg->hdr.msg = QEMU_IO_DMA_REQ_NEW;
    dma_msg->hdr.size = sizeof(*dma_msg);
    dma_msg->host_data = 0;
    dma_msg->src = dmac->io[DW_SAR(chan) >> 2];
    dma_msg->dest = dmac->io[DW_DAR(chan) >> 2];
    dma_msg->size = dmac->io[DW_CTRL_HIGH(chan) >> 2] & DW_CTLH_BLOCK_TS_MASK;
    dma_msg->dmac_id = dmac->id;
    dma_msg->chan_id = chan;
    dma_msg->client_data = (uint64_t)dma_chan;
    dma_msg->direction = direction;

    log_text(dmac->log, LOG_DMA_M2M,
        "DMA req: src 0x%x dest 0x%x size 0x%x\n",
        dma_msg->src, dma_msg->dest, dma_msg->size);

    qemu_io_send_msg(&dma_msg->hdr);
}

static void dma_host_complete(struct adsp_gp_dmac *dmac, uint32_t chan)
{
    struct dma_chan *dma_chan = &dmac->dma_chan[chan];
    struct qemu_io_msg_dma32 *dma_msg = &dma_chan->dma_msg;

    /* send IRQ to parent */
    dma_msg->hdr.type = QEMU_IO_TYPE_DMA;
    dma_msg->hdr.msg = QEMU_IO_DMA_REQ_COMPLETE;
    dma_msg->size = sizeof(struct qemu_io_msg_dma32);

    log_text(dmac->log, LOG_DMA_M2M,
        "DMA req complete: src 0x%x dest 0x%x size 0x%x\n",
        dma_msg->src, dma_msg->dest, dma_msg->size);

    qemu_io_send_msg(&dma_msg->hdr);
}

static int dma_llp_reloaded(struct dma_chan *dma_chan)
{
    struct adsp_gp_dmac *dmac = dma_chan->dmac;
    struct dw_lli2 *lli;
    bool src_llp_en, dst_llp_en;
    hwaddr lli_size = sizeof(*lli);
    int chan = dma_chan->chan;

    /* pointer to next LLP */
    if (dmac->io[DW_LLP(chan) >> 2] == 0 || dma_chan->stop)
        return 0;

    /* get next LLP */
    lli = cpu_physical_memory_map(dmac->io[DW_LLP(chan) >> 2], &lli_size, 0);
    if (lli == NULL) {
        printf("cant map LLP at 0x%x\n", dmac->io[DW_LLP(chan) >> 2]);
        return 0;
    }

    /* update SAR */
    src_llp_en = (dmac->io[DW_CTRL_LOW(chan) >> 2] & DW_CTLL_LLP_S_EN) != 0;
    if (src_llp_en)
        dmac->io[DW_SAR(chan) >> 2] = lli->sar;

    /* update DAR */
    dst_llp_en = (dmac->io[DW_CTRL_LOW(chan) >> 2] & DW_CTLL_LLP_D_EN) != 0;
    if (dst_llp_en)
        dmac->io[DW_DAR(chan) >> 2] = lli->dar;

    /* update LLP */
    dmac->io[DW_LLP(chan) >> 2] = lli->llp;

    /* update CTL_LO */
    dmac->io[DW_CTRL_LOW(chan) >> 2] = lli->ctrl_lo;

    /* update CTL_HI */
    dmac->io[DW_CTRL_HIGH(chan) >> 2] = lli->ctrl_hi;

    /* unmap LLP */
    cpu_physical_memory_unmap(lli, lli_size, 0, lli_size);

    log_text(dmac->log, LOG_DMA,
        "LLP reload SAR 0x%x DAR 0x%x size 0x%x\n",
        dmac->io[DW_SAR(chan) >> 2], dmac->io[DW_DAR(chan) >> 2],
        dmac->io[DW_CTRL_HIGH(chan) >> 2] & DW_CTLH_BLOCK_TS_MASK);

    return 1;
}

/* read from MEM and write to SSP - audio playback */
static int dma_M2P_copy_burst(struct dma_chan *dma_chan)
{
    struct adsp_gp_dmac *dmac = dma_chan->dmac;
    struct adsp_ssp *ssp = ssp_get_port(dma_chan->ssp);
    uint32_t chan = dma_chan->chan;
    hwaddr burst_size;
    uint32_t buffer[0x1000];
    hwaddr size, sar;

    sar = dmac->io[DW_SAR(chan) >> 2];
    size = dmac->io[DW_CTRL_HIGH(chan) >> 2] & DW_CTLH_BLOCK_TS_MASK;
    burst_size = size / 2;

    /* copy burst from SAR to DAR */
    cpu_physical_memory_read(sar, buffer, burst_size);

    /* copy buffer to files */
    if (dma_chan->fd > 0) {
        if (write(dma_chan->fd, buffer, burst_size) != burst_size)
            fprintf(stderr, "error: writing to DMAC file %d\n", -errno);
    }

    if (ssp->tx.fd > 0) {
        if (write(ssp->tx.fd, buffer, burst_size) != burst_size)
            fprintf(stderr, "error: writing to SSP%d file %d\n", dma_chan->ssp, -errno);
    }

    /* update SAR, DAR and bytes copied */
    dmac->io[DW_SAR(chan) >> 2] += burst_size;
    dma_chan->ptr += burst_size;
    dma_chan->bytes += burst_size;
    dma_chan->tbytes += burst_size;

    /* block complete ? then send IRQ */
    if (dma_chan->bytes >= size || dma_chan->stop) {

        /* assert block interrupt */
        dmac->io[DW_RAW_BLOCK >> 2] |= CHAN_RAW_ENABLE(chan);
        dmac_reg_sync(dmac, DW_STATUS_BLOCK);

        /* reload LLP and re-arm the timer if LLP exists */
        if (dma_llp_reloaded(dma_chan) && !dma_chan->stop) {

            /* map CPU memory for next block */
            size = dmac->io[DW_CTRL_HIGH(chan) >> 2] & DW_CTLH_BLOCK_TS_MASK;

            /* prepare timer context */
            dma_chan->bytes = 0;

            log_text(dmac->log, LOG_DMA_M2P,
                "dma: %d:%d: completed SAR 0x%lx DAR 0x%lx size 0x%x total bytes 0x%x\n",
                dmac->id, chan, sar, dmac->io[DW_DAR(chan) >> 2],
                dmac->io[DW_CTRL_HIGH(chan) >> 2] & DW_CTLH_BLOCK_TS_MASK,
                dma_chan->tbytes);

            /* start next block */
            return 1;
        } else {
            /* clear chan enable bit */
            dmac->io[DW_DMA_CHAN_EN >> 2] &= ~CHAN_RAW_ENABLE(chan);

            /* assert tfr interrupt */
            dmac->io[DW_RAW_TFR >> 2] |= CHAN_RAW_ENABLE(chan);
            dmac_reg_sync(dmac, DW_STATUS_TFR);

            /* transfer done */
            return 0;
        }
    } else {
        /* block not complete so re-arm timer */
        return 1;
    }
}

/* read from SSP and write to MEM - audio capture */
static int dma_P2M_copy_burst(struct dma_chan *dma_chan)
{
    struct adsp_gp_dmac *dmac = dma_chan->dmac;
    struct adsp_ssp *ssp = ssp_get_port(dma_chan->ssp);
    uint32_t chan = dma_chan->chan;
    hwaddr burst_size;
    hwaddr size, dar;
    uint32_t buffer[0x1000];

    size = dmac->io[DW_CTRL_HIGH(chan) >> 2] & DW_CTLH_BLOCK_TS_MASK;
    burst_size = size / 2;
    dar = dmac->io[DW_DAR(chan) >> 2];

    /* read in data from SSP file */
    if (ssp->rx.fd > 0) {
        if (write(ssp->rx.fd, buffer, burst_size) != burst_size)
            fprintf(stderr, "error: reading from SSP%d file %d\n", dma_chan->ssp, -errno);
    }

    /* copy burst from SAR to DAR */
    cpu_physical_memory_read(dar, buffer, burst_size);

    /* update SAR, DAR and bytes copied */
    dmac->io[DW_DAR(chan) >> 2] += burst_size;
    dma_chan->ptr += burst_size;
    dma_chan->bytes += burst_size;
    dma_chan->tbytes += burst_size;

    /* block complete ? then send IRQ */
    if (dma_chan->bytes >= size || dma_chan->stop) {

        /* assert block interrupt */
        dmac->io[DW_RAW_BLOCK >> 2] |= CHAN_RAW_ENABLE(chan);
        dmac_reg_sync(dmac, DW_STATUS_BLOCK);

        /* reload LLP and re-arm the timer if LLP exists */
        if (dma_llp_reloaded(dma_chan) && !dma_chan->stop) {

            /* map CPU memory for next block */
            size = dmac->io[DW_CTRL_HIGH(chan) >> 2] & DW_CTLH_BLOCK_TS_MASK;

            /* prepare timer context */
            dma_chan->bytes = 0;

            log_text(dmac->log, LOG_DMA_P2M,
                "dma: %d:%d: completed SAR 0x%lx DAR 0x%lx size 0x%x total bytes 0x%x\n",
                dmac->id, chan, dmac->io[DW_SAR(chan) >> 2], dar,
                dmac->io[DW_CTRL_HIGH(chan) >> 2] & DW_CTLH_BLOCK_TS_MASK,
                dma_chan->tbytes);

            /* start next block */
            return 1;
        } else {
            /* clear chan enable bit */
            dmac->io[DW_DMA_CHAN_EN >> 2] &= ~CHAN_RAW_ENABLE(chan);

            /* assert tfr interrupt */
            dmac->io[DW_RAW_TFR >> 2] |= CHAN_RAW_ENABLE(chan);
            dmac_reg_sync(dmac, DW_STATUS_TFR);

            /* transfer done */
            return 0;
        }
    } else {
        /* block not complete */
        return 1;
    }
}

/* read from host and write to DSP - audio playback */
static int dma_M2M_read_host_burst(struct dma_chan *dma_chan)
{
    struct adsp_gp_dmac *dmac = dma_chan->dmac;
    uint32_t chan = dma_chan->chan;
    hwaddr burst_size = 32; // TODO read from DMAC
    uint32_t dar, size;

    dar = dmac->io[DW_DAR(chan) >> 2];
    size = dmac->io[DW_CTRL_HIGH(chan) >> 2] & DW_CTLH_BLOCK_TS_MASK;

    /* copy burst from SAR to DAR */
    cpu_physical_memory_write(dar, dma_chan->ptr, burst_size);

    /* update SAR, DAR and bytes copied */
    dmac->io[DW_DAR(chan) >> 2] += burst_size;
    dmac->io[DW_SAR(chan) >> 2] += burst_size;

    dma_chan->ptr += burst_size;
    dma_chan->bytes += burst_size;
    dma_chan->tbytes += burst_size;

    if (dma_chan->fd > 0) {
        if (write(dma_chan->fd, dma_chan->ptr, burst_size) != burst_size)
            fprintf(stderr, "error: writing to DMAC file %d\n", -errno);
    }

    /* block complete ? then send IRQ */
    if (dma_chan->bytes >= size || dma_chan->stop) {

        /* assert block interrupt */
        dmac->io[DW_RAW_BLOCK >> 2] |= CHAN_RAW_ENABLE(chan);
        dmac_reg_sync(dmac, DW_STATUS_BLOCK);

        /* tell host we are complete */
        dma_host_complete(dmac, chan);

        /* free SHM */
        qemu_io_free_shm(ADSP_IO_SHM_DMA(dmac->id, chan)) ;

        /* reload LLP and re-arm the timer if LLP exists */
        if (dma_llp_reloaded(dma_chan) && !dma_chan->stop) {
            dma_host_req(dmac, chan, QEMU_IO_DMA_DIR_READ);

            log_text(dmac->log, LOG_DMA_M2M,
                "dma: %d:%d: completed SAR 0x%x DAR 0x%x size 0x%x total bytes 0x%x\n",
                dmac->id, chan, dmac->io[DW_SAR(chan) >> 2], dar,
                dmac->io[DW_CTRL_HIGH(chan) >> 2] & DW_CTLH_BLOCK_TS_MASK,
                dma_chan->tbytes);

            return 1;
        } else {
            /* clear chan enable bit */
            dmac->io[DW_DMA_CHAN_EN >> 2] &= ~CHAN_RAW_ENABLE(chan);

            /* assert tfr interrupt */
            dmac->io[DW_RAW_TFR >> 2] |= CHAN_RAW_ENABLE(chan);
            dmac_reg_sync(dmac, DW_STATUS_TFR);

            /* transfer complete */
            return 0;
        }
    } else {
        /* block not complete */
        return 1;
    }
}

/* read from DSP and write to host - audio capture */
static int dma_M2M_write_host_burst(struct dma_chan *dma_chan)
{
    struct adsp_gp_dmac *dmac = dma_chan->dmac;
    uint32_t chan = dma_chan->chan;
    hwaddr burst_size = 32; // TODO read from DMAC
    uint32_t sar, size;

    sar = dmac->io[DW_SAR(chan) >> 2];
    size = dmac->io[DW_CTRL_HIGH(chan) >> 2] & DW_CTLH_BLOCK_TS_MASK;

    /* copy burst from SAR to DAR */
    cpu_physical_memory_read(sar, dma_chan->ptr, burst_size);

    /* update SAR, DAR and bytes copied */
    dmac->io[DW_DAR(chan) >> 2] += burst_size;
    dmac->io[DW_SAR(chan) >> 2] += burst_size;
    dma_chan->ptr += burst_size;
    dma_chan->bytes += burst_size;
    dma_chan->tbytes += burst_size;

    /* block complete ? then send IRQ */
    if (dma_chan->bytes >= size || dma_chan->stop) {

        /* assert block interrupt */
        dmac->io[DW_RAW_BLOCK >> 2] |= CHAN_RAW_ENABLE(chan);
        dmac_reg_sync(dmac, DW_STATUS_BLOCK);

        /* tell host we are complete */
        dma_host_complete(dmac, chan);

        /* free SHm */
        qemu_io_free_shm(ADSP_IO_SHM_DMA(dmac->id, chan));

        /* reload LLP and re-arm the timer if LLP exists */
        if (dma_llp_reloaded(dma_chan) && !dma_chan->stop) {
            dma_host_req(dmac, chan, QEMU_IO_DMA_DIR_WRITE);

            log_text(dmac->log, LOG_DMA_M2M,
                "dma: %d:%d: completed SAR 0x%x DAR 0x%x size 0x%x total bytes 0x%x\n",
                dmac->id, chan, sar, dmac->io[DW_DAR(chan) >> 2],
                dmac->io[DW_CTRL_HIGH(chan) >> 2] & DW_CTLH_BLOCK_TS_MASK,
                dma_chan->tbytes);

            return 1;
        } else {
            /* clear chan enable bit */
            dmac->io[DW_DMA_CHAN_EN >> 2] &= ~CHAN_RAW_ENABLE(chan);

            /* assert tfr interrupt */
            dmac->io[DW_RAW_TFR >> 2] |= CHAN_RAW_ENABLE(chan);
            dmac_reg_sync(dmac, DW_STATUS_TFR);

            /* transfer complete */
            return 0;
        }
    } else {
        /* block not complete */
        return 1;
    }
}

static void open_dmac_file(struct dma_chan *dma_chan)
{
    struct adsp_gp_dmac *dmac = dma_chan->dmac;
    char name[32];

    /* create filename */
    sprintf(name, "/tmp/dmac%d-%d-%d.wav", dmac->id, dma_chan->chan,
        dma_chan->file_idx++);
    unlink(name);

    if (dma_chan->fd == 0)
        dma_chan->fd = open(name, O_WRONLY | O_CREAT,
            S_IRUSR | S_IWUSR | S_IXUSR);

    if (dma_chan->fd < 0)
        fprintf(stderr, "error: can't open file %s %d\n", name, -errno);
}

static void close_dmac_file(struct dma_chan *dma_chan)
{
    if (dma_chan->fd > 0) {
        close(dma_chan->fd);
        dma_chan->fd = 0;
    }
}

/* TODO: make these configurable on rate etc */
#define DMA_M2M_BURST_SLEEP    (200 * 1000)    /* 200us */
#define DMA_M2P_BURST_SLEEP    (6667 * 1000)    /* 6.66ms */
#define DMA_P2M_BURST_SLEEP    (6667 * 1000)    /* 6.66ms */

/* dsp mem to host mem work for capture */
static void * dma_channel_Mhost2Mdsp_work(void *data)
{
    struct dma_chan *dma_chan = data;

    open_dmac_file(dma_chan);

    do {
        dma_sleep(DMA_M2M_BURST_SLEEP);
    } while (dma_M2M_write_host_burst(dma_chan));

    close_dmac_file(dma_chan);

    return NULL;
}

/* dsp mem to host mem work for capture */
static void * dma_channel_Mdsp2Mhost_work(void *data)
{
    struct dma_chan *dma_chan = data;

    open_dmac_file(dma_chan);

    do {
        dma_sleep(DMA_M2M_BURST_SLEEP);
    } while (dma_M2M_read_host_burst(dma_chan));

    close_dmac_file(dma_chan);

    return NULL;
}

/* peripheral to mem channel work */
static void * dma_channel_P2M_work(void *data)
{
    struct dma_chan *dma_chan = data;

    open_dmac_file(dma_chan);

    do {
        dma_sleep(DMA_P2M_BURST_SLEEP);
    } while (dma_P2M_copy_burst(dma_chan));

    close_dmac_file(dma_chan);

    return NULL;
}

/* mem to peripheral channel work */
static void * dma_channel_M2P_work(void *data)
{
    struct dma_chan *dma_chan = data;

    open_dmac_file(dma_chan);

    do {
        dma_sleep(DMA_M2P_BURST_SLEEP);
    } while (dma_M2P_copy_burst(dma_chan));

    close_dmac_file(dma_chan);

    return NULL;
}

static void dma_P2M_start(struct adsp_gp_dmac *dmac, uint32_t chan, int ssp)
{
    struct dma_chan *dma_chan = &dmac->dma_chan[chan];

    /* prepare timer context */
    dma_chan->bytes = 0;
    dma_chan->ssp = ssp;
    dma_chan->tbytes = 0;

    qemu_thread_create(&dma_chan->thread, dma_chan->thread_name,
        dma_channel_P2M_work, dma_chan, QEMU_THREAD_DETACHED);
}

static void dma_M2P_start(struct adsp_gp_dmac *dmac, uint32_t chan, int ssp)
{
    struct dma_chan *dma_chan = &dmac->dma_chan[chan];

    /* prepare timer context */
    dma_chan->bytes = 0;
    dma_chan->ssp = ssp;
    dma_chan->tbytes = 0;

    qemu_thread_create(&dma_chan->thread, dma_chan->thread_name,
        dma_channel_M2P_work, dma_chan, QEMU_THREAD_DETACHED);
}

static void dma_Mdsp2Mhost_start(struct adsp_gp_dmac *dmac, uint32_t chan)
{
    struct dma_chan *dma_chan = &dmac->dma_chan[chan];

    /* prepare timer context */
    dma_chan->bytes = 0;
    dma_chan->tbytes = 0;

    qemu_thread_create(&dma_chan->thread, dma_chan->thread_name,
        dma_channel_Mdsp2Mhost_work, dma_chan, QEMU_THREAD_DETACHED);
}

static void dma_Mhost2Mdsp_start(struct adsp_gp_dmac *dmac, uint32_t chan)
{
    struct dma_chan *dma_chan = &dmac->dma_chan[chan];

    /* prepare timer context */
    dma_chan->bytes = 0;
    dma_chan->tbytes = 0;

    qemu_thread_create(&dma_chan->thread, dma_chan->thread_name,
        dma_channel_Mhost2Mdsp_work, dma_chan, QEMU_THREAD_DETACHED);
}

/* init new DMA mem to mem transfer after host is ready */
static void dma_M2M_do_transfer(struct dma_chan *dma_chan,
    uint32_t chan, int direction)
{
    struct adsp_gp_dmac *dmac = dma_chan->dmac;
    void *ptr = NULL;
    uint32_t size;
    int err;

    size = dmac->io[DW_CTRL_HIGH(chan) >> 2] & DW_CTLH_BLOCK_TS_MASK;

    /* create SHM region for DMA */
    err = qemu_io_register_shm(dma_chan->thread_name,
        ADSP_IO_SHM_DMA(dmac->id, chan), size, &ptr);
    if (err < 0) {
        fprintf(stderr, "error: can't create SHM size 0x%x for DMAC %d chan %d\n", size,
            dmac->id, chan);
        return;
    }

    /* prepare timer context */
    dma_chan->ptr = ptr;

    /* allocate new timer for DMAC channel and direction */
    if (direction == QEMU_IO_DMA_DIR_READ)
        dma_Mdsp2Mhost_start(dmac, chan);
    else
        dma_Mhost2Mdsp_start(dmac, chan);
}

/* stop DMA transaction */
static void dma_stop_transfer(struct adsp_gp_dmac *dmac, uint32_t chan)
{
    struct dma_chan *dma_chan = &dmac->dma_chan[chan];

    /* prepare timer context */
    dma_chan->stop = 1;

    log_text(dmac->log, LOG_DMA,
        "dma: %d:%d: stop SAR 0x%x DAR 0x%x size 0x%x total bytes 0x%x\n",
        dmac->id, chan, dmac->io[DW_SAR(chan) >> 2], dmac->io[DW_DAR(chan) >> 2],
        dmac->io[DW_CTRL_HIGH(chan) >> 2] & DW_CTLH_BLOCK_TS_MASK,
        dma_chan->tbytes);
}

/* init new DMA mem to mem playback transfer */
static void dma_start_transfer(struct adsp_gp_dmac *dmac, uint32_t chan)
{
    struct adsp_ssp *ssp;
    struct dma_chan *dma_chan;
    uint32_t sar, dar, ctl_lo;
    int i = 0;

    /* prepare timer context */
    dma_chan = &dmac->dma_chan[chan];
    dma_chan->stop = 0;
    dma_chan->tbytes = 0;

    /* determine transfer type */
    dar = dmac->io[DW_DAR(chan) >> 2];
    sar = dmac->io[DW_SAR(chan) >> 2];
    ctl_lo = dmac->io[DW_CTRL_LOW(chan) >> 2];
    ctl_lo = (ctl_lo >> 20) & 0x3;

    log_text(dmac->log, LOG_DMA,
        "dma: %d:%d: start SAR 0x%x DAR 0x%x size 0x%x\n",
        dmac->id, chan, sar, dar,
        dmac->io[DW_CTRL_HIGH(chan) >> 2] & DW_CTLH_BLOCK_TS_MASK);

    switch (ctl_lo) {
    case 0: /* DW_CTLL_FC_M2M */
        /* determine if we are to/from host - MSB == 1 then addr is DSP */
        if (sar & 0x80000000) {
            dma_host_req(dmac, chan, QEMU_IO_DMA_DIR_READ); /* capture */
            return;
        } else {
            dma_host_req(dmac, chan, QEMU_IO_DMA_DIR_WRITE); /* playback */
            return;
        }
        break;
    case 1: /*DW_CTLL_FC_M2P */
        while ((ssp = ssp_get_port(i++)) != NULL) {
            if (dar == ssp->ssp_dev->desc.base + SSDR) {
                        dma_M2P_start(dmac, chan, i - 1);
                        return;
            }
        }
        break;
    case 2: /* DW_CTLL_FC_P2M */
        while ((ssp = ssp_get_port(i++)) != NULL) {
            if (sar == ssp->ssp_dev->desc.base + SSDR) {
                        dma_P2M_start(dmac, chan, i - 1);
                        return;
            }
        }
        break;
    default:
        /* should not get here */
        log_text(dmac->log, LOG_DMA,
            "error: invalid DMA request %d - cant find source/target\n", ctl_lo);
        break;
    }

    /* should not get here */
    log_text(dmac->log, LOG_DMA,
        "error: invalid DMA request cant find device for type %d\n", ctl_lo);
}


void dw_dmac_reset(void *opaque)
{
    struct adsp_gp_dmac *dmac = opaque;
    const struct adsp_reg_space *gp_dmac_dev = dmac->desc;

    memset(dmac->io, 0, gp_dmac_dev->desc.size);
}

static uint64_t dmac_read(void *opaque, hwaddr addr,
        unsigned size)
{
    struct adsp_gp_dmac *dmac = opaque;
    const struct adsp_reg_space *gp_dmac_dev = dmac->desc;

    /* only print IO from guest */
    log_read(dmac->log, gp_dmac_dev, addr, size,
            dmac->io[addr >> 2]);

    return dmac->io[addr >> 2];
}

static void dmac_write(void *opaque, hwaddr addr,
        uint64_t val, unsigned size)
{
    struct adsp_gp_dmac *dmac = opaque;
    const struct adsp_reg_space *gp_dmac_dev = dmac->desc;
    int chan;
    uint32_t active, set;

    if ((addr != DW_DMA_CFG) && !(dmac->io[DW_DMA_CFG >> 2] & 1))
        return;

    set = val & ~dmac->io[addr >> 2];

    log_write(dmac->log, gp_dmac_dev, addr, val, size,
            dmac->io[addr >> 2]);

    /* special case registers */
    switch (addr) {
    case DW_DMA_CHAN_EN:
        dmac->io[addr >> 2] = val;
        for (chan = 0; chan < DW_MAX_CHAN; chan++) {
            if (val & CHAN_WRITE_ENABLE(chan)) {
                /* start or stop the DMA transferring*/
                if (val & CHAN_RAW_ENABLE(chan))
                    dma_start_transfer(dmac, chan);
            }
        }
        break;
    case DW_RAW_BLOCK:
        dmac->io[addr >> 2] = val;
        dmac_reg_sync(dmac, DW_STATUS_BLOCK);
        break;
    case DW_RAW_TFR:
        dmac->io[addr >> 2] = val;
        dmac_reg_sync(dmac, DW_STATUS_TFR);
        break;
    case DW_MASK_BLOCK:
        active = dmac->io[DW_MASK_BLOCK >> 2] & 0xff;

        for (chan = 0; chan < DW_MAX_CHAN; chan++) {
            if ((val & INT_MASK(chan)) == INT_MASK(chan)) {
                /* mask interrupt for the chan */
                active &= ~(CHAN_RAW_ENABLE(chan));
            } else if ((val & INT_UNMASK(chan)) == INT_UNMASK(chan)) {
                /* unmask interrupt for the chan */
                active |= CHAN_RAW_ENABLE(chan);
            }
        }

        dmac->io[DW_MASK_BLOCK >> 2] = active;
        dmac_reg_sync(dmac, DW_STATUS_BLOCK);
        break;
    case DW_MASK_TFR:
        active = dmac->io[DW_MASK_TFR >> 2] & 0xff;

        for (chan = 0; chan < DW_MAX_CHAN; chan++) {
            if ((val & INT_MASK(chan)) == INT_MASK(chan)) {
                /* mask interrupt for the chan */
                active &= ~(CHAN_RAW_ENABLE(chan));
            } else if ((val & INT_UNMASK(chan)) == INT_UNMASK(chan)) {
                /* unmask interrupt for the chan */
                active |= CHAN_RAW_ENABLE(chan);
            }
        }

        dmac->io[DW_MASK_TFR >> 2] = active;
        dmac_reg_sync(dmac, DW_STATUS_TFR);
        break;
    case DW_CLEAR_BLOCK:
        dmac->io[addr >> 2] = val;
        dmac->io[DW_RAW_BLOCK >> 2] &= ~val;
        dmac_reg_sync(dmac, DW_STATUS_BLOCK);
        break;
    case DW_CLEAR_TFR:
        dmac->io[addr >> 2] = val;
        dmac->io[DW_RAW_TFR >> 2] &= ~val;
        dmac_reg_sync(dmac, DW_STATUS_TFR);
        break;
    case DW_STATUS_TFR:
    case DW_STATUS_BLOCK:
        dmac->io[addr >> 2] = val;
        dmac_reg_sync(dmac, DW_INTR_STATUS);
        break;
    case DW_CFG_LOW(0):
        dmac->io[addr >> 2] = val;
        if (set & DW_CFG_CH_SUSPEND)
            dma_stop_transfer(dmac, 0);
        break;
    case DW_CFG_LOW(1):
        dmac->io[addr >> 2] = val;
        if (set & DW_CFG_CH_SUSPEND)
            dma_stop_transfer(dmac, 1);
        break;
    case DW_CFG_LOW(2):
        dmac->io[addr >> 2] = val;
        if (set & DW_CFG_CH_SUSPEND)
            dma_stop_transfer(dmac, 2);
        break;
    case DW_CFG_LOW(3):
        dmac->io[addr >> 2] = val;
        if (set & DW_CFG_CH_SUSPEND)
            dma_stop_transfer(dmac, 3);
        break;
    case DW_CFG_LOW(4):
        dmac->io[addr >> 2] = val;
        if (set & DW_CFG_CH_SUSPEND)
            dma_stop_transfer(dmac, 4);
        break;
    case DW_CFG_LOW(5):
        dmac->io[addr >> 2] = val;
        if (set & DW_CFG_CH_SUSPEND)
            dma_stop_transfer(dmac, 5);
        break;
    case DW_CFG_LOW(6):
        dmac->io[addr >> 2] = val;
        if (set & DW_CFG_CH_SUSPEND)
            dma_stop_transfer(dmac, 6);
        break;
    case DW_CFG_LOW(7):
        dmac->io[addr >> 2] = val;
        if (set & DW_CFG_CH_SUSPEND)
            dma_stop_transfer(dmac, 7);
        break;
    default:
        dmac->io[addr >> 2] = val;
        break;
    }
}

void dw_dma_msg(struct qemu_io_msg *msg)
{
    struct qemu_io_msg_dma32 *dma_msg = (struct qemu_io_msg_dma32 *)msg;
    struct dma_chan *dma_chan = (struct dma_chan*)dma_msg->client_data;
    struct qemu_io_msg_dma32 *local_dma_msg = &dma_chan->dma_msg;

    /* get host buffer address */
    local_dma_msg->host_data = dma_msg->host_data;

    if (msg->msg == QEMU_IO_DMA_REQ_READY) {

        dma_M2M_do_transfer(
            (struct dma_chan*)dma_msg->client_data,
            dma_msg->chan_id, dma_msg->direction);
    }
}

const MemoryRegionOps dw_dmac_ops = {
    .read = dmac_read,
    .write = dmac_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};
