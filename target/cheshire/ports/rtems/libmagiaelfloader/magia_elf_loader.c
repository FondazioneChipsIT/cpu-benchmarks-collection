/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * magia_elf_loader: loads an ELF image from memory into the MAGIA L2.
 *
 * Copyright (C) 2026 Chips-IT
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems/magia_elf_loader.h>

#include <rtems.h>

#include <elf.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

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

void load_elf_from_memory(const uint8_t *file_base,
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
