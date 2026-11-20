/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * omptarget_minimal: the subset of libomptarget's omptarget.h and
 * omptargetplugin.h needed by the MAGIA target plugin (rtl.cpp) and by its
 * callers. RTEMS has no libomptarget, so the types, return codes and the
 * __tgt_rtl_* plugin interface are declared here. Usable from C and C++.
 *
 * Copyright (C) 2026 Chips-IT
 */

#ifndef OMPTARGET_MINIMAL_H
#define OMPTARGET_MINIMAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Return codes of the __tgt_rtl_* functions (omptarget.h) */
#define OFFLOAD_SUCCESS (0)
#define OFFLOAD_FAIL    (~0)

/*
 * Device numbers of the MAGIA plugin, as HERODEV_SVM / HERODEV_MEMCPY in the
 * HERO plugin: the same MAGIA, two ways of handling the mapped data.
 *   MAGIADEV_SHMMEM  zero copy: the host buffers must lie in the L2 section
 *                    .omp_offloaded_buffer_L2 (MAGIA_OMP_SHARED, magia_api.h),
 *                    otherwise data_alloc / submit / retrieve fail;
 *   MAGIADEV_MEMCPY  copies to / from the L2 heap (device(1) in the HERO apps).
 */
#define MAGIADEV_SHMMEM (0)
#define MAGIADEV_MEMCPY (1)

/*
 * Allocation kind passed to __tgt_rtl_data_alloc (omptarget.h, same values).
 * libomptarget uses TARGET_ALLOC_DEFAULT for map/private/firstprivate and
 * omp_target_alloc; the other kinds come only from
 * llvm_omp_target_alloc_{device,host,shared}, always with hst_ptr == NULL.
 * Accepted by the MAGIA plugin:
 *   MAGIADEV_SHMMEM  TARGET_ALLOC_DEFAULT, TARGET_ALLOC_SHARED
 *   MAGIADEV_MEMCPY  TARGET_ALLOC_DEFAULT, TARGET_ALLOC_DEVICE
 */
enum TargetAllocTy {
  TARGET_ALLOC_DEVICE = 0,
  TARGET_ALLOC_HOST,
  TARGET_ALLOC_SHARED,
  TARGET_ALLOC_DEFAULT
};

/// This struct is a record of an entry point or global. For a function
/// entry point the size is expected to be zero
struct __tgt_offload_entry {
  void *addr;       // Pointer to the offload entry info (function or global)
  char *name;       // Name of the function or global
  size_t size;      // Size of the entry info (0 if it is a function)
  int32_t flags;    // Flags associated with the entry, e.g. 'link'.
  int32_t reserved; // Reserved, to be used by the runtime library.
};

/// This struct is a record of the device image information
struct __tgt_device_image {
  void *ImageStart;                         // Pointer to the target code start
  void *ImageEnd;                           // Pointer to the target code end
  struct __tgt_offload_entry *EntriesBegin; // Begin of table with all target entries
  struct __tgt_offload_entry *EntriesEnd;   // End of table (non inclusive)
};

/// This struct contains the offload entries identified by the target runtime
struct __tgt_target_table {
  struct __tgt_offload_entry *EntriesBegin; // Begin of the table with all the entries
  struct __tgt_offload_entry *EntriesEnd;   // End of the table with all the entries (non inclusive)
};

/* ---- Plugin interface (omptargetplugin.h) -------------------------------- */

/* 1 if image is a little-endian RISC-V ELF (e_machine 243), 0 otherwise */
int32_t __tgt_rtl_is_valid_binary(struct __tgt_device_image *image);

int32_t __tgt_rtl_number_of_devices(void);

/*
 * Resets the L2 heap, sets up the mailboxes and allocates the argument buffer.
 * Must be called BEFORE the MAGIA fetch enable.
 */
int32_t __tgt_rtl_init_device(int32_t device_id);

/*
 * Resolves every host entry of image in the MAGIA ELF .symtab (exact name
 * first, then OpenMP kernel suffix, e.g. "_l28"), loads the ELF in the L2 and
 * enables the MAGIA fetch. All or nothing: returns NULL, without starting
 * MAGIA, if any entry is missing. The returned table has the device address
 * of each host entry, in the same order. Only one image can be loaded.
 * Must be called AFTER __tgt_rtl_init_device (mailboxes before fetch enable).
 */
struct __tgt_target_table *__tgt_rtl_load_binary(int32_t device_id,
                                                 struct __tgt_device_image *image);

/* Sends STOP to MAGIA and releases the mailboxes */
int32_t __tgt_rtl_deinit_device(int32_t device_id);

/* Returns NULL when the L2 heap is exhausted. hst_ptr and kind are ignored. */
void *__tgt_rtl_data_alloc(int32_t device_id, int64_t size, void *hst_ptr,
                           int32_t kind);

/*
 * Releases tgt_ptr, returned by __tgt_rtl_data_alloc. The L2 heap is a bump
 * allocator: memory is not reclaimed, the pointer is only validated.
 * LLVM <= 16 signature (no kind argument), as in the HERO plugin.
 */
int32_t __tgt_rtl_data_delete(int32_t device_id, void *tgt_ptr);

int32_t __tgt_rtl_data_submit(int32_t device_id, void *tgt_ptr, void *hst_ptr,
                              int64_t size);

int32_t __tgt_rtl_data_retrieve(int32_t device_id, void *hst_ptr, void *tgt_ptr,
                                int64_t size);

/*
 * tgt_entry_ptr: device address of the kernel wrapper.
 * tgt_args[i] + tgt_offsets[i]: device address of the i-th argument.
 */
int32_t __tgt_rtl_run_target_team_region(int32_t device_id, void *tgt_entry_ptr,
                                         void **tgt_args,
                                         ptrdiff_t *tgt_offsets,
                                         int32_t arg_num, int32_t team_num,
                                         int32_t thread_limit,
                                         uint64_t loop_tripcount);

int32_t __tgt_rtl_run_target_region(int32_t device_id, void *tgt_entry_ptr,
                                    void **tgt_args, ptrdiff_t *tgt_offsets,
                                    int32_t arg_num);

#ifdef __cplusplus
}
#endif

#endif /* OMPTARGET_MINIMAL_H */
