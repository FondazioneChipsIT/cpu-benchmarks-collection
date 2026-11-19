/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * magia_api: static library with the host-side API to drive MAGIA from RTEMS:
 * device init/stop, L2 data allocation and copies, mailbox access.
 * It is the layer below the OpenMP target plugin (libopenmptargetmagia),
 * like libhero below the HERO plugin.
 *
 * This header must stay usable from C++ (rtl.cpp includes only this one):
 * do NOT include ringbuf.h here, directly or indirectly.
 *
 * Copyright (C) 2026 Chips-IT
 */

#ifndef MAGIA_API_H
#define MAGIA_API_H

#include <stdint.h>

#include <rtems/magia/offload_layout.h>   /* MBOX_* codes, OFL_* layout */
#include <rtems/magia/mailbox.h>          /* host_mbox_read/write */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * MEMORY ALLOCATION. In HERO: hero_dev_l2_malloc (o1heap).
 * Here a bump allocator in [OFL_HEAP_BASE, OFL_HEAP_END) that never frees is
 * enough. Returns 0 when the heap is exhausted (as libomptarget expects NULL
 * from __tgt_rtl_data_alloc): 0 is never a valid L2 address.
 */
uintptr_t data_alloc(uint32_t size);

/*
 * MEMORY RELEASE. In HERO: hero_dev_l2_free (o1heap).
 * The bump allocator cannot free single blocks, so nothing is reclaimed (the
 * whole heap is reset by magia_dev_init()): only checks that dev was returned
 * by data_alloc(), i.e. lies in [OFL_HEAP_BASE, current top of the heap).
 * Returns 0 if valid, -1 otherwise.
 */
int data_free(uintptr_t dev);

/* HOST -> DEVICE COPY. In HERO: GOMP_OFFLOAD_host2dev. */
void data_submit(uintptr_t dev, const void *host, uint32_t size);

/* DEVICE -> HOST COPY. In HERO: GOMP_OFFLOAD_dev2host. */
void data_retrieve(void *host, uintptr_t dev, uint32_t size);

/*
 * DEVICE INITIALIZATION. In HERO: hero_dev_init + hero_dev_alloc_mboxes.
 * Resets the L2 heap and sets up the mailboxes with host_mbox_alloc().
 * Must be done BEFORE the MAGIA fetch enable.
 */
void magia_dev_init(void);

/*
 * END. In HERO the STOP is sent by __tgt_rtl_deinit / hero_dev_exit.
 * The device leaves gomp_offload_manager and main returns (GVSoC terminates).
 * timeout_ms is the maximum wait for a free slot in the h2a mailbox.
 * Then releases the mailboxes with host_mbox_free(), also on failure.
 * Returns 0 if STOP was written, -1 on timeout.
 */
int magia_dev_stop(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* MAGIA_API_H */
