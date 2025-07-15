/* Common main.c for the benchmarks

   Copyright (C) 2014 Embecosm Limited and University of Bristol
   Copyright (C) 2018-2019 Embecosm Limited

   Contributor: James Pallister <james.pallister@bristol.ac.uk>
   Contributor: Jeremy Bennett <jeremy.bennett@embecosm.com>

   This file is part of Embench and was formerly part of the Bristol/Embecosm
   Embedded Benchmark Suite.

   SPDX-License-Identifier: GPL-3.0-or-later */


#include "regs/cheshire.h"
#include "dif/clint.h"
#include "dif/uart.h"
#include "params.h"
#include "util.h"
#include "printf.h"
#include "math.h"

#include "support.h"

// Number of iterations for each benchmark
#define WARMUP_HEAT 50

int __attribute__ ((used))
main (int argc __attribute__ ((unused)),
      char *argv[] __attribute__ ((unused)))
{
  int i;
  volatile int result;
  int correct;

  initialise_benchmark ();
  warm_caches (WARMUP_HEAT);

  uint64_t start = get_mcycle();
  result = benchmark ();
  uint64_t stop = get_mcycle();

  uint64_t diff = stop - start;

  /* bmarks that use arrays will check a global array rather than int result */

  correct = verify_benchmark (result);

  return (!correct);

}				/* main () */


/*
   Local Variables:
   mode: C
   c-file-style: "gnu"
   End:
*/
