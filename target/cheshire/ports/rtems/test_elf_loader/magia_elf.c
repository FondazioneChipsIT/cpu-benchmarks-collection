/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * Embeds the MAGIA ELF in the RTEMS image via .incbin.
 *
 * MAGIA_ELF_PATH (absolute path) and MAGIA_ELF_SHA1 are set by the build
 * option MAGIA_ELF_PATH (spec/build/testsuites/samples/optmagiaelfpath.yml),
 * read from config.ini; with the cpu-benchmarks-collection makefiles:
 *   make rtems MAGIA_ELF=/path/to/magia.elf
 * The SHA1 of the ELF is part of the compiler flags, so this file is rebuilt
 * whenever the ELF content changes.
 *
 * Without MAGIA_ELF_PATH an empty image is built (magia_elf_size == 0) and
 * test_elf_loader panics at run time.
 */

#define STR_( x ) #x
#define STR( x ) STR_( x )

#ifndef MAGIA_ELF_PATH
/* a note, not a #warning: RTEMS builds with -Werror */
#pragma message "MAGIA_ELF_PATH not set: test_elf_loader embeds an empty MAGIA image"
#define MAGIA_ELF_INCBIN ""
#else
#define MAGIA_ELF_INCBIN "  .incbin \"" STR( MAGIA_ELF_PATH ) "\"\n"
#endif

__asm__(
  "  .section .rodata.magia_elf, \"a\", @progbits\n"
  "  .balign 8\n"
  "  .global magia_elf\n"
  "  .type   magia_elf, @object\n"
  "magia_elf:\n"
  MAGIA_ELF_INCBIN
  "magia_elf_end:\n"
  "  .size   magia_elf, magia_elf_end - magia_elf\n"
  "  .balign 8\n"
  "  .global magia_elf_size\n"
  "  .type   magia_elf_size, @object\n"
  "magia_elf_size:\n"
  "  .dword  magia_elf_end - magia_elf\n"
  "  .size   magia_elf_size, 8\n"
  "  .previous\n"
);
