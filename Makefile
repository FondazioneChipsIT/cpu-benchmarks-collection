# ============================================================
# CPU-Benchmarks-Collection Makefile
# ============================================================
ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
.DEFAULT_GOAL := all

# ------------------------------------------------------------
# Toolchain
# ------------------------------------------------------------
PLATFORM_CROSS_COMPILE ?= riscv64-unknown-elf
CC      := $(PLATFORM_CROSS_COMPILE)-gcc
AR      := $(PLATFORM_CROSS_COMPILE)-ar
OBJCOPY := $(PLATFORM_CROSS_COMPILE)-objcopy
OBJDUMP := $(PLATFORM_CROSS_COMPILE)-objdump

# ------------------------------------------------------------
# Platform & benchmark selection
# ------------------------------------------------------------
PLATFORM     ?= cheshire
BENCHMARK    ?= embench-iot

ALL_BENCHMARKS := $(notdir $(wildcard $(ROOT)/benchmarks/deps/*)) rtems-bsp

# ------------------------------------------------------------
# RTEMS paths
# -----------------------------------------------------------
RTEMS_PORTS_DIR := $(ROOT)/target/$(PLATFORM)/ports/rtems
RTEMS_TOOLCHAIN_INSTALL_DIR := $(RTEMS_PORTS_DIR)/toolchain/compiler-strict 
RTEMS_DIR := $(ROOT)/benchmarks/deps/rtems

# ------------------------------------------------------------
# Per-platform configuration
# ------------------------------------------------------------
-include $(ROOT)/target/$(PLATFORM)/target.mk

# ============================================================
# Benchmark-specific settings & Build Engine
# ============================================================
BENCH_DIR := $(ROOT)/benchmarks/deps/$(BENCHMARK)
-include $(ROOT)/benchmarks/$(BENCHMARK).mk

# ------------------------------------------------------------
# Optional single-benchmark / single-memory filter
#   make BENCHMARK=bringup-bench TEST=aes
#   make BENCHMARK=bringup-bench MEM=dram
# Filtering is done inside each fragment (TEST); MEM filters
# PLAT_MEMORY here so GEN_BENCHMARK_RULES sees the right set.
# ------------------------------------------------------------
ifdef MEM
PLAT_MEMORY := $(filter $(MEM),$(PLAT_MEMORY))
endif

# Default implementation
ifndef bmarks_for_mem
bmarks_for_mem = $(bmarks)
endif

# ------------------------------------------------------------
# Platform-specific port additions (sources & includes)
# ------------------------------------------------------------
BENCH_PLAT_DIR      := $(ROOT)/target/$(PLATFORM)/ports/$(BENCHMARK)
BENCH_PLAT_SRCS     := $(wildcard $(BENCH_PLAT_DIR)/*.c)
BENCH_PLAT_INCLUDES := -I$(BENCH_PLAT_DIR)/include

BUILD_DIR ?= build/$(PLATFORM)/$(BENCHMARK)


# Helper function for memory flags (SPM vs DRAM)
get_mem_flags = $(if $(and $(filter spm,$(1)),$(STACK_SIZE_SPM)),\
                  -DSTACK_SIZE=$(STACK_SIZE_SPM) -DMAX_HEAP=$(HEAP_SIZE_SPM),\
                $(if $(and $(filter dram,$(1)),$(STACK_SIZE_DRAM)),\
                  -DSTACK_SIZE=$(STACK_SIZE_DRAM) -DMAX_HEAP=$(HEAP_SIZE_DRAM),\
                ))

# ------------------------------------------------------------
# Target lists (elf + disassembly)
# ------------------------------------------------------------

SUB_BMARK_ELFS  := $(foreach mem,$(PLAT_MEMORY),\
                     $(foreach bmark,$(call bmarks_for_mem,$(mem)),\
                       $(BUILD_DIR)/$(bmark)/$(bmark).$(mem).elf\
                     )\
                   )

SUB_BMARK_DUMPS := $(foreach mem,$(PLAT_MEMORY),\
                     $(foreach bmark,$(call bmarks_for_mem,$(mem)),\
                       $(BUILD_DIR)/$(bmark)/$(bmark).$(mem).dump\
                     )\
                   )

# ------------------------------------------------------------
# Top-level targets
# ------------------------------------------------------------
.PHONY: all build
all: build
build: $(SUB_BMARK_DUMPS)

# To be added
#$(info SUB_BMARK_ELFS = $(SUB_BMARK_ELFS))

.PRECIOUS: $(SUB_BMARK_ELFS)

# ------------------------------------------------------------
# Per-benchmark build rule template
# $(1) = benchmark name   $(2) = memory target (spm | dram)
# ------------------------------------------------------------
define GEN_BENCHMARK_RULES
$(BUILD_DIR)/$(1)/$(1).$(2).elf:
	@mkdir -p $(BUILD_DIR)/$(1)
	$$(CC) $$(PLAT_CFLAGS) $$(BENCH_CFLAGS) $$(EXTRA_CFLAGS) \
			$(call get_mem_flags,$(2)) \
			$$(BENCH_INCLUDES) $$(BENCH_PLAT_INCLUDES) $$(PLAT_INCLUDES) \
			-T$$(PLAT_LD_DIR)/$(2).ld \
			-Wl,-L$$(PLAT_LD_DIR) \
			-o $$@ \
			$$(PLAT_STARTUP) \
			$$(BENCH_PLAT_SRCS) $$(BENCH_COMMON_SRCS) \
			$$(call get-bmark-srcs,$(1)) \
			$$(PLAT_LDFLAGS) $$(PLAT_LIBS)

$(BUILD_DIR)/$(1)/$(1).$(2).dump: $(BUILD_DIR)/$(1)/$(1).$(2).elf
	$$(OBJDUMP) -d -S $$< > $$@
endef

# Apply the above template to each benchmark
$(foreach mem,$(PLAT_MEMORY),\
    $(foreach bmark,$(call bmarks_for_mem,$(mem)),\
        $(eval $(call GEN_BENCHMARK_RULES,$(bmark),$(mem)))\
    )\
)

# ============================================================
# Convenience top-level aliases
# ============================================================
.PHONY: bringup-bench
bringup-bench:
	$(MAKE) BENCHMARK=bringup-bench build

.PHONY: rtems rtems-bsp
rtems rtems-bsp:
	$(MAKE) BENCHMARK=rtems build PLATFORM=$(PLATFORM)

# Build toolchain RTEMS
.PHONY: rtems-toolchain
rtems-toolchain:
	@echo "[TOOLCHAIN] Building strict-align toolchain..."
	@$(RTEMS_PORTS_DIR)/toolchain/build-toolchain.sh \
		$(RTEMS_PORTS_DIR)/toolchain/rsb-strict \
		$(RTEMS_TOOLCHAIN_INSTALL_DIR)

# Se BENCHMARK e' rtems o rtems-bsp, delega a rtems-bsp.mk
ifeq ($(filter rtems rtems-bsp,$(BENCHMARK)),$(BENCHMARK))
build:
	@echo "[RTEMS] Compiling BSP RTEMS for $(PLATFORM)..."
	$(MAKE) -f $(ROOT)/benchmarks/rtems-bsp.mk \
		PLATFORM=$(PLATFORM) \
		RTEMS_TOOLS=$(RTEMS_TOOLCHAIN_INSTALL_DIR) \
		BSP_DIR=$(RTEMS_PORTS_DIR) \
		all
else
build: $(SUB_BMARK_DUMPS)
endif


# Build all available benchmarks
.PHONY: all_benchmarks
build-all:
	@$(foreach bmark,$(ALL_BENCHMARKS), \
		$(MAKE) build BENCHMARK=$(bmark) PLATFORM=$(PLATFORM) || exit $$?; \
	)
	
# ------------------------------------------------------------
# OBPMark
# Usage:
#   make obpmark PLATFORM=cheshire
#   make obpmark PLATFORM=astral TEST=1.1-image_calibration IMPL=openmp
#   make obpmark-rtems PLATFORM=cheshire
#   make obpmark-linux PLATFORM=astral
# ------------------------------------------------------------
TEST        ?= 3.1-aes_encryption
IMPL        ?= cpu
SIZE        ?= 128
NUM_THREADS ?= 2
NUM_CORES   ?= 2
OS          ?= rtems

obpmark:
	@if [ "$(origin PLATFORM)" != "command line" ]; then \
		echo ""; \
		echo " [ERRORE] Devi specificare la piattaforma target per OBPMark!"; \
		echo " Uso corretto:"; \
		echo "   make obpmark PLATFORM=astral [OS=linux]"; \
		echo "   make obpmark PLATFORM=cheshire"; \
		echo ""; \
		exit 1; \
	fi
	$(MAKE) -f benchmarks/obpmark.mk \
		TARGET=$(PLATFORM) \
		TEST=$(TEST) \
		IMPL=$(IMPL) \
		SIZE=$(SIZE) \
		NUM_THREADS=$(NUM_THREADS) \
		NUM_CORES=$(NUM_CORES) \
		OS=$(OS)


obpmark-rtems:
	$(MAKE) obpmark OS=rtems

obpmark-linux:
	$(MAKE) obpmark OS=linux

# ============================================================
# FPGA Commands (for bringup-bench)
# ============================================================
-include $(ROOT)/target/$(PLATFORM)/ports/bringup-bench/fpga.mk

# ============================================================
# Clean 
# ============================================================
.PHONY: clean clean-rtems distclean-rtems

.PHONY: clean
clean:
	@if [ "$(BENCHMARK)" = "obpmark" ]; then \
		$(MAKE) -f benchmarks/obpmark.mk clean; \
	else \
		rm -rf $(BUILD_DIR); \
		echo "[CLEAN] Removed $(BUILD_DIR)"; \
	fi
	
clean-rtems:
	$(MAKE) -f $(ROOT)/benchmarks/rtems-bsp.mk \
		PLATFORM=$(PLATFORM) \
		RTEMS_TOOLS=$(RTEMS_TOOLCHAIN_INSTALL_DIR) \
		BSP_DIR=$(RTEMS_PORTS_DIR) \
		clean

distclean-rtems:
	$(MAKE) -f $(ROOT)/benchmarks/rtems-bsp.mk \
		PLATFORM=$(PLATFORM) \
		RTEMS_TOOLS=$(RTEMS_TOOLCHAIN_INSTALL_DIR) \
		BSP_DIR=$(RTEMS_PORTS_DIR) \
		distclean

.PHONY: clean-%
clean-%:
	$(MAKE) clean BENCHMARK=$*