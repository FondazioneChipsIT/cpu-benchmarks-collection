#ifndef STACK_SIZE
#define STACK_SIZE (16*1024)
#endif

.section .text.init
_start:
  .global _start
  .option push
  .option norelax
  la sp, _boot_stack
  .option pop
  li t0, STACK_SIZE
  add sp, sp, t0

  li    t0, 0x8
  csrc  mstatus, t0

  la t0, _cheshire_exc_handler
  csrw mtvec, t0

  li   t0, (3 << 13)
  csrc mstatus, t0
  li   t0, (3 << 13)
  csrs mstatus, t0

  /* Clear BSS */
  la x26, __bss_start
  la x27, __bss_end
  bge x26, x27, zero_loop_end
zero_loop:
  sd x0, 0(x26)
  addi x26, x26, 8
  blt x26, x27, zero_loop
zero_loop_end:

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

  addi x10, x0, 0
  addi x11, x0, 0
  jal x1, main
  tail _cheshire_exit