/* Generic DMA support for heterogeneous virtualization.
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
#include "hw/audio/adsp-host.h"
#include "hw/adsp/shim.h"
#include "hw/adsp/log.h"
#include <sys/mman.h>

static void dma_M2M_create_read_shm(struct adsp_host *adsp, struct qemu_io_msg *msg)
{
    struct qemu_io_msg_dma32 *dma_msg = (struct qemu_io_msg_dma32 *)msg;
    struct qemu_io_msg_dma32 ack_msg = *dma_msg;
    hwaddr size = dma_msg->size;
    void *data, *cpu_data;
    int err;

    /* make sure request is valid */
    if (dma_msg->size == 0) {
        fprintf(stderr, "error: DMA M2M read size is 0\n");
        return;
    }

    /* allocate context and SHM buffer */
    data = g_malloc(dma_msg->size);
    ack_msg.host_data = (uint64_t)data;

    /* map guest physical address space */
    cpu_data = cpu_physical_memory_map(dma_msg->src, &size, 0);

    /* share guest via SHM */
    err = qemu_io_register_shm(
        adsp->dma_shm_buffer[dma_msg->dmac_id][dma_msg->chan_id].name,
        ADSP_IO_SHM_DMA(dma_msg->dmac_id, dma_msg->chan_id),
        dma_msg->size, &data);
    if (err < 0)
        fprintf(stderr, "error: cant alloc dma SHM %d\n", err);

    /* copy CPU data to SHM and unmap CPU */
    memcpy(data, cpu_data, dma_msg->size);
    cpu_physical_memory_unmap(cpu_data, size, 0, size);

    /* send IRQ to DSP client */
    ack_msg.hdr.type = QEMU_IO_TYPE_DMA;
    ack_msg.hdr.msg = QEMU_IO_DMA_REQ_READY;
    ack_msg.size = sizeof(struct qemu_io_msg_dma32);

    qemu_io_send_msg(&ack_msg.hdr);
}

static void dma_M2M_destroy_read_shm(struct adsp_host *adsp, struct qemu_io_msg *msg)
{
    struct qemu_io_msg_dma32 *dma_msg = (struct qemu_io_msg_dma32 *)msg;

    qemu_io_free_shm(ADSP_IO_SHM_DMA(dma_msg->dmac_id, dma_msg->chan_id));
    g_free((void*)dma_msg->host_data);
}

static void dma_M2M_create_write_shm(struct adsp_host *adsp, struct qemu_io_msg *msg)
{
    struct qemu_io_msg_dma32 *dma_msg = (struct qemu_io_msg_dma32 *)msg;
    struct qemu_io_msg_dma32 ack_msg = *dma_msg;
    void *data;
    int err;

    /* make sure request is valid */
    if (dma_msg->size == 0) {
        fprintf(stderr, "error: DMA M2M write size is 0\n");
        return;
    }

    /* allocate context and SHM buffer */
    data = g_malloc(dma_msg->size);
    ack_msg.host_data = (uint64_t)data;

    /* share guest via SHM */
    err = qemu_io_register_shm(
        adsp->dma_shm_buffer[dma_msg->dmac_id][dma_msg->chan_id].name,
        ADSP_IO_SHM_DMA(dma_msg->dmac_id, dma_msg->chan_id),
        dma_msg->size, &data);
    if (err < 0)
        fprintf(stderr, "error: cant alloc dma SHM %d\n", err);

    /* send IRQ to DSP client */
    ack_msg.hdr.type = QEMU_IO_TYPE_DMA;
    ack_msg.hdr.msg = QEMU_IO_DMA_REQ_READY;
    ack_msg.size = sizeof(struct qemu_io_msg_dma32);

    qemu_io_send_msg(&ack_msg.hdr);
}

static void dma_M2M_destroy_write_shm(struct adsp_host *adsp, struct qemu_io_msg *msg)
{
    struct qemu_io_msg_dma32 *dma_msg = (struct qemu_io_msg_dma32 *)msg;
    void *cpu_data, *data = (void*)dma_msg->host_data;
    hwaddr size = dma_msg->size;

    /* sanity check */
    if (data == NULL) {
        fprintf(stderr, "error: DMA M2M write no source buffer\n");
        goto free;
    }

    /* map guest physical address space */
    cpu_data = cpu_physical_memory_map(dma_msg->src, &size, 1);

    /* copy SHM data to CPU and unmap CPU */
    memcpy(cpu_data, data, dma_msg->size);
    cpu_physical_memory_unmap(cpu_data, size, 1, size);

free:
    qemu_io_free_shm(ADSP_IO_SHM_DMA(dma_msg->dmac_id, dma_msg->chan_id));
    g_free((void*)dma_msg->host_data);
}


void adsp_host_do_dma(struct adsp_host *adsp, struct qemu_io_msg *msg)
{
    struct qemu_io_msg_dma32 *dma_msg = (struct qemu_io_msg_dma32 *)msg;

    switch (dma_msg->hdr.msg) {
    case QEMU_IO_DMA_REQ_NEW:
        if (dma_msg->direction == QEMU_IO_DMA_DIR_READ)
            dma_M2M_create_read_shm(adsp, msg);
        else
            dma_M2M_create_write_shm(adsp, msg);
        break;
    case QEMU_IO_DMA_REQ_COMPLETE:
        if (dma_msg->direction == QEMU_IO_DMA_DIR_READ)
            dma_M2M_destroy_read_shm(adsp, msg);
        else
            dma_M2M_destroy_write_shm(adsp, msg);
        break;
    default:
        break;
    }
}
