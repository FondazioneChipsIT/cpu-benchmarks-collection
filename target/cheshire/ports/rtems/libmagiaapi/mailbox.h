/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * mailbox: host side of the HERO software mailboxes shared between RTEMS and
 * MAGIA (h2a: host -> device, a2h: device -> host) and of the "ready" cell
 * that replaces LETTER_0/1. Part of the magia_api static library.
 * The fixed addresses and message codes are in <rtems/magia/offload_layout.h>.
 *
 * This header does NOT include ringbuf.h: struct ring_buf and the rb_*
 * helpers are used only inside mailbox.c. This keeps the header usable from
 * C++ (ringbuf.h uses _Static_assert, which g++ rejects).
 *
 * Copyright (C) 2026 Chips-IT
 */

#ifndef MAGIA_API_MAILBOX_H
#define MAGIA_API_MAILBOX_H

#include <stdint.h>

#include <rtems/magia/offload_layout.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * timeout_ms: maximum wait for each mailbox word, chosen by the caller.
 * OFL_MBOX_WAIT_FOREVER waits without limit, as HERO does
 * (while (hero_dev_mbox_read(...)); in rtl.cpp). A finite timeout keeps the
 * application from hanging if MAGIA does not start or gets stuck. Note that
 * GVSoC simulates MAGIA much more slowly than QEMU's real time.
 */
#define OFL_MBOX_WAIT_FOREVER UINT32_MAX

/*
 * MAILBOX SETUP. In HERO: hero_dev_alloc_mboxes (mailboxes + LETTER_0/1).
 * Here the mailboxes are at fixed addresses (offload_layout.h), so nothing is
 * really allocated: the function runs the "host:" sequence of the contract:
 *   1. invalidate the "ready" cell and the mailbox addresses, fence w,w
 *   2. initialize both rings and their data areas
 *   3. write OFL_H2A_ADDR / OFL_A2H_ADDR, fence w,w
 *   4. write OFL_READY_MAGIC, LAST
 * Must be called BEFORE the MAGIA fetch enable.
 */
void host_mbox_alloc(void);

/*
 * MAILBOX RELEASE. In HERO: hero_dev_free_mboxes.
 * Only invalidates the "ready" cell and the mailbox addresses: the rings are
 * NOT reset, because a STOP just written in h2a may not have been read by
 * MAGIA yet and resetting head/tail would lose it.
 */
void host_mbox_free(void);

/*
 * WRITE to the h2a mailbox. In HERO: hero_dev_mbox_write (mailbox.c).
 * 0 = written, -1 = timeout (h2a still full after timeout_ms).
 */
int host_mbox_write(uint32_t w, uint32_t timeout_ms);

/*
 * READ from the a2h mailbox. In HERO: hero_dev_mbox_read (mailbox.c).
 * Blocking as in HERO, because the device runs in parallel.
 * 0 = read, -1 = timeout (a2h still empty after timeout_ms).
 */
int host_mbox_read(uint32_t *w, uint32_t timeout_ms);

/* Prints head/tail of both mailboxes and the "ready" cell (for timeouts) */
void print_mbox_state(void);

#ifdef __cplusplus
}
#endif

#endif /* MAGIA_API_MAILBOX_H */
