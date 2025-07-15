// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// Nicole Narr <narrn@student.ethz.ch>
// Christopher Reinwardt <creinwar@student.ethz.ch>
// Paul Scheffler <paulsc@iis.ee.ethz.ch>

#include <stdarg.h>

#include "regs/cheshire.h"
#include "dif/clint.h"
#include "dif/uart.h"
#include "params.h"
#include "util.h"
//#include "printf.h"

void printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_end(args);
}

// Declare the renamed STREAM benchmark entry point
int stream_main();

// Replace mysecond with your version
static uint64_t core_freq = 0;

double mysecond() {
    if (core_freq == 0) {
        uint32_t rtc_freq = *reg32(&__base_regs, CHESHIRE_RTC_FREQ_REG_OFFSET);
        core_freq = clint_get_core_freq(rtc_freq, 2500);
    }
    uint64_t cycles = get_mcycle();
    return (double)cycles / core_freq;
}

// Your custom main or entry point
int main() {
    // Setup Cheshire-specific runtime if needed
    return stream_main();
}
