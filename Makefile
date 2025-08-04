# Copyright 2025 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Alessandro Ottaviano <aottaviano@iis.ee.ethz.ch>

ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

# Toolchain binaries
PLATFORM_CROSS_COMPILE ?= riscv64-unknown-elf-
CC      := $(PLATFORM_CROSS_COMPILE)-gcc
AR      := $(PLATFORM_CROSS_COMPILE)-ar
OBJCOPY := $(PLATFORM_CROSS_COMPILE)-objcopy
OBJDUMP := $(PLATFORM_CROSS_COMPILE)-objdump

# Platform and benchmark, these are just some defaults
PLATFORM      ?= cheshire
BENCHMARK     ?= embench-iot

# Per-platform config
include $(ROOT)/target/$(PLATFORM)/target.mk

# Benchmark-specific settings
BENCH_DIR := $(ROOT)/benchmarks/deps/$(BENCHMARK)
include $(ROOT)/benchmarks/$(BENCHMARK).mk

# Platform-specific additions
BENCH_PLAT_DIR      := $(ROOT)/target/$(PLATFORM)/ports/$(BENCHMARK)
BENCH_PLAT_SRCS     := $(wildcard $(BENCH_PLAT_DIR)/*.c)
BENCH_PLAT_INCLUDES := -I$(BENCH_PLAT_DIR)/include

# Output directory and files
BUILD_DIR := build/$(PLATFORM)/$(BENCHMARK)

SUB_BMARK_ELFS  := $(foreach bmark,$(bmarks),$(BUILD_DIR)/$(bmark)/$(bmark).$(PLAT_MEMORY).elf)
SUB_BMARK_DUMPS := $(foreach bmark,$(bmarks),$(BUILD_DIR)/$(bmark)/$(bmark).$(PLAT_MEMORY).dump)

# Default target: build all dumps
all: $(SUB_BMARK_DUMPS)

$(info SUB_BMARK_ELFS = $(SUB_BMARK_ELFS))

.PRECIOUS: $(SUB_BMARK_ELFS)

# Explicit rules for each benchmark
define GEN_BENCHMARK_RULES
$(BUILD_DIR)/$(1)/$(1).$(PLAT_MEMORY).elf:
	@mkdir -p $(BUILD_DIR)/$(1)
	$$(CC) $$(PLAT_CFLAGS) $$(BENCH_CFLAGS) \
	       $$(BENCH_INCLUDES) $$(BENCH_PLAT_INCLUDES) $$(PLAT_INCLUDES) \
	       -T$$(PLAT_LD_SCRIPT) \
	       -o $$@ \
	       $$(BENCH_PLAT_SRCS) $$(BENCH_COMMON_SRCS) $$(call get-bmark-srcs,$(1)) \
	       $$(PLAT_LDFLAGS) $$(PLAT_LIBS)

$(BUILD_DIR)/$(1)/$(1).$(PLAT_MEMORY).dump: $(BUILD_DIR)/$(1)/$(1).$(PLAT_MEMORY).elf
	$$(OBJDUMP) -d -S $$< > $$@
endef

# Apply the above template to each benchmark
$(foreach bmark,$(bmarks),$(eval $(call GEN_BENCHMARK_RULES,$(bmark))))

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
