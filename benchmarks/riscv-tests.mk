# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Alessandro Ottaviano <aottaviano@iis.ee.ethz.ch>

# List of sub-benchmarks
base_bmarks = \
	dhrystone \
	median \
	qsort \
	rsort \
	towers \
	vvadd \
	multiply \
        mt-matmul \
        mm \
        spmv

bmarks := $(base_bmarks)

# Directory paths
BENCH_COMMON_DIR := $(BENCH_DIR)/benchmarks/common
BENCH_COMMON_SRCS := # unused

# Function to resolve sources for a given benchmark name
define get-bmark-srcs
$(wildcard $(BENCH_DIR)/benchmarks/$(1)/*.c)
endef

# Include paths
BENCH_INCLUDES := \
	-I$(BENCH_COMMON_DIR) \
	-I$(BENCH_DIR)/env \
	$(addprefix -I$(BENCH_DIR)/benchmarks/, $(bmarks))
