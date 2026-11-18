/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * magia_mailbox: static library with the host side of the HERO software
 * mailboxes shared between RTEMS and MAGIA (h2a: host -> device, a2h:
 * device -> host) and of the "ready" cell that replaces LETTER_0/1.
 * The fixed addresses and message codes are in <rtems/magia/offload_layout.h>.
 *
 * Copyright (C) 2026 Chips-IT
 */

#ifndef MAGIA_MAILBOX_H
#define MAGIA_MAILBOX_H

#include <stdint.h>

#include <rtems/magia/ringbuf.h>
#include <rtems/magia/offload_layout.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * With UMA (QEMU + GVSoC) host and MAGIA see the L2 at the same addresses,
 * so these pointers are valid on RTEMS too (see offload_layout.h).
 */
#define MAGIA_MBOX_H2A ((volatile struct ring_buf *) OFL_H2A_RING)
#define MAGIA_MBOX_A2H ((volatile struct ring_buf *) OFL_A2H_RING)

/*
 * timeout_ms: maximum wait for each mailbox word, chosen by the application.
 * HERO waits without limit (while (hero_dev_mbox_read(...)); in rtl.cpp); here
 * a timeout keeps the application from hanging if MAGIA does not start or gets
 * stuck. Note that GVSoC simulates MAGIA much more slowly than QEMU's real time.
 */

/*
 * Clears the "ready" flag and the mailbox addresses, then fence w,w: a flag
 * left over from a previous run is never seen as valid.
 */
void invalidate_ready(void);

/*
 * Retries op (rb_host_get/rb_host_put) until it succeeds or the timeout
 * expires. Time is measured with the CPU counter (CLINT mtime), which works
 * even without a clock driver. The counter is 32-bit: differences are
 * accumulated, so counter wrap-around is not a problem.
 * Returns 0 on success, -1 after timeout_ms milliseconds.
 */
int mbox_wait(int (*op)(volatile struct ring_buf *, void *),
              volatile struct ring_buf *rb, uint32_t *w, uint32_t timeout_ms);

/* Prints head/tail of both mailboxes and the "ready" cell (for timeouts) */
void print_mbox_state(void);

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

#ifdef __cplusplus
}
#endif

#endif /* MAGIA_MAILBOX_H */
