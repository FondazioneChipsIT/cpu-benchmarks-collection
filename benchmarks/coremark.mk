# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Alessandro Ottaviano <aottaviano@iis.ee.ethz.ch>

bmarks := coremark

define get-bmark-srcs
$(wildcard $(BENCH_DIR)/*.c)
endef
BENCH_INCLUDES := -I$(BENCH_DIR)

# Benchmark-specific macros
ITERATIONS     ?= 5000
CLOCKS_PER_SEC ?= 200000000
BENCH_CFLAGS   := -DFLAGS_STR="\"$(PLAT_CFLAGS)\"" -DITERATIONS=$(ITERATIONS) -DCLOCKS_PER_SEC=$(CLOCKS_PER_SEC)
