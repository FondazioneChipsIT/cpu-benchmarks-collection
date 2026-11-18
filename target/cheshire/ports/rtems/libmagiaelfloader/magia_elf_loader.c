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

static inline void write_fence( void )
{
  __asm__ volatile ( "fence rw, w" ::: "memory" );
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
    
    /* make visible the loaded segments */
    write_fence();
    
    printf("[ELF  ] MAGIA ELF: %zu bytes, entry=0x%08" PRIx64 "\n",
           file_size, info->entry);
}

void set_entry_point_and_enable_fetch(const elf_info_t *info)
{
    if ( info->entry > UINT32_MAX ) {
        rtems_panic( "MAGIA entry 0x%" PRIx64 " does not fit in 32 bit", info->entry );
    }
      
    magia_entry_buffer[0] = (uint32_t) info->entry;
    printf( "[FETCH] MAGIA entry 0x%08" PRIx32 " written at 0x%08lx\n",
            (uint32_t) info->entry, MAGIA_ENTRY_BUFFER );
            
    /* make visible entry*/
    write_fence();

    printf( "[FETCH] enabling Magia fetch\n" );

    fetch_enable_buffer[0] = FETCH_ENABLE_PATTERN;
      
    printf( "[FETCH] enabled Magia fetch\n" );
    write_fence();
  
}
/* ------------------------------------------------------------------------ */
/* Symbol table                                                              */
/* ------------------------------------------------------------------------ */

/*
 * Section header fields needed to walk .symtab, independent of the ELF class.
 * The class was already validated by check_elf_ident().
 */
typedef struct {
    uint32_t type;
    uint32_t link;
    uint64_t offset;
    uint64_t size;
    uint64_t entsize;
} shdr_t;

static void check_image(const uint8_t *file_base, size_t file_size)
{
    if (file_size < EI_NIDENT) {
        rtems_panic("ELF image too small");
    }

    check_elf_ident(file_base);
}

static uint16_t elf_shnum(const uint8_t *file_base, size_t file_size,
                          uint64_t *shoff)
{
    uint16_t shnum, shentsize;
    size_t shdr_size;

    if (file_base[EI_CLASS] == ELFCLASS32) {
        const Elf32_Ehdr *ehdr = (const Elf32_Ehdr *)file_base;

        if (file_size < sizeof(*ehdr)) {
            rtems_panic("ELF32 header exceeds file size");
        }
        *shoff = ehdr->e_shoff;
        shnum = ehdr->e_shnum;
        shentsize = ehdr->e_shentsize;
        shdr_size = sizeof(Elf32_Shdr);
    } else {
        const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)file_base;

        if (file_size < sizeof(*ehdr)) {
            rtems_panic("ELF64 header exceeds file size");
        }
        *shoff = ehdr->e_shoff;
        shnum = ehdr->e_shnum;
        shentsize = ehdr->e_shentsize;
        shdr_size = sizeof(Elf64_Shdr);
    }

    if (shnum == 0) {
        return 0;
    }

    if (shentsize != shdr_size) {
        rtems_panic("ELF unexpected section header size %u", (unsigned)shentsize);
    }

    if (!range_ok(*shoff, (uint64_t)shnum * shdr_size, file_size)) {
        rtems_panic("ELF section header table exceeds file size");
    }

    return shnum;
}

static void get_shdr(const uint8_t *file_base, uint64_t shoff, unsigned i,
                     shdr_t *out)
{
    if (file_base[EI_CLASS] == ELFCLASS32) {
        const Elf32_Shdr *sh = (const Elf32_Shdr *)(file_base + shoff) + i;

        out->type = sh->sh_type;
        out->link = sh->sh_link;
        out->offset = sh->sh_offset;
        out->size = sh->sh_size;
        out->entsize = sh->sh_entsize;
    } else {
        const Elf64_Shdr *sh = (const Elf64_Shdr *)(file_base + shoff) + i;

        out->type = sh->sh_type;
        out->link = sh->sh_link;
        out->offset = sh->sh_offset;
        out->size = sh->sh_size;
        out->entsize = sh->sh_entsize;
    }
}

