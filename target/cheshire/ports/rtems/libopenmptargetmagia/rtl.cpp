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

/*
 * libopenmptargetmagia: OpenMP target plugin (libomptarget RTL) for MAGIA,
 * the RTEMS counterpart of the HERO plugin (HERO2 plugins/hero/rtl.cpp).
 * Each __tgt_rtl_* function wraps the libmagiaapi host API.
 *
 * Built as C++ without exceptions and RTTI (see libopenmptargetmagia.yml):
 * keep it free of the C++ runtime, so that C programs can link it with gcc.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems/magia/omptarget_minimal.h>
#include <rtems/magia/magia_api.h>
#include <rtems/magia_elf_loader.h>   /* ELF check, symbols, load, fetch */

#include <inttypes.h>
#include <stdio.h>

#define NUMBER_OF_DEVICES (1)

/* ELF e_machine of MAGIA (EM_RISCV), as checked by the HERO plugin */
#define MAGIA_ELF_MACHINE (243)

/*
 * Argument buffer size (words). As in HERO (rtl.cpp:41) there is only one,
 * dev_arg_buf, allocated by __tgt_rtl_init_device and reused for every
 * offload: this works because the host waits for DONE before the next region.
 */
#define ARG_BUF_SIZE 16u

/*
 * Maximum wait for each mailbox word. HERO waits without limit; here a
 * timeout keeps RTEMS from hanging if MAGIA does not start or gets stuck.
 * GVSoC simulates MAGIA much more slowly than QEMU's real time: a passing
 * test still takes ~3 s. Can be overridden via cxxflags.
 */
#ifndef OFL_MBOX_TIMEOUT_MS
#define OFL_MBOX_TIMEOUT_MS 30000u
#endif

/*
 * Maximum number of entries (target regions) of the loaded image. HERO builds
 * the device table with new/std::vector; a static one keeps rtl.cpp free of
 * the C++ runtime.
 */
#define MAX_ENTRIES 64u

static uintptr_t dev_arg_buf;

static __tgt_offload_entry device_entries[MAX_ENTRIES];
static __tgt_target_table  target_table;
static bool magia_started;   // the MAGIA fetch enable can be raised only once

static bool valid_device(int32_t device_id)
{
  if (device_id >= 0 && device_id < NUMBER_OF_DEVICES)
    return true;
  printf("[host] FAIL: invalid device id %" PRId32 "\n", device_id);
  return false;
}

// Sizes come as int64_t from libomptarget, libmagiaapi works with uint32_t
static bool valid_size(int64_t size)
{
  if (size >= 0 && size <= (int64_t)UINT32_MAX)
    return true;
  printf("[host] FAIL: invalid size %" PRId64 "\n", size);
  return false;
}

/*
 * In HERO: satisfy_requested_symbols (rtl.cpp:106). Looks up the host entry
 * in the MAGIA .symtab by exact name, as HERO does, then as OpenMP kernel
 * suffix (the RTEMS host is not built by the HERO clang, so the full name may
 * be unknown). Fills dev with the host entry and the device address.
 */
static int resolve_entry(const uint8_t *elf, size_t size,
                         const __tgt_offload_entry *host, __tgt_offload_entry *dev)
{
  magia_elf_sym_t sym;

  if (host->addr == nullptr || host->name == nullptr) {
    printf("[host] FAIL: invalid host entry\n");
    return -1;
  }
  if (magia_elf_find_symbol(elf, size, host->name, &sym) != 0 &&
      magia_elf_find_omp_kernel(elf, size, host->name, &sym) != 0) {
    printf("[host] FAIL: entry %s not found (or ambiguous) in the MAGIA ELF\n",
           host->name);
    return -1;
  }
  // Only functions (size 0), inside the L2: their address is a mailbox word
  if (host->size != 0 || sym.value < MAGIA_L2_START || sym.value > MAGIA_L2_END) {
    printf("[host] FAIL: entry %s at 0x%08" PRIx64 " is not a MAGIA kernel\n",
           host->name, sym.value);
    return -1;
  }
  *dev = *host;
  dev->addr = (void *)(uintptr_t)sym.value;
  printf("[host]   %s -> 0x%08" PRIx64 " %s\n", host->name, sym.value, sym.name);
  return 0;
}

