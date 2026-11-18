/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * magia_mailbox: host side of the HERO software mailboxes shared with MAGIA.
 *
 * Copyright (C) 2026 Chips-IT
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems/magia_mailbox.h>

#include <rtems/counter.h>

#include <inttypes.h>
#include <stdio.h>

void invalidate_ready(void)
{
    *(volatile uint32_t *)OFL_READY_FLAG = 0;
    *(volatile uint32_t *)OFL_H2A_ADDR   = 0;
    *(volatile uint32_t *)OFL_A2H_ADDR   = 0;
    __asm__ volatile("fence w,w" ::: "memory");
}

int mbox_wait(int (*op)(volatile struct ring_buf *, void *),
              volatile struct ring_buf *rb, uint32_t *w, uint32_t timeout_ms)
{
    uint64_t limit = (uint64_t)rtems_counter_frequency() * timeout_ms / 1000u;
    uint64_t elapsed = 0;
    rtems_counter_ticks last = rtems_counter_read();

    while ((*op)(rb, w)) {
        rtems_counter_ticks now = rtems_counter_read();
        elapsed += rtems_counter_difference(now, last);
        last = now;
        if (elapsed >= limit)
            return -1;
    }
    return 0;
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
