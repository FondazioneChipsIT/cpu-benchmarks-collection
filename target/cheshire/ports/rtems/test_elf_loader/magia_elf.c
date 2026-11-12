/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * Embeds the MAGIA ELF (MAGIA_ELF_PATH, absolute path set in the build spec)
 * in the RTEMS image via .incbin.
 *
 * NOTE: waf does not track the dependency on the embedded file: after
 * rebuilding the MAGIA binary, touch this file and rebuild RTEMS.
 */

#ifndef MAGIA_ELF_PATH
#error "Define MAGIA_ELF_PATH to the MAGIA ELF to embed"
#endif

#define STR_( x ) #x
#define STR( x ) STR_( x )

__asm__(
  "  .section .rodata.magia_elf, \"a\", @progbits\n"
  "  .balign 8\n"
  "  .global magia_elf\n"
  "  .type   magia_elf, @object\n"
  "magia_elf:\n"
  "  .incbin \"" STR( MAGIA_ELF_PATH ) "\"\n"
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
