/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * magia_elf_loader: static library that loads a RISC-V ELF (32 or 64 bit,
 * little-endian) from memory into the MAGIA L2 window seen by RTEMS.
 *
 * Copyright (C) 2026 Chips-IT
 */

#ifndef MAGIA_ELF_LOADER_H
#define MAGIA_ELF_LOADER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 
 *  Must match the fetch_en_pattern expected by GVSoC 
 * (gvsoc pulp/chips/magia_common/fetch_enable_module/fetch_enable_module_v2.py)
*/
#define FETCH_ENABLE_PATTERN 0x1A2B3C4E
                             
/*
 * MAGIA L2 window as seen by RTEMS (gvsoc pulp/chips/magia_v2/arch.py:
 * L2_ADDR_START, L2_SIZE). GVSoC backs it with the QEMU RAM (UMA), so the
 * segments are copied directly at their physical address.
 */
#define MAGIA_L2_START 0xC0000000UL
#define MAGIA_L2_END   ( MAGIA_L2_START + 0x0CFEFFFFUL )

/*
 * MAGIA fetch enable, GVSoC polls this address for the FETCH_ENABLE_PATTERN,
 * when found, it raise the fetch enable for Magia. 
 * (gvsoc pulp/chips/magia_v4/arch.py, FETCH_ENABLE_ADDR_START): must be written
 * after the entry point word.
 */
#define FETCH_ENABLE_BUFFER 0xCCFF0400UL
#define fetch_enable_buffer ((volatile uint32_t *) FETCH_ENABLE_BUFFER)

/*
 * MAGIA entry point, read by GVSoC when it finds the fetch enable pattern
 * (gvsoc pulp/chips/magia_v4/arch.py, ENTRY_ADDR_START): must be written
 * before the fetch enable word.
 */
#define MAGIA_ENTRY_BUFFER 0xCCFF0408UL
#define magia_entry_buffer ((volatile uint32_t *) MAGIA_ENTRY_BUFFER)

typedef struct {
    uint64_t entry;
    int is_32;
} elf_info_t;

/*
 * Validates the ELF image [file_base, file_base + file_size) and copies its
 * PT_LOAD segments at their physical address (inside the MAGIA L2), zeroing
 * the .bss part and checking the copy by readback. Fills info with the entry
 * point and the ELF class. Calls rtems_panic() on any error.
 */
void load_elf_from_memory(const uint8_t *file_base,
                          size_t file_size,
                          elf_info_t *info);
                          
void set_entry_point_and_enable_fetch(const elf_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* MAGIA_ELF_LOADER_H */
