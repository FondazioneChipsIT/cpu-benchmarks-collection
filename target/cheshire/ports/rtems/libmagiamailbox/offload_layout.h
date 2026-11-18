// offload_layout.h
//
// "Contract" between host and device: fixed addresses in L2 and message codes.
// Must be included both on the host side (RTEMS on QEMU) and on the device side (omptarget_magia.c).
//
// In HERO this role is played by:
//   - the LETTER_0/1 registers of the Carfield mailbox (HERO2/sw/libhero/src/host/carfield/snitch_cluster.h)
//   - the MBOX_* codes                                  (HERO2/sw/libhero/include/libhero/host/hero_api.h)
//
// MAGIA v4 memory map (magia-sdk/gvsoc/pulp/pulp/chips/magia_v4/arch.py, magia_v4/link.ld):
//   L2           0xC000_0000 - 0xFFFF_FFFF (L2_SIZE = 1 GB by default)
//   code         0xCC00_0000 (instrram, 32 KB)    <- DO NOT use
//   data/.bss    0xCC01_0000 (dataram)            <- DO NOT use
//   TEST_END     0xCCFF_0000                      <- DO NOT use
//   stdout       0xFFFF_0004                      <- DO NOT use
// The offload structures are therefore placed at the beginning of the L2.
//
// Riccardo: this file is the contract to share with the RTEMS host. With UMA
// memory (QEMU + GVSoC) host and MAGIA see the L2 at the SAME addresses, so
// these values hold unchanged on the host too. Still to be verified that the
// whole 0xC000_0000 - 0xC100_0000 range is shared.

#pragma once
#include <stdint.h>

// ---- "ready" cell: replaces LETTER_0/1 ------------------------------------
// Sequence (the shared memory may survive between two runs):
//   host:   flag = 0, addresses = 0, fence w,w        <- invalidate stale leftovers
//           initialize the mailboxes
//           write OFL_H2A_ADDR / OFL_A2H_ADDR, fence w,w
//           flag = OFL_READY_MAGIC                    <- LAST
//   device: wait for flag == OFL_READY_MAGIC, fence r,r
//           read OFL_H2A_ADDR / OFL_A2H_ADDR
//           fence r,w, flag = 0                       <- "consume" the flag
// Riccardo: the "host:" part of the sequence is what must be written on RTEMS,
// BEFORE the MAGIA fetch_enable: this way MAGIA starts only with the memory
// already set up and cannot see a flag left over from a previous session.

#define OFL_READY_FLAG   0xC0000000u   // the host writes OFL_READY_MAGIC here LAST
#define OFL_H2A_ADDR     0xC0000004u   // address of the host -> device mailbox
#define OFL_A2H_ADDR     0xC0000008u   // address of the device -> host mailbox
#define OFL_READY_MAGIC  0xCAFEBABEu

// ---- The two mailboxes (struct ring_buf from ringbuf.h, 64 B aligned) ------
#define OFL_H2A_RING     0xC0000100u
#define OFL_A2H_RING     0xC0000200u
#define OFL_H2A_DATA     0xC0001000u   // data area of the h2a mailbox
#define OFL_A2H_DATA     0xC0001400u   // data area of the a2h mailbox
#define OFL_RING_SLOTS   64u           // slots per mailbox (HERO uses 16; more here
                                       // because the fake host queues everything up front)

// The 64 words span 0xC000_1000 - 0xC000_10FF (h2a) and 0xC000_1400 - 0xC000_14FF (a2h).
// 64 slots per mailbox, 1 word (4 B) each = 256 B of data each. 64*4 = 256 bytes = 0x100
// ---- Heap for the data buffers (map) and the argument buffers ---------------
#define OFL_HEAP_BASE    0xC0010000u
#define OFL_HEAP_END     0xC1000000u

// ---- Protocol: identical to HERO (hero_api.h / sw_mailbox.h) ----------------
#define MBOX_DEVICE_START  (0x02U)   // host->dev: START, &function, &argbuf, 0
#define MBOX_DEVICE_DONE   (0x04U)   // dev->host: DONE, cycles, dma, tot, dma, issue, compute
#define MBOX_DEVICE_STOP   (0x0FU)   // host->dev: exit the loop
#define MBOX_DEVICE_LOGLVL (0x10U)   // host->dev: LOGLVL, value (ignored)
#define MBOX_DONE_WORDS    7u        // words making up a DONE reply (DONE included)