int magia_elf_foreach_symbol(const uint8_t *file_base,
                             size_t file_size,
                             magia_elf_sym_visitor_t visit,
                             void *arg)
{
    uint64_t shoff;
    uint16_t shnum;
    shdr_t symtab, strtab;
    unsigned i;
    bool is_32;
    size_t sym_size;
    uint64_t count;
    const char *strings;

    check_image(file_base, file_size);
    is_32 = file_base[EI_CLASS] == ELFCLASS32;
    sym_size = is_32 ? sizeof(Elf32_Sym) : sizeof(Elf64_Sym);

    shnum = elf_shnum(file_base, file_size, &shoff);

    /* Find .symtab, as satisfy_requested_symbols (HERO plugins/hero/rtl.cpp) */
    for (i = 0; i < shnum; ++i) {
        get_shdr(file_base, shoff, i, &symtab);
        if (symtab.type == SHT_SYMTAB) {
            break;
        }
    }

    if (i == shnum) {
        return -1;
    }

    if (symtab.entsize != sym_size) {
        rtems_panic("ELF unexpected symbol size %" PRIu64, symtab.entsize);
    }

    if (!range_ok(symtab.offset, symtab.size, file_size)) {
        rtems_panic("ELF .symtab exceeds file size");
    }

    if (symtab.link == 0 || symtab.link >= shnum) {
        rtems_panic("ELF .symtab has an invalid string table index %" PRIu32,
                    symtab.link);
    }

    get_shdr(file_base, shoff, symtab.link, &strtab);

    if (strtab.type != SHT_STRTAB || strtab.size == 0 ||
        !range_ok(strtab.offset, strtab.size, file_size)) {
        rtems_panic("ELF invalid .symtab string table");
    }

    strings = (const char *)(file_base + strtab.offset);

    if (strings[strtab.size - 1] != '\0') {
        rtems_panic("ELF string table is not NUL terminated");
    }

    count = symtab.size / sym_size;

    /* Entry 0 is the reserved null symbol */
    for (uint64_t n = 1; n < count; ++n) {
        const uint8_t *p = file_base + symtab.offset + n * sym_size;
        magia_elf_sym_t sym;
        uint32_t st_name;
        unsigned char st_info;
        int rv;

        if (is_32) {
            const Elf32_Sym *es = (const Elf32_Sym *)p;

            st_name = es->st_name;
            st_info = es->st_info;
            sym.value = es->st_value;
            sym.size = es->st_size;
            sym.shndx = es->st_shndx;
        } else {
            const Elf64_Sym *es = (const Elf64_Sym *)p;

            st_name = es->st_name;
            st_info = es->st_info;
            sym.value = es->st_value;
            sym.size = es->st_size;
            sym.shndx = es->st_shndx;
        }

        if (st_name == 0) {
            continue;
        }

        if (st_name >= strtab.size) {
            rtems_panic("ELF symbol %" PRIu64 " name outside the string table",
                        n);
        }

        sym.name = strings + st_name;
        /* ELF32_ST_* and ELF64_ST_* are the same macros */
        sym.type = ELF32_ST_TYPE(st_info);
        sym.bind = ELF32_ST_BIND(st_info);

        rv = (*visit)(&sym, arg);
        if (rv != 0) {
            return rv;
        }
    }

    return 0;
}

typedef struct {
    const char *name;
    magia_elf_sym_t *sym;
} find_by_name_t;

static int visit_find_by_name(const magia_elf_sym_t *sym, void *arg)
{
    find_by_name_t *ctx = arg;

    if (sym->shndx != SHN_UNDEF && strcmp(sym->name, ctx->name) == 0) {
        *ctx->sym = *sym;
        return 1;
    }

    return 0;
}

int magia_elf_find_symbol(const uint8_t *file_base,
                          size_t file_size,
                          const char *name,
                          magia_elf_sym_t *sym)
{
    find_by_name_t ctx = { name, sym };

    return magia_elf_foreach_symbol(file_base, file_size,
                                    visit_find_by_name, &ctx) == 1 ? 0 : -1;
}

typedef struct {
    const char *suffix;
    size_t suffix_len;
    magia_elf_sym_t *sym;
    unsigned matches;
} find_kernel_t;

static int visit_find_kernel(const magia_elf_sym_t *sym, void *arg)
{
    find_kernel_t *ctx = arg;
    size_t prefix_len = sizeof(MAGIA_OMP_KERNEL_PREFIX) - 1;
    size_t len;

    if (sym->type != STT_FUNC || sym->shndx == SHN_UNDEF ||
        strncmp(sym->name, MAGIA_OMP_KERNEL_PREFIX, prefix_len) != 0) {
        return 0;
    }

    len = strlen(sym->name);
    if (len < prefix_len + ctx->suffix_len ||
        strcmp(sym->name + len - ctx->suffix_len, ctx->suffix) != 0) {
        return 0;
    }

    if (ctx->matches == 0) {
        *ctx->sym = *sym;
    } else {
        printf("[ELF  ] OpenMP kernel suffix \"%s\" is ambiguous: %s, %s\n",
               ctx->suffix, ctx->sym->name, sym->name);
    }
    ++ctx->matches;

    return 0;
}

int magia_elf_find_omp_kernel(const uint8_t *file_base,
                              size_t file_size,
                              const char *suffix,
                              magia_elf_sym_t *sym)
{
    find_kernel_t ctx = { suffix, strlen(suffix), sym, 0 };

    if (magia_elf_foreach_symbol(file_base, file_size,
                                 visit_find_kernel, &ctx) < 0) {
        return -1;
    }

    if (ctx.matches == 0) {
        return -1;
    }

    return ctx.matches == 1 ? 0 : -2;
}