#ifdef __cplusplus
extern "C" {
#endif

int32_t __tgt_rtl_is_valid_binary(__tgt_device_image *image) {

  uintptr_t start, end;

  if (image == nullptr)
    return 0;
  start = (uintptr_t)image->ImageStart;
  end   = (uintptr_t)image->ImageEnd;
  if (start == 0 || end <= start)
    return 0;

  return magia_elf_check_machine((const uint8_t *)start, (size_t)(end - start),
                                 MAGIA_ELF_MACHINE);
}

int32_t __tgt_rtl_number_of_devices(void) {

  return NUMBER_OF_DEVICES;
}

int32_t __tgt_rtl_init_device(int32_t device_id) {

  if (!valid_device(device_id))
    return OFFLOAD_FAIL;

  magia_dev_init();

  dev_arg_buf = data_alloc(ARG_BUF_SIZE * sizeof(uint64_t));
  if (dev_arg_buf == 0)
    return OFFLOAD_FAIL;

  return OFFLOAD_SUCCESS;
}

__tgt_target_table *__tgt_rtl_load_binary(int32_t device_id,
                                          __tgt_device_image *image) {

  elf_info_t info = {};

  if (!valid_device(device_id))
    return nullptr;
  if (!__tgt_rtl_is_valid_binary(image)) {
    printf("[host] FAIL: the image is not a RISC-V ELF\n");
    return nullptr;
  }
  if (dev_arg_buf == 0) {        // contract: mailboxes BEFORE the fetch enable
    printf("[host] FAIL: load_binary before init_device\n");
    return nullptr;
  }
  if (magia_started) {           // one MAGIA, one ELF
    printf("[host] FAIL: MAGIA already running\n");
    return nullptr;
  }

  const uint8_t *elf = (const uint8_t *)image->ImageStart;
  size_t size = (size_t)((uintptr_t)image->ImageEnd - (uintptr_t)image->ImageStart);
  size_t n = (size_t)(image->EntriesEnd - image->EntriesBegin);

  if (image->EntriesBegin == nullptr || image->EntriesEnd < image->EntriesBegin ||
      n > MAX_ENTRIES) {
    printf("[host] FAIL: invalid entry table (%zu entries, maximum %u)\n",
           n, MAX_ENTRIES);
    return nullptr;
  }

  // 1. Resolve everything first: all or nothing, MAGIA not touched yet.
  //    Every entry is looked up, so that every missing one gets reported.
  printf("[host] OpenMP kernels in the MAGIA .symtab:\n");
  if (magia_elf_foreach_symbol(elf, size, print_omp_kernel, nullptr) < 0) {
    // In HERO: "Symbols section not found" -> return false
    printf("[host] FAIL: no .symtab (MAGIA ELF stripped?)\n");
    return nullptr;
  }
  printf("[host] resolving %zu entries:\n", n);
  int missing = 0;
  for (size_t i = 0; i < n; i++)
    if (resolve_entry(elf, size, &image->EntriesBegin[i], &device_entries[i]))
      missing++;
  if (missing)
    return nullptr;

  // 2. Load and start. In HERO: map_to_mem + hero_dev_exe_start.
  //    load_elf_from_memory() calls rtems_panic() on a malformed image.
  load_elf_from_memory(elf, size, &info);
  set_entry_point_and_enable_fetch(&info);
  magia_started = true;

  // 3. Device table, same order as the host entries
  target_table.EntriesBegin = device_entries;
  target_table.EntriesEnd   = device_entries + n;
  return &target_table;
}

int32_t __tgt_rtl_deinit_device(int32_t device_id) {

  if (!valid_device(device_id))
    return OFFLOAD_FAIL;

  dev_arg_buf = 0;
  magia_started = false;
  return magia_dev_stop(OFL_MBOX_TIMEOUT_MS) == 0 ? OFFLOAD_SUCCESS : OFFLOAD_FAIL;
}

void *__tgt_rtl_data_alloc(int32_t device_id, int64_t size, void *hst_ptr, int32_t kind) {

  (void)hst_ptr;
  (void)kind;   // only one kind of memory: the L2 heap

  if (!valid_device(device_id) || !valid_size(size))
    return nullptr;

  return (void *)data_alloc((uint32_t)size);   // 0 (heap exhausted) -> nullptr
}

int32_t __tgt_rtl_data_delete(int32_t device_id, void *tgt_ptr) {

  if (!valid_device(device_id))
    return OFFLOAD_FAIL;

  return data_free((uintptr_t)tgt_ptr) == 0 ? OFFLOAD_SUCCESS : OFFLOAD_FAIL;
}

int32_t __tgt_rtl_data_submit(int32_t device_id, void *tgt_ptr, void *hst_ptr,
                              int64_t size) {

  if (!valid_device(device_id) || !valid_size(size))
    return OFFLOAD_FAIL;

  data_submit((uintptr_t)tgt_ptr, hst_ptr, (uint32_t)size);
  return OFFLOAD_SUCCESS;
}

int32_t __tgt_rtl_data_retrieve(int32_t device_id, void *hst_ptr, void *tgt_ptr,
                                int64_t size) {

  if (!valid_device(device_id) || !valid_size(size))
    return OFFLOAD_FAIL;

  data_retrieve(hst_ptr, (uintptr_t)tgt_ptr, (uint32_t)size);
  return OFFLOAD_SUCCESS;
}

/*
 * In HERO: rtl.cpp:457-519. Copies the argument addresses into dev_arg_buf,
 * sends START, kernel, argbuf, no. of miss handlers, then WAITS for DONE and
 * the 6 words that follow it. team_num, thread_limit and loop_tripcount are
 * ignored, as in HERO.
 */
int32_t __tgt_rtl_run_target_team_region(int32_t device_id, void *tgt_entry_ptr,
                                         void **tgt_args,
                                         ptrdiff_t *tgt_offsets,
                                         int32_t arg_num, int32_t team_num,
                                         int32_t thread_limit,
                                         uint64_t loop_tripcount) {

  uint64_t args[ARG_BUF_SIZE];
  uint32_t done[MBOX_DONE_WORDS];
  // MAGIA is a 32-bit device: its addresses fit in one mailbox word
  uint32_t kernel = (uint32_t)(uintptr_t)tgt_entry_ptr;

  (void)team_num;
  (void)thread_limit;
  (void)loop_tripcount;

  if (!valid_device(device_id))
    return OFFLOAD_FAIL;
  if (dev_arg_buf == 0) {
    printf("[host] FAIL: device %" PRId32 " not initialized\n", device_id);
    return OFFLOAD_FAIL;
  }
  if (arg_num < 0 || (uint32_t)arg_num > ARG_BUF_SIZE) {
    printf("[host] FAIL: %" PRId32 " arguments, maximum %u\n", arg_num, ARG_BUF_SIZE);
    return OFFLOAD_FAIL;
  }

  // As in HERO: the device address of each argument is base + offset
  for (int32_t i = 0; i < arg_num; i++)
    args[i] = (uint64_t)((uintptr_t)tgt_args[i] + tgt_offsets[i]);
  data_submit(dev_arg_buf, args, (uint32_t)arg_num * sizeof(uint64_t));

  if (host_mbox_write(MBOX_DEVICE_START, OFL_MBOX_TIMEOUT_MS) ||
      host_mbox_write(kernel, OFL_MBOX_TIMEOUT_MS) ||                  // wrapper address
      host_mbox_write((uint32_t)dev_arg_buf, OFL_MBOX_TIMEOUT_MS) ||   // argument buffer address
      host_mbox_write(0, OFL_MBOX_TIMEOUT_MS))                         // no. of miss handlers (unused)
    return OFFLOAD_FAIL;

  // HERO discards words other than DONE only for MBOX_DEVICE_PRINT, which the
  // MAGIA device does not use: any other word is a protocol error.
  if (host_mbox_read(&done[0], OFL_MBOX_TIMEOUT_MS))
    return OFFLOAD_FAIL;
  if (done[0] != MBOX_DEVICE_DONE) {
    printf("[host] FAIL: expected DONE (0x%02x), read 0x%08" PRIx32 "\n",
           MBOX_DEVICE_DONE, done[0]);
    return OFFLOAD_FAIL;
  }
  for (uint32_t i = 1; i < MBOX_DONE_WORDS; i++) {      // cycles, dma + 4 counters
    if (host_mbox_read(&done[i], OFL_MBOX_TIMEOUT_MS)) {
      printf("[host] FAIL: incomplete DONE\n");
      return OFFLOAD_FAIL;
    }
  }
  // Data written by the kernel is read only after DONE has been seen
  __asm__ volatile("fence r,r" ::: "memory");
  printf("[host] region 0x%08" PRIx32 " DONE, kernel cycles = %" PRIu32 "\n",
         kernel, done[1]);
  return OFFLOAD_SUCCESS;
}

int32_t __tgt_rtl_run_target_region(int32_t device_id, void *tgt_entry_ptr,
                                    void **tgt_args, ptrdiff_t *tgt_offsets,
                                    int32_t arg_num) {

  // use one team and the default number of threads.
  const int32_t team_num = 1;
  const int32_t thread_limit = 0;
  return __tgt_rtl_run_target_team_region(device_id, tgt_entry_ptr, tgt_args,
                                          tgt_offsets, arg_num, team_num,
                                          thread_limit, 0);
}

#ifdef __cplusplus
}
#endif
