/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * mailbox: host side of the HERO software mailboxes shared with MAGIA.
 *
 * Copyright (C) 2026 Chips-IT
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems/magia/mailbox.h>
#include <rtems/magia/ringbuf.h>

#include <rtems/counter.h>

#include <inttypes.h>
#include <stdio.h>

/*
 * With UMA (QEMU + GVSoC) host and MAGIA see the L2 at the same addresses,
 * so these pointers are valid on RTEMS too (see offload_layout.h).
 */
#define MAGIA_MBOX_H2A ((volatile struct ring_buf *) OFL_H2A_RING)
#define MAGIA_MBOX_A2H ((volatile struct ring_buf *) OFL_A2H_RING)

/*
 * Clears the "ready" flag and the mailbox addresses, then fence w,w: a flag
 * left over from a previous run is never seen as valid.
 */
static void invalidate_ready(void)
{
    *(volatile uint32_t *)OFL_READY_FLAG = 0;
    *(volatile uint32_t *)OFL_H2A_ADDR   = 0;
    *(volatile uint32_t *)OFL_A2H_ADDR   = 0;
    __asm__ volatile("fence w,w" ::: "memory");
}

/*
 * Retries op (rb_host_get/rb_host_put) until it succeeds or the timeout
 * expires. Time is measured with the CPU counter (CLINT mtime), which works
 * even without a clock driver. The counter is 32-bit: differences are
 * accumulated, so counter wrap-around is not a problem.
 * Returns 0 on success, -1 after timeout_ms milliseconds.
 * timeout_ms == OFL_MBOX_WAIT_FOREVER: retries without limit, never -1.
 */
static int mbox_wait(int (*op)(volatile struct ring_buf *, void *),
                     volatile struct ring_buf *rb, uint32_t *w, uint32_t timeout_ms)
{
    uint64_t limit = (uint64_t)rtems_counter_frequency() * timeout_ms / 1000u;
    uint64_t elapsed = 0;
    rtems_counter_ticks last = rtems_counter_read();

    if (timeout_ms == OFL_MBOX_WAIT_FOREVER) {
        while ((*op)(rb, w))
            ;
        return 0;
    }

    while ((*op)(rb, w)) {
        rtems_counter_ticks now = rtems_counter_read();
        elapsed += rtems_counter_difference(now, last);
        last = now;
        if (elapsed >= limit)
            return -1;
    }
    return 0;
}

void host_mbox_alloc(void)
{
    volatile struct ring_buf *h2a = MAGIA_MBOX_H2A;
    volatile struct ring_buf *a2h = MAGIA_MBOX_A2H;

    // First of all invalidate the "ready" cell: if the shared memory
    // survives between two runs, a flag left over from a previous run
    // must never be seen as valid while we set up the mailboxes.
    invalidate_ready();

    rb_init(h2a, OFL_RING_SLOTS, sizeof(uint32_t));
    rb_init(a2h, OFL_RING_SLOTS, sizeof(uint32_t));
    // data_v (address seen by the host) and data_p (seen by the device) coincide
    // with UMA. They differ only with a virtual-memory host (Linux in HERO).
    h2a->prod.data_v = h2a->prod.data_p = OFL_H2A_DATA;
    a2h->prod.data_v = a2h->prod.data_p = OFL_A2H_DATA;

    // Addresses first, then the flag, LAST (same order as the device).
    *(volatile uint32_t *)OFL_H2A_ADDR = OFL_H2A_RING;
    *(volatile uint32_t *)OFL_A2H_ADDR = OFL_A2H_RING;
    __asm__ volatile("fence w,w" ::: "memory");
    *(volatile uint32_t *)OFL_READY_FLAG = OFL_READY_MAGIC;
}

void host_mbox_free(void)
{
    // Rings deliberately left untouched: see mailbox.h
    invalidate_ready();
}

void print_mbox_state(void)
{
    volatile struct ring_buf *h2a = MAGIA_MBOX_H2A;
    volatile struct ring_buf *a2h = MAGIA_MBOX_A2H;

    printf("[host]   h2a head=%" PRIu32 " tail=%" PRIu32
           ", a2h head=%" PRIu32 " tail=%" PRIu32 ", ready=0x%08" PRIx32 "\n",
           h2a->prod.head, h2a->cons.tail, a2h->prod.head, a2h->cons.tail,
           *(volatile uint32_t *)OFL_READY_FLAG);
    // ready still OFL_READY_MAGIC: MAGIA never picked up the mailboxes (did it not start?)
}

// rb_host_put from ringbuf.h can be reused as is.
int host_mbox_write(uint32_t w, uint32_t timeout_ms)
{
    if (mbox_wait(rb_host_put, MAGIA_MBOX_H2A, &w, timeout_ms)) {
        printf("[host] timeout: h2a mailbox still full after %" PRIu32 " ms\n",
               timeout_ms);
        print_mbox_state();
        return -1;
    }
    return 0;
}

int host_mbox_read(uint32_t *w, uint32_t timeout_ms)
{
    if (mbox_wait(rb_host_get, MAGIA_MBOX_A2H, w, timeout_ms)) {
        printf("[host] timeout: no reply in the a2h mailbox after %" PRIu32 " ms\n",
               timeout_ms);
        print_mbox_state();
        return -1;
    }
    return 0;
}
