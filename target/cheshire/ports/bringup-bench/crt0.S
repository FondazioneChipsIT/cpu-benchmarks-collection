# Copyright lowRISC contributors.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0

.section .text.init

#ifndef STACK_SIZE
#define STACK_SIZE (64*1024)
#endif

_start:
  .global _start

  .option push
  .option norelax
  la sp, _boot_stack
  .option pop

  #li t0, (64*1024)
  li t0, STACK_SIZE
  add sp, sp, t0

  /* Disable all interrupts */
  li    t0, 0x8             # MSTATUS_MIE
  csrc  mstatus, t0         # clear bits in mstatus selected by t0

  /* Route all traps/exceptions to _cva6_trap_entry in direct mode. */
  la t0, _cheshire_exc_handler
  csrw mtvec, t0

  /* Enable FP */
  li   t0, (3 << 13)       # Clear mstatus.FS
  csrc mstatus, t0
  li   t0, (3 << 13)       # Set mstatus.FS = Dirty
  csrs mstatus, t0


  /* */
  /* clear BSS */
  la x26, _bss_start
  la x27, _bss_end

  bge x26, x27, zero_loop_end

zero_loop:
  sd x0, 0(x26)
  addi x26, x26, 8
  blt x26, x27, zero_loop
zero_loop_end:

  /* CANARY: DOPO il clear BSS, PRIMA di uart_init e main */
#ifdef MEASURE_MEMORY
  la   t0, _boot_stack
  mv   t1, sp
  addi t1, t1, -256
  li   t2, 0xDEADBEEF
  slli t2, t2, 32
  li   t3, 0xDEADBEEF
  or   t2, t2, t3
canary_loop:
  bge  t0, t1, canary_done
  sd   t2, 0(t0)
  addi t0, t0, 8
  j    canary_loop
canary_done:
#endif

  .global uart_init_cheshire
  call uart_init_cheshire
  /* jump to main program entry point (argc = argv = 0) */
  addi x10, x0, 0
  addi x11, x0, 0

  jal x1, main

  /* if we return from main, call exit() */
  tail _cheshire_exit

#if 0
/* Trap vector entry for CVA6.
 * Keep this tiny and always tail-call into C to print/terminate.
 */
  .balign 16
_cva6_trap_entry:
  j _cva6_exc_handler
#endif
