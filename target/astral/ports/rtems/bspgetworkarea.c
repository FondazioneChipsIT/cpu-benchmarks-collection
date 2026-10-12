/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @brief BSP specific initialization support routines
 *
 */

/*
 * COPYRIGHT (c) 1989-2020.
 * On-Line Applications Research Corporation (OAR).
 * Cobham Gaisler AB.
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

#include <bsp.h>
#include <bsp/fdt.h>
#include <bsp/riscv-fdt.h>

#include <rtems/sysinit.h>
#include <rtems/malloc.h>
#include <rtems/score/heap.h>
#include <rtems/score/wkspacedata.h>

#include <libfdt.h>

/*
 *  These are provided by the linkcmds for ALL of the BSPs which use this file.
 */
extern char WorkAreaBase[];
extern char RamEnd[];

/* Free part of the cluster L1 after .rodata (see linkcmds) */
extern char L1HeapBegin[];
extern char L1HeapEnd[];

/*
 * Only one memory area (L2): the BSP uses wkspaceinitone.c/mallocinitone.c,
 * so the RTEMS workspace (objects, stacks, SMP locks) is always in L2.
 */
static Memory_Area _Memory_Areas[ 1 ];

static void bsp_memory_initialize( void )
{
  _Memory_Initialize( &_Memory_Areas[ 0 ], WorkAreaBase, RamEnd );
}

RTEMS_SYSINIT_ITEM(
  bsp_memory_initialize,
  RTEMS_SYSINIT_MEMORY,
  RTEMS_SYSINIT_ORDER_MIDDLE
);

/*
 * The cluster L1 does not support AXI atomics, so it can only hold plain
 * data.  Add it to the malloc heap only, after the heap is initialized.  With
 * CONFIGURE_UNIFIED_WORK_AREAS the malloc heap is the workspace, so kernel
 * objects could end up in L1: do not extend in that case.
 */
static void bsp_malloc_extend_l1( void )
{
  uintptr_t size = (uintptr_t) L1HeapEnd - (uintptr_t) L1HeapBegin;

  if ( _Workspace_Is_unified || RTEMS_Malloc_Heap == NULL ) {
    return;
  }

  if ( (uintptr_t) L1HeapEnd > (uintptr_t) L1HeapBegin ) {
    (void) _Heap_Extend( RTEMS_Malloc_Heap, L1HeapBegin, size, 0 );
  }
}

RTEMS_SYSINIT_ITEM(
  bsp_malloc_extend_l1,
  RTEMS_SYSINIT_MALLOC,
  RTEMS_SYSINIT_ORDER_LAST
);

static const Memory_Information _Memory_Information =
  MEMORY_INFORMATION_INITIALIZER( _Memory_Areas );

const Memory_Information *_Memory_Get( void )
{
  return &_Memory_Information;
}


// /* SPDX-License-Identifier: BSD-2-Clause */

// /**
//  * @file
//  *
//  * @brief BSP specific initialization support routines
//  *
//  */

// /*
//  * COPYRIGHT (c) 1989-2020.
//  * On-Line Applications Research Corporation (OAR).
//  * Cobham Gaisler AB.
//  *
//  * Redistribution and use in source and binary forms, with or without
//  * modification, are permitted provided that the following conditions
//  * are met:
//  * 1. Redistributions of source code must retain the above copyright
//  *    notice, this list of conditions and the following disclaimer.
//  * 2. Redistributions in binary form must reproduce the above copyright
//  *    notice, this list of conditions and the following disclaimer in the
//  *    documentation and/or other materials provided with the distribution.
//  *
//  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
//  * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
//  * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
//  * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
//  * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
//  * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
//  * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  * POSSIBILITY OF SUCH DAMAGE.
//  */

// #include <bsp.h>
// #include <bsp/fdt.h>
// #include <bsp/riscv-fdt.h>

// #include <rtems/sysinit.h>

// #include <libfdt.h>

// /*
//  *  These are provided by the linkcmds for ALL of the BSPs which use this file.
//  */
// extern char WorkAreaBase[];
// extern char RamEnd[];

// extern char L1Base[];
// extern char L1End[];         /* aggiunto */
// extern char L2End[];         /* aggiunto */

// static Memory_Area _Memory_Areas[ 1 ];   /* era 1 */

// static void bsp_memory_initialize( void )
// {
//   // _Memory_Initialize( &_Memory_Areas[ 0 ], WorkAreaBase, L2End );


//   // /* Area 1: La memoria SRAM a 0x50000000 (64 KB) usata come Workspace aggiuntivo */
//   // void *sram_start = (void *)0x50000000U;
//   // void *sram_end   = (void *)(0x50000000U + 0x10000U); /* 64 KB */

//   // _Memory_Initialize( &_Memory_Areas[ 1 ], sram_start, sram_end );
  
//   // doppia
//   // _Memory_Initialize( &_Memory_Areas[ 0 ], WorkAreaBase, L2End  );  /* L2 workspace */
//   // _Memory_Initialize( &_Memory_Areas[ 1 ], L1Base,       L1End  );  /* L1 aggiuntiva */

//   _Memory_Initialize( &_Memory_Areas[ 0 ], WorkAreaBase, L1End );

// }

// RTEMS_SYSINIT_ITEM(
//   bsp_memory_initialize,
//   RTEMS_SYSINIT_MEMORY,
//   RTEMS_SYSINIT_ORDER_MIDDLE
// );

// static const Memory_Information _Memory_Information =
//   MEMORY_INFORMATION_INITIALIZER( _Memory_Areas );

// const Memory_Information *_Memory_Get( void )
// {
//   return &_Memory_Information;
// }
