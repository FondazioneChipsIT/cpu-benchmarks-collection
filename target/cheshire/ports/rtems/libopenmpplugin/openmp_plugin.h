/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * openmp_plugin: static library with what libomptarget + the HERO plugin do
 * on the host side to offload OpenMP target regions to MAGIA: device
 * init/stop, L2 data allocation and copies, execution of a target region
 * through the mailbox protocol (START / DONE / STOP).
 *
 * Copyright (C) 2026 Chips-IT
 */

#ifndef OPENMP_PLUGIN_H
#define OPENMP_PLUGIN_H

#include <stdint.h>

#include <rtems/magia_elf_loader.h>   /* kernel_t */
#include <rtems/magia_mailbox.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Argument buffer size (words). In HERO (rtl.cpp, __tgt_rtl_init_device) there
 * is only one, dev_arg_buf, allocated at initialization and reused for every
 * offload: this works because the host waits for DONE before the next region.
 */
#define ARG_BUF_SIZE 16u                 // as in rtl.cpp:41

/*
 * MEMORY ALLOCATION. In HERO: __tgt_rtl_data_alloc (rtl.cpp) ->
 * GOMP_OFFLOAD_alloc (plugin-hero-dev.cpp) -> hero_dev_l2_malloc (o1heap).
 * Here a bump allocator in [OFL_HEAP_BASE, OFL_HEAP_END) that never frees is
 * enough. Calls rtems_panic() when the heap is exhausted.
 */
uintptr_t data_alloc(uint32_t size);

/* HOST -> DEVICE COPY. In HERO: __tgt_rtl_data_submit -> GOMP_OFFLOAD_host2dev. */
void data_submit(uintptr_t dev, const void *host, uint32_t size);

/* DEVICE -> HOST COPY. In HERO: __tgt_rtl_data_retrieve -> GOMP_OFFLOAD_dev2host. */
void data_retrieve(void *host, uintptr_t dev, uint32_t size);

/*
 * DEVICE INITIALIZATION. In HERO: __tgt_rtl_init_device ->
 * hero_dev_init / hero_dev_alloc_mboxes (mailboxes + LETTER_0/1). Here the
 * mailboxes are at fixed addresses and the "ready" cell replaces LETTER_0/1
 * (offload_layout.h). Must be done BEFORE the MAGIA fetch enable.
 */
void magia_dev_init(void);

/*
 * RUNNING A REGION. In HERO: __tgt_rtl_run_target_team_region
 * (rtl.cpp:457-519): copies the arguments into dev_arg_buf, sends START, kernel,
 * argbuf, no. of miss handlers, then WAITS for DONE and the 6 words that follow it.
 * timeout_ms is the maximum wait for each mailbox word (see magia_mailbox.h).
 * Returns 0 if the region completed, -1 otherwise.
 */
int run_target_region(kernel_t kernel, uint32_t nargs, const uintptr_t *dev_ptrs,
                      uint32_t timeout_ms);

/*
 * END. In HERO the STOP is sent by __tgt_rtl_deinit / hero_dev_exit.
 * The device leaves gomp_offload_manager and main returns (GVSoC terminates).
 * timeout_ms is the maximum wait for a free slot in the h2a mailbox.
 */
void magia_dev_stop(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* OPENMP_PLUGIN_H */
