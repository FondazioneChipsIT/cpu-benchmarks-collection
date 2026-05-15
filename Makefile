# Makefile (root-level)
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

# Toolchain binaries
PLATFORM_CROSS_COMPILE ?= riscv64-unknown-elf-
CC      := $(PLATFORM_CROSS_COMPILE)-gcc
AR      := $(PLATFORM_CROSS_COMPILE)-ar
OBJCOPY := $(PLATFORM_CROSS_COMPILE)-objcopy
OBJDUMP := $(PLATFORM_CROSS_COMPILE)-objdump

# Platform and benchmark, these are just some defaults
PLATFORM     ?= cheshire
BENCHMARK    ?= embench-iot
ALL_BENCHMARKS := $(notdir $(wildcard $(ROOT)/benchmarks/deps/*)) rtems-bsp

# Per-platform config
include $(ROOT)/target/$(PLATFORM)/target.mk

# RTEMS + TOOLCHAIN INTEGRATION

# Directory porting RTEMS
RTEMS_PORTS_DIR := $(ROOT)/target/cheshire/ports/rtems

# Directory toolchain 
RTEMS_TOOLCHAIN_INSTALL_DIR := $(RTEMS_PORTS_DIR)/toolchain/compiler-strict

# Directory RTEMS vanilla 
RTEMS_DIR := $(ROOT)/benchmarks/deps/cheshire/rtems

# Build toolchain RTEMS
.PHONY: rtems-toolchain
rtems-toolchain:
	@echo "[TOOLCHAIN] Building strict-align toolchain..."
	@$(RTEMS_PORTS_DIR)/toolchain/build-toolchain.sh \
		$(RTEMS_PORTS_DIR)/toolchain/rsb-strict \
		$(RTEMS_TOOLCHAIN_INSTALL_DIR)

# Build BSP RTEMS
.PHONY: rtems
rtems:
	$(MAKE) -f $(ROOT)/benchmarks/rtems-bsp.mk
        RTEMS_DIR=$(RTEMS_DIR) \
        RTEMS_TOOLS=$(RTEMS_TOOLCHAIN_INSTALL_DIR)

# Benchmark-specific settings
BENCH_DIR := $(ROOT)/benchmarks/deps/$(BENCHMARK)
include $(ROOT)/benchmarks/$(BENCHMARK).mk

# Platform-specific additions
BENCH_PLAT_DIR      := $(ROOT)/target/$(PLATFORM)/ports/$(BENCHMARK)
BENCH_PLAT_SRCS     := $(wildcard $(BENCH_PLAT_DIR)/*.c)
BENCH_PLAT_INCLUDES := -I$(BENCH_PLAT_DIR)/include

# Output directory and files
BUILD_DIR := build/$(PLATFORM)/$(BENCHMARK)

SUB_BMARK_ELFS  := $(foreach mem,$(PLAT_MEMORY),\
                     $(foreach bmark,$(bmarks),\
                       $(BUILD_DIR)/$(bmark)/$(bmark).$(mem).elf\
                     )\
                   )
SUB_BMARK_DUMPS := $(foreach mem,$(PLAT_MEMORY),\
                     $(foreach bmark,$(bmarks),\
                       $(BUILD_DIR)/$(bmark)/$(bmark).$(mem).dump\
                     )\
                   )

# Default target: build all dumps
build: $(SUB_BMARK_DUMPS)

$(info SUB_BMARK_ELFS = $(SUB_BMARK_ELFS))

.PRECIOUS: $(SUB_BMARK_ELFS)

# Explicit rules for each benchmark
define GEN_BENCHMARK_RULES
$(BUILD_DIR)/$(1)/$(1).$(2).elf:
	@mkdir -p $(BUILD_DIR)/$(1)
	$$(CC) $$(PLAT_CFLAGS) $$(BENCH_CFLAGS) \
	       $$(BENCH_INCLUDES) $$(BENCH_PLAT_INCLUDES) $$(PLAT_INCLUDES) \
	       -T$$(PLAT_LD_DIR)/$(2).ld \
	       -o $$@ \
	       $$(BENCH_PLAT_SRCS) $$(BENCH_COMMON_SRCS) $$(call get-bmark-srcs,$(1)) \
	       $$(PLAT_LDFLAGS) $$(PLAT_LIBS)

$(BUILD_DIR)/$(1)/$(1).$(2).dump: $(BUILD_DIR)/$(1)/$(1).$(2).elf
	$$(OBJDUMP) -d -S $$< > $$@
endef

# Apply the above template to each benchmark
$(foreach mem,$(PLAT_MEMORY),\
	$(foreach bmark,$(bmarks),\
		$(eval $(call GEN_BENCHMARK_RULES,$(bmark),$(mem)))\
	)\
)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# Build all available benchmarks
.PHONY: all_benchmarks
build-all:
	@$(foreach bmark,$(ALL_BENCHMARKS), \
		$(MAKE) build BENCHMARK=$(bmark) PLATFORM=$(PLATFORM) || exit $$?; \
	)
