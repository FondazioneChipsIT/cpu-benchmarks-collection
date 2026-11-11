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

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>


const char rtems_test_name[] = "ELF LOADER";

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

/*
 * MAGIA entry point, read by GVSoC when it finds the fetch enable pattern
 * (gvsoc pulp/chips/magia_v2/arch.py, ENTRY_ADDR_START): must be written
 * before the fetch enable word.
 */
#define MAGIA_ENTRY_BUFFER 0xCCFF0408UL
#define magia_entry_buffer ((volatile uint32_t *) MAGIA_ENTRY_BUFFER)

/* Exchange buffer layout (32-bit words) */
#define SYNC_FROM_MAGIA 0   /* written by MAGIA */
#define SYNC_TO_MAGIA   1   /* written by RTEMS */
#define GUEST_READY_IDX 2   /* written by RTEMS */

#define INVALID_PATTERN 0xFAFAFAFAU
#define GUEST_READY     0x5EADBEEFU
#define MAGIA_TO_GUEST  0x9ABCDEF0U
#define GUEST_TO_MAGIA  0xAAAAAAAAU
#define FETCH_ENABLE_PATTERN 0xFFFFFFFF

typedef struct {
    uint64_t entry;
    int is_32;
} elf_info_t;

static inline void write_fence( void )
{
  __asm__ volatile ( "fence rw, w" ::: "memory" );
}

/* true if [off, off + len) is inside [0, size), without overflow */
static bool range_ok(uint64_t off, uint64_t len, uint64_t size)
{
    return off <= size && len <= size - off;
}

static void check_elf_ident(const unsigned char *ident)
{
    if (memcmp(ident, ELFMAG, SELFMAG) != 0) {
        rtems_panic("Input file is not an ELF");
    }

    if (ident[EI_DATA] != ELFDATA2LSB) {
        rtems_panic("Only little-endian ELF files are supported");
    }

    if (ident[EI_CLASS] != ELFCLASS32 && ident[EI_CLASS] != ELFCLASS64) {
        rtems_panic("Unsupported ELF class");
    }
}

/*
 * MAGIA L2 window as seen by RTEMS (gvsoc pulp/chips/magia_v2/arch.py:
 * L2_ADDR_START, L2_SIZE). GVSoC backs it with the QEMU RAM (UMA), so the
 * segments are copied directly at their physical address.
 */
#define MAGIA_L2_START 0xC0000000UL
#define MAGIA_L2_END   ( MAGIA_L2_START + 0x0CFEFFFFUL )

static void load_segment(uint64_t paddr,
                         const uint8_t *src,
                         uint64_t filesz,
                         uint64_t memsz)
{
    uint8_t *dst;

    if (filesz > memsz) {
        rtems_panic("ELF segment filesz > memsz");
    }

    if (paddr < MAGIA_L2_START || paddr > MAGIA_L2_END ||
        memsz > MAGIA_L2_END - paddr) {
        rtems_panic("ELF segment 0x%08" PRIx64 "+0x%" PRIx64
                    " outside the MAGIA L2", paddr, memsz);
    }

    dst = (uint8_t *)(uintptr_t)paddr;

    if (filesz > 0) {
        memcpy(dst, src, (size_t)filesz);
    }

    if (memsz > filesz) {
        memset(dst + filesz, 0, (size_t)(memsz - filesz));
    }

    if (filesz > 0 && memcmp(dst, src, (size_t)filesz) != 0) {
        rtems_panic("ELF segment readback mismatch at 0x%08" PRIx64, paddr);
    }
}

static void load_elf32_segments(const uint8_t *file_base,
                                size_t file_size,
                                elf_info_t *info)
{
    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *)file_base;
    const Elf32_Phdr *phdrs;

    if (file_size < sizeof(Elf32_Ehdr)) {
        rtems_panic("ELF32 header exceeds file size");
    }

    if (ehdr->e_machine != EM_RISCV) {
        rtems_panic("ELF32 machine %u is not RISC-V", (unsigned)ehdr->e_machine);
    }

    if (ehdr->e_phnum > 0 && ehdr->e_phentsize != sizeof(Elf32_Phdr)) {
        rtems_panic("ELF32 unexpected program header size %u",
                    (unsigned)ehdr->e_phentsize);
    }

    if (!range_ok(ehdr->e_phoff,
                  (uint64_t)ehdr->e_phnum * sizeof(Elf32_Phdr), file_size)) {
        rtems_panic("ELF32 program header table exceeds file size");
    }

    phdrs = (const Elf32_Phdr *)(file_base + ehdr->e_phoff);
    info->entry = ehdr->e_entry;
    info->is_32 = 1;

    for (int i = 0; i < ehdr->e_phnum; ++i) {
        const Elf32_Phdr *seg = &phdrs[i];

        if (seg->p_type != PT_LOAD || seg->p_memsz == 0) {
            continue;
        }

        if (!range_ok(seg->p_offset, seg->p_filesz, file_size)) {
            rtems_panic("ELF32 segment %d exceeds file size", i);
        }

        printf("[ELF32] LOAD %d: off=0x%08" PRIx32
               " paddr=0x%08" PRIx32
               " filesz=0x%08" PRIx32
               " memsz=0x%08" PRIx32 "\n",
               i,
               seg->p_offset,
               seg->p_paddr,
               seg->p_filesz,
               seg->p_memsz);

        load_segment(seg->p_paddr, file_base + seg->p_offset,
                     seg->p_filesz, seg->p_memsz);
    }
}

