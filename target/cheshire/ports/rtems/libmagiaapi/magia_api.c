/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * magia_api: host-side API to drive MAGIA from RTEMS.
 *
 * Copyright (C) 2026 Chips-IT
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems/magia/magia_api.h>

#include <inttypes.h>
#include <stdio.h>

static uintptr_t heap_next;

uintptr_t data_alloc(uint32_t size)
{
    uintptr_t p = heap_next;
    uintptr_t aligned = ((uintptr_t)size + 7u) & ~(uintptr_t)7u;

    if (heap_next == 0) {               // magia_dev_init() not called yet
        printf("[host] data_alloc before magia_dev_init\n");
        return 0;
    }
    if (aligned > OFL_HEAP_END - heap_next) {
        printf("[host] L2 heap exhausted (%" PRIu32 " bytes requested)\n", size);
        return 0;
    }
    heap_next += aligned;
    return p;
}

int data_free(uintptr_t dev)
{
    if (dev < OFL_HEAP_BASE || dev >= heap_next) {
        printf("[host] data_free: 0x%08" PRIxPTR " not allocated in the L2 heap\n", dev);
        return -1;
    }
    return 0;   // bump allocator: memory is reclaimed only by magia_dev_init()
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
    heap_next = OFL_HEAP_BASE;
    host_mbox_alloc();
}

int magia_dev_stop(uint32_t timeout_ms)
{
    int ret = host_mbox_write(MBOX_DEVICE_STOP, timeout_ms);

    if (ret == 0)
        printf("[host] STOP sent\n");
    else
        printf("[host] FAIL: STOP send failed\n");
    host_mbox_free();
    return ret;
}
