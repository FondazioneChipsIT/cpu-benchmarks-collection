/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * openmp_plugin: host side of the OpenMP offload to MAGIA (libomptarget +
 * HERO plugin equivalent).
 *
 * Copyright (C) 2026 Chips-IT
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems/openmp_plugin.h>

#include <rtems.h>

#include <inttypes.h>
#include <stdio.h>

static uintptr_t heap_next;
static uintptr_t dev_arg_buf;

uintptr_t data_alloc(uint32_t size)
{
    uintptr_t p = heap_next;
    heap_next += (size + 7u) & ~7u;
    if (heap_next > OFL_HEAP_END)
        rtems_panic("[host] L2 heap exhausted");
    return p;
}

void data_submit(uintptr_t dev, const void *host, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        ((volatile uint8_t *)dev)[i] = ((const uint8_t *)host)[i];
}

void data_retrieve(void *host, uintptr_t dev, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        ((uint8_t *)host)[i] = ((volatile uint8_t *)dev)[i];
}

void magia_dev_init(void)
{
    volatile struct ring_buf *h2a = MAGIA_MBOX_H2A;
    volatile struct ring_buf *a2h = MAGIA_MBOX_A2H;

    // First of all invalidate the "ready" cell: if the shared memory
    // survives between two runs, a flag left over from a previous run
    // must never be seen as valid while we set up the mailboxes.
    invalidate_ready();

    heap_next = OFL_HEAP_BASE;

    rb_init(h2a, OFL_RING_SLOTS, sizeof(uint32_t));
    rb_init(a2h, OFL_RING_SLOTS, sizeof(uint32_t));
    // data_v (address seen by the host) and data_p (seen by the device) coincide
    // with UMA. They differ only with a virtual-memory host (Linux in HERO).
    h2a->prod.data_v = h2a->prod.data_p = OFL_H2A_DATA;
    a2h->prod.data_v = a2h->prod.data_p = OFL_A2H_DATA;

    dev_arg_buf = data_alloc(ARG_BUF_SIZE * sizeof(uint64_t));

    // Addresses first, then the flag, LAST (same order as the device).
    *(volatile uint32_t *)OFL_H2A_ADDR = OFL_H2A_RING;
    *(volatile uint32_t *)OFL_A2H_ADDR = OFL_A2H_RING;
    __asm__ volatile("fence w,w" ::: "memory");
    *(volatile uint32_t *)OFL_READY_FLAG = OFL_READY_MAGIC;
}

int run_target_region(kernel_t kernel, uint32_t nargs, const uintptr_t *dev_ptrs,
                      uint32_t timeout_ms)
{
    uint64_t args[ARG_BUF_SIZE];
    uint32_t done[MBOX_DONE_WORDS];

    if (nargs > ARG_BUF_SIZE) {
        printf("[host] FAIL: %" PRIu32 " arguments, maximum %u\n", nargs, ARG_BUF_SIZE);
        return -1;
    }
    for (uint32_t i = 0; i < nargs; i++)
        args[i] = (uint64_t)dev_ptrs[i];
    data_submit(dev_arg_buf, args, nargs * sizeof(uint64_t));   // __tgt_rtl_data_submit

    if (host_mbox_write(MBOX_DEVICE_START, timeout_ms) ||
        host_mbox_write((uint32_t)(uintptr_t)kernel, timeout_ms) || // wrapper address
        host_mbox_write((uint32_t)dev_arg_buf, timeout_ms) ||       // argument buffer address
        host_mbox_write(0, timeout_ms))                             // no. of miss handlers (unused)
        return -1;

    // HERO discards words other than DONE only for MBOX_DEVICE_PRINT, which the
    // MAGIA device does not use: any other word is a protocol error.
    if (host_mbox_read(&done[0], timeout_ms))
        return -1;
    if (done[0] != MBOX_DEVICE_DONE) {
        printf("[host] FAIL: expected DONE (0x%02x), read 0x%08" PRIx32 "\n",
               MBOX_DEVICE_DONE, done[0]);
        return -1;
    }
    for (uint32_t i = 1; i < MBOX_DONE_WORDS; i++) {      // cycles, dma + 4 counters
        if (host_mbox_read(&done[i], timeout_ms)) {
            printf("[host] FAIL: incomplete DONE\n");
            return -1;
        }
    }
    // Data written by the kernel is read only after DONE has been seen
    __asm__ volatile("fence r,r" ::: "memory");
    printf("[host] region 0x%08" PRIx32 " DONE, kernel cycles = %" PRIu32 "\n",
           (uint32_t)(uintptr_t)kernel, done[1]);
    return 0;
}

void magia_dev_stop(uint32_t timeout_ms)
{
    if (host_mbox_write(MBOX_DEVICE_STOP, timeout_ms) == 0)
    {
        printf("[host] STOP sent\n");
    }
    else
    {
        printf("[host] FAIL: STOP send failed\n");
    }
    invalidate_ready();
}
