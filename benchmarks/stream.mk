# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Alessandro Ottaviano <aottaviano@iis.ee.ethz.ch>

bmarks := stream

define get-bmark-srcs
$(filter-out $(BENCH_DIR)/mysecond.c, $(wildcard $(BENCH_DIR)/*.c))
endef
BENCH_INCLUDES := -I$(BENCH_DIR)
BENCH_CFLAGS   := -DCUSTOM_MYSECOND -DSTREAM_ARRAY_SIZE=1000
