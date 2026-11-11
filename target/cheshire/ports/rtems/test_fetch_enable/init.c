/* SPDX-License-Identifier: BSD-2-Clause */

/*
 *  COPYRIGHT (c) 1989-2012.
 *  On-Line Applications Research Corporation (OAR).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * RTEMS <-> MAGIA (GVSoC) synchronization test over the shared memory (UMA).
 *
 * Handshake:
 *   1. RTEMS invalidates the two sync words, then writes GUEST_READY;
 *   2. MAGIA sees GUEST_READY and writes MAGIA_TO_GUEST;
 *   3. RTEMS sees MAGIA_TO_GUEST and answers GUEST_TO_MAGIA.
 *
 * Run together with the MAGIA test test_uma_sync, e.g.
 *   launch_qemu_cheshire_gvsoc_magia.py --binary .../test_uma_sync \
 *       --bios .../testsuites/samples/test_fetch_enable.exe
 */

#include <rtems.h>
#include <tmacros.h>

const char rtems_test_name[] = "FETCH ENABLE";

/*
 * Fixed RTEMS <-> MAGIA exchange buffer in the shared memory, independent of
 * the application layout (gvsoc pulp/chips/magia_v2/arch.py, UMA_EXCHANGE_ADDR).
 * GVSoC maps the MAGIA L2 (0xC000_0000) on the part of the QEMU RAM seen at
 * 0xC000_0000, so RTEMS and MAGIA use the same address: 0xC100_0000.
 * Outside the RTEMS RAM region (0x80000000 + RISCV_RAM_REGION_SIZE = 4 MiB);
 * requires a QEMU RAM covering it (e.g. -m 2G).
 */
#define UMA_EXCHANGE_BUFFER 0xC1000000UL
#define exchange_buffer ((volatile uint32_t *) UMA_EXCHANGE_BUFFER)

//#define FETCH_ENABLE_BUFFER 0xCCFEFFF8UL
#define FETCH_ENABLE_BUFFER 0xCCFF0400UL
#define fetch_enable_buffer ((volatile uint32_t *) FETCH_ENABLE_BUFFER)

/* Exchange buffer layout (32-bit words) */
#define SYNC_FROM_MAGIA 0   /* written by MAGIA */
#define SYNC_TO_MAGIA   1   /* written by RTEMS */
#define GUEST_READY_IDX 2   /* written by RTEMS */

#define INVALID_PATTERN 0xFAFAFAFAU
#define GUEST_READY     0x5EADBEEFU
#define MAGIA_TO_GUEST  0x9ABCDEF0U
#define GUEST_TO_MAGIA  0xAAAAAAAAU
#define FETCH_ENABLE_PATTERN 0xFFFFFFFF

static inline void write_fence( void )
{
  __asm__ volatile ( "fence rw, w" ::: "memory" );
}

static rtems_task Init( rtems_task_argument ignored )
{
  (void) ignored;

  rtems_print_printer_fprintf_putc( &rtems_test_printer );
  TEST_BEGIN();
  printf( "[RTEMS] enabling Magia fetch\n" );
  fetch_enable_buffer[0] = FETCH_ENABLE_PATTERN;
  
  printf( "[RTEMS] enabled Magia fetch\n" );
  write_fence();
  exchange_buffer[ SYNC_FROM_MAGIA ] = INVALID_PATTERN;
  exchange_buffer[ SYNC_TO_MAGIA ] = INVALID_PATTERN;
  write_fence();
  exchange_buffer[ GUEST_READY_IDX ] = GUEST_READY;
  printf( "[RTEMS] guest ready, waiting for MAGIA\n" );

  while ( exchange_buffer[ SYNC_FROM_MAGIA ] != MAGIA_TO_GUEST ) {
    /* busy wait: the clock driver is disabled */
  }

  write_fence();
  exchange_buffer[ SYNC_TO_MAGIA ] = GUEST_TO_MAGIA;
  printf( "[RTEMS] received 0x%08" PRIx32 " from MAGIA, answered 0x%08" PRIx32 "\n",
          (uint32_t) MAGIA_TO_GUEST, (uint32_t) GUEST_TO_MAGIA );

  TEST_END();
  rtems_test_exit( 0 );
}

/* NOTICE: the clock driver is explicitly disabled */
#define CONFIGURE_APPLICATION_DOES_NOT_NEED_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_SIMPLE_CONSOLE_DRIVER

#define CONFIGURE_MAXIMUM_TASKS 1

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_INIT_TASK_ATTRIBUTES RTEMS_FLOATING_POINT

#define CONFIGURE_INITIAL_EXTENSIONS RTEMS_TEST_INITIAL_EXTENSION

#define CONFIGURE_INIT
#include <rtems/confdefs.h>