static void load_elf64_segments(const uint8_t *file_base,
                                size_t file_size,
                                elf_info_t *info)
{
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)file_base;
    const Elf64_Phdr *phdrs;

    if (file_size < sizeof(Elf64_Ehdr)) {
        rtems_panic("ELF64 header exceeds file size");
    }

    if (ehdr->e_machine != EM_RISCV) {
        rtems_panic("ELF64 machine %u is not RISC-V", (unsigned)ehdr->e_machine);
    }

    if (ehdr->e_phnum > 0 && ehdr->e_phentsize != sizeof(Elf64_Phdr)) {
        rtems_panic("ELF64 unexpected program header size %u",
                    (unsigned)ehdr->e_phentsize);
    }

    if (!range_ok(ehdr->e_phoff,
                  (uint64_t)ehdr->e_phnum * sizeof(Elf64_Phdr), file_size)) {
        rtems_panic("ELF64 program header table exceeds file size");
    }

    phdrs = (const Elf64_Phdr *)(file_base + ehdr->e_phoff);
    info->entry = ehdr->e_entry;
    info->is_32 = 0;

    for (int i = 0; i < ehdr->e_phnum; ++i) {
        const Elf64_Phdr *seg = &phdrs[i];

        if (seg->p_type != PT_LOAD || seg->p_memsz == 0) {
            continue;
        }

        if (!range_ok(seg->p_offset, seg->p_filesz, file_size)) {
            rtems_panic("ELF64 segment %d exceeds file size", i);
        }

        printf("[ELF64] LOAD %d: off=0x%016" PRIx64
               " paddr=0x%016" PRIx64
               " filesz=0x%016" PRIx64
               " memsz=0x%016" PRIx64 "\n",
               i,
               seg->p_offset,
               seg->p_paddr,
               seg->p_filesz,
               seg->p_memsz);

        load_segment(seg->p_paddr, file_base + seg->p_offset,
                     seg->p_filesz, seg->p_memsz);
    }
}

/* MAGIA ELF embedded in the image by magia_elf.c (.incbin) */
extern const unsigned char magia_elf[];
extern const size_t magia_elf_size;

static void load_elf_from_memory(const uint8_t *file_base,
                                 size_t file_size,
                                 elf_info_t *info)
{
    if (file_size == 0) {
        rtems_panic("No MAGIA ELF embedded: rebuild with MAGIA_ELF_PATH set "
                    "(make rtems MAGIA_ELF=/path/to/elf)");
    }

    if (file_size < EI_NIDENT) {
        rtems_panic("ELF image too small");
    }

    check_elf_ident(file_base);

    if (file_base[EI_CLASS] == ELFCLASS32) {
        load_elf32_segments(file_base, file_size, info);
    } else {
        load_elf64_segments(file_base, file_size, info);
    }
}



static rtems_task Init( rtems_task_argument ignored )
{
  (void) ignored;
  
  /** ELF Loader **/
   elf_info_t elf_info;
   memset(&elf_info, 0, sizeof(elf_info));
   
   load_elf_from_memory(magia_elf, magia_elf_size, &elf_info);
   printf("[RTEMS] MAGIA ELF: %zu bytes, entry=0x%08" PRIx64 "\n",
          magia_elf_size, elf_info.entry);
  
  /**************/

  rtems_print_printer_fprintf_putc( &rtems_test_printer );
  TEST_BEGIN();
  if ( elf_info.entry > UINT32_MAX ) {
    rtems_panic( "MAGIA entry 0x%" PRIx64 " does not fit in 32 bit", elf_info.entry );
  }
  magia_entry_buffer[0] = (uint32_t) elf_info.entry;
  printf( "[RTEMS] MAGIA entry 0x%08" PRIx32 " written at 0x%08lx\n",
          (uint32_t) elf_info.entry, MAGIA_ENTRY_BUFFER );

  printf( "[RTEMS] enabling Magia fetch\n" );
  /* the loaded segments and the entry must be visible before the fetch enable */
  __asm__ volatile ( "fence rw, rw" ::: "memory" );
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
