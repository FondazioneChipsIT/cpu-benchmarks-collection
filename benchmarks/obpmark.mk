# ============================================================
# OBPMark — build fragment
# ============================================================
# Invoked by the root Makefile via:
#   $(MAKE) -f benchmarks/obpmark.mk PLATFORM=cheshire|astral \
#           TEST=3.1-aes_encryption IMPL=cpu|openmp SIZE=128 OS=rtems|linux
#
# This file is NOT included by the generic GEN_BENCHMARK_RULES
# engine — OBPMark has its own source layout, toolchain (RTEMS
# compiler-strict), and link model that are incompatible with
# the generic bmarks/PLAT_* framework.
# ============================================================

MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

.PHONY: all clean
.DEFAULT_GOAL := all

# ------------------------------------------------------------
# Parameters (all overridable from command line)
# ------------------------------------------------------------
TEST        ?= 3.1-aes_encryption
IMPL        ?= cpu
SIZE        ?= 128
OS          ?= rtems
PLATFORM      ?= cheshire
NUM_THREADS ?= 2

ifeq ($(PLATFORM),astral)
NUM_CORES   ?= 1
else
NUM_CORES   ?= 2
endif

# ifeq ($(PLATFORM),astral)
#   ifeq ($(IMPL),openmp)
#     $(error [OBPMark] IMPL=openmp non e' supportato su PLATFORM=astral: RTEMS compilato senza SMP, single-core)
#   endif
# endif

BENCH_SLUG := $(subst -,_,$(subst .,_,$(TEST)))

# ------------------------------------------------------------
# Paths
# ------------------------------------------------------------
ROOT_DIR    := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
OBPMARK_SRC := $(ROOT_DIR)/benchmarks/deps/OBPMark/src
BENCH_DIR   := $(OBPMARK_SRC)/$(TEST)
COMMON      := $(OBPMARK_SRC)/common

BSP_BASE    := $(ROOT_DIR)/target/$(PLATFORM)/ports/rtems
RTEMS_PORT  := $(ROOT_DIR)/target/$(PLATFORM)/ports/OBPMark/rtems

# ============================================================
# OS = linux
# ============================================================
ifeq ($(OS),linux)

LINUX_PORT_DIR  := $(ROOT_DIR)/target/$(PLATFORM)/ports/OBPMark/linux
CONFIG_FILE     := $(LINUX_PORT_DIR)/$(IMPL).config
BUILD_DIR_LINUX := $(ROOT_DIR)/build/$(PLATFORM)/obpmark/linux
EXE_LINUX       := $(BUILD_DIR_LINUX)/obpmark_$(BENCH_SLUG)_$(IMPL)_$(SIZE).exe

OBPMARK_ROOT    := $(abspath $(ROOT_DIR)/benchmarks/deps/OBPMark)
VANILLA_BIN     := $(OBPMARK_ROOT)/bin/$(TEST)/obpmark-$(TEST)_$(IMPL)

# Include paths necessari per la struttura interna di OBPMark
INC_COMMON      := $(OBPMARK_ROOT)/src/common
INC_BENCH       := $(OBPMARK_ROOT)/src/$(TEST)/include
INC_PORTS       := $(OBPMARK_ROOT)/src/$(TEST)/src/ports/$(IMPL)

all:
	@echo "=== [OBPMark] Building $(TEST) ($(IMPL)) for Linux/$(PLATFORM) ==="
	mkdir -p $(BUILD_DIR_LINUX)
	$(eval EXTRA_FLAGS := $(shell [ -f $(CONFIG_FILE) ] && cat $(CONFIG_FILE) | xargs))
	$(MAKE) -C $(OBPMARK_ROOT) $(IMPL) \
		SUBDIRS="$(TEST)" \
		CFLAGS="$(EXTRA_FLAGS) -DSIZE=$(SIZE) -I$(INC_COMMON) -I$(INC_BENCH) -I$(INC_PORTS)" \
		CXXFLAGS="$(EXTRA_FLAGS) -DSIZE=$(SIZE) -I$(INC_COMMON) -I$(INC_BENCH) -I$(INC_PORTS)"
	@[ -f $(VANILLA_BIN) ] || { \
		echo "ERROR: expected binary $(VANILLA_BIN) not found"; exit 1; }
	cp $(VANILLA_BIN) $(EXE_LINUX)
	@echo "=== Generated: $(EXE_LINUX) ==="
	
# ============================================================
# OS = rtems  (default)
# ============================================================
else

BUILD_DIR := $(ROOT_DIR)/build/$(PLATFORM)/obpmark/rtems
OBJ_DIR   := $(BUILD_DIR)/_obj/$(BENCH_SLUG)_$(IMPL)_$(SIZE)
EXE       := $(BUILD_DIR)/obpmark_$(BENCH_SLUG)_$(IMPL)_$(SIZE).exe
INIT_SRC  := $(RTEMS_PORT)/init_$(BENCH_SLUG).c

# ------------------------------------------------------------
# Toolchain (RTEMS compiler-strict, per PLATFORM)
# ------------------------------------------------------------
RTEMS_TOOLS := $(BSP_BASE)/toolchain/compiler-strict/bin
BSP_INSTALL := $(BSP_BASE)/install/riscv-rtems7/rv64imafdc
CROSS       := riscv-rtems7
CC          := $(RTEMS_TOOLS)/$(CROSS)-gcc
CXX         := $(RTEMS_TOOLS)/$(CROSS)-c++
OBJCOPY_CMD := $(RTEMS_TOOLS)/$(CROSS)-objcopy

# ------------------------------------------------------------
# Compiler flags - CHESHIRE
# ------------------------------------------------------------
CFLAGS := \
	-I$(BSP_INSTALL)/lib/include \
	-B$(BSP_INSTALL)/lib \
	-qrtems \
	-march=rv64imafdc_zicsr_zifencei -mabi=lp64d -mcmodel=medany \
	-O3 -g \
	-DSIZE=$(SIZE) \
	-DNUM_THREADS=$(NUM_THREADS) \
	-DNUM_CORES=$(NUM_CORES)


# ------------------------------------------------------------
# Compiler flags - FOR ASTRAL(to reduce footprint)
# ------------------------------------------------------------
ifeq ($(PLATFORM),astral)
CFLAGS := \
	-I$(BSP_INSTALL)/lib/include \
	-B$(BSP_INSTALL)/lib \
	-qrtems \
	-march=rv64imafdc_zicsr_zifencei -mabi=lp64d -mcmodel=medany \
	-Os \
	-ffunction-sections -fdata-sections \
	-fno-exceptions -fno-rtti \
	-DSIZE=$(SIZE) \
	-DNUM_THREADS=$(NUM_THREADS) \
	-DNUM_CORES=$(NUM_CORES)
endif

ifeq ($(IMPL),openmp)
CFLAGS += -fopenmp -DOPENMP
endif

CXXFLAGS := $(CFLAGS) -include ctime

# CFLAGS   += $(EXTRA_CFLAGS)
# CXXFLAGS += $(EXTRA_CFLAGS)

INCLUDES := \
	-I$(BENCH_DIR)/include \
	-I$(BENCH_DIR)/src/ports/$(IMPL) \
	-I$(COMMON)

# ------------------------------------------------------------
# Sources
# main.cpp is excluded — replaced by main_wrapper.cpp
# ------------------------------------------------------------
BENCH_SRCS_C := $(filter-out $(BENCH_DIR)/src/main.cpp,\
                  $(wildcard $(BENCH_DIR)/src/*.c) \
                  $(wildcard $(BENCH_DIR)/src/ports/$(IMPL)/*.c) \
                  $(wildcard $(COMMON)/*.c))

BENCH_SRCS_CPP := $(filter-out $(BENCH_DIR)/src/main.cpp,\
                    $(wildcard $(BENCH_DIR)/src/*.cpp) \
                    $(wildcard $(BENCH_DIR)/src/ports/$(IMPL)/*.cpp) \
                    $(wildcard $(COMMON)/*.cpp))

BENCH_OBJS := \
	$(patsubst %.c,$(OBJ_DIR)/%.o,$(notdir $(BENCH_SRCS_C))) \
	$(patsubst %.cpp,$(OBJ_DIR)/%.o,$(notdir $(BENCH_SRCS_CPP)))

VPATH := \
	$(BENCH_DIR)/src \
	$(BENCH_DIR)/src/ports/$(IMPL) \
	$(COMMON)

# ------------------------------------------------------------
# Root filesystem image (input data embedded in the binary)
# ------------------------------------------------------------
INPUT_DIR   := $(OBPMARK_SRC)/../data/input_data/$(TEST)/$(SIZE)
DATA_PATH   := data/input_data/$(TEST)/$(SIZE)
INPUT_FILES := $(wildcard $(INPUT_DIR)/*)

$(BUILD_DIR) $(OBJ_DIR):
	mkdir -p $@

# DOPO (CORRETTO)
$(OBJ_DIR)/rootfs.tar: | $(OBJ_DIR)
	@[ -n "$(INPUT_FILES)" ] || { \
	    echo "ERROR: no input files found in $(INPUT_DIR)"; exit 1; }
	rm -rf $(OBJ_DIR)/_tarroot
	mkdir -p $(OBJ_DIR)/_tarroot/$(DATA_PATH)
	cp $(INPUT_FILES) $(OBJ_DIR)/_tarroot/$(DATA_PATH)/
	cd $(OBJ_DIR)/_tarroot && tar -cf ../rootfs.tar -b 1 data
	rm -rf $(OBJ_DIR)/_tarroot

# $(OBJ_DIR)/rootfs.o: $(OBJ_DIR)/rootfs.tar
# 	cd $(OBJ_DIR) && $(OBJCOPY_CMD) \
# 		-I binary -O elf64-littleriscv -B riscv \
# 		$(notdir $<) $(notdir $@)


# In RTEMS e sulle architetture embedded, la sezione .rodata risiede nella memoria ROM/Flash o nello spazio riservato alle costanti del programma, lasciando libera la sezione .data in RAM.
#Ho messo la sezione .data in .rodata perche' altrimenti il linker di RTEMS non riesce a linkare il file rootfs.o, che contiene i dati del filesystem tar, nella sezione .data, che e' in RAM. In questo modo, i dati del filesystem tar saranno letti dalla sezione .rodata, che e' in ROM/Flash o nello spazio riservato alle costanti del programma.
# rodata messo poi in L1 che è solo lettura
$(OBJ_DIR)/rootfs.o: $(OBJ_DIR)/rootfs.tar
	cd $(OBJ_DIR) && $(OBJCOPY_CMD) \
		-I binary -O elf64-littleriscv -B riscv \
		--rename-section .data=.rodata,alloc,load,readonly,data,contents \
		$(notdir $<) $(notdir $@)

# ------------------------------------------------------------
# Compilation rules
# ------------------------------------------------------------
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(OBJ_DIR)/%.o: %.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

$(OBJ_DIR)/init_$(BENCH_SLUG).o: $(INIT_SRC) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

ifeq ($(TEST)_$(IMPL),1.1-image_cpu)
# OBPMark 1.1-image cpu: scrub_mask.f non e' mai allocato ma clean() lo libera
$(OBJ_DIR)/device.o: $(BENCH_DIR)/src/ports/$(IMPL)/device.c | $(OBJ_DIR)
	sed '/free(image_data->scrub_mask.f);/d' $< > $(OBJ_DIR)/device_patched.c
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $(OBJ_DIR)/device_patched.c
endif
# $(OBJ_DIR)/main_wrapper_$(BENCH_SLUG).o: \
# 	$(RTEMS_PORT)/main_wrapper.cpp $(BENCH_DIR)/src/main.cpp $(BENCH_OBJS) | $(OBJ_DIR)

# 	$(eval KSYM := $(strip $(shell nm $(OBJ_DIR)/device.o | awk '$$2=="T" && $$3 ~ /process_benchmark/ {print $$3; exit}')))
# 	@echo "$(KSYM)" > $(OBJ_DIR)/ksym.txt

# 	$(CXX) $(CXXFLAGS) $(INCLUDES) \
# 		$(if $(KSYM),-DBENCH_KERNEL_SYM=$(KSYM)) \
# 		-DBENCH_MAIN=obpmark_$(BENCH_SLUG)_main \
# 		-DBENCH_MAIN_INCLUDE=\"$(BENCH_DIR)/src/main.cpp\" \
# 		-c -o $@ $<

$(OBJ_DIR)/main_wrapper_$(BENCH_SLUG).o: \
	$(RTEMS_PORT)/main_wrapper.cpp $(BENCH_DIR)/src/main.cpp $(BENCH_OBJS) | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) \
		$(if $(KSYM),-DBENCH_KERNEL_SYM=$(KSYM)) \
		-DBENCH_MAIN=obpmark_$(BENCH_SLUG)_main \
		-DBENCH_MAIN_INCLUDE=\"$(BENCH_DIR)/src/main.cpp\" \
		-c -o $@ $<

# ------------------------------------------------------------
# Link
# ------------------------------------------------------------
all: $(EXE)



#aggiunto :-Wl,--gc-sections per astral to reduce footprint
# $(EXE): $(BUILD_DIR) \
#         $(OBJ_DIR)/init_$(BENCH_SLUG).o \
#         $(OBJ_DIR)/main_wrapper_$(BENCH_SLUG).o \
#         $(BENCH_OBJS) \
#         $(OBJ_DIR)/rootfs.o
# 	$(CXX) $(CXXFLAGS) $(LDFLAGS) -Wl,--gc-sections -o $@ \
# 		$(OBJ_DIR)/init_$(BENCH_SLUG).o \
# 		$(OBJ_DIR)/main_wrapper_$(BENCH_SLUG).o \
# 		$(BENCH_OBJS) \
# 		$(OBJ_DIR)/rootfs.o
# 	@echo "=== Generated: $(EXE) ==="

ifeq ($(USE_ROOTFS_TAR),yes)
CFLAGS     += -DUSE_ROOTFS_TAR
CXXFLAGS   += -DUSE_ROOTFS_TAR
ROOTFS_OBJ := $(OBJ_DIR)/rootfs.o
else
ROOTFS_OBJ :=
endif

# Trigger GPIO: BENCH_TRIGGER: trigger run intera , BENCH_TRIGGER_KERNEL: trigger solo kernel (sincronizzato con Elapsed)
ifeq ($(BENCH_TRIGGER),yes)
BENCH_TRIG_FLAGS := -DBENCH_TRIGGER -DBENCH_PAD_SETUP \
          -DBENCH_PADFRAME_BASE=0x21000000UL \
          -DBENCH_PAD_MUX_OFF=0x8 -DBENCH_PAD_MUX_GPIO=0x2 -DBENCH_PAD_CFG_OFF=0x4

ifeq ($(BENCH_TRIGGER_KERNEL),yes)
BENCH_TRIG_FLAGS += -DBENCH_TRIGGER_KERNEL
# simbolo mangled di process_benchmark letto da device.o (valutato solo quando gira la ricetta)
KSYM = $(strip $(shell nm $(OBJ_DIR)/device.o | awk '$$2=="T" && $$3 ~ /process_benchmark/ {print $$3; exit}'))
endif

CFLAGS   += $(BENCH_TRIG_FLAGS)
CXXFLAGS += $(BENCH_TRIG_FLAGS)
endif

# ifeq ($(BENCH_TRIGGER),yes)
# BENCH_TRIG_FLAGS := -DBENCH_TRIGGER -DBENCH_PAD_SETUP \
#           -DBENCH_PADFRAME_BASE=0x21000000UL \
#           -DBENCH_PAD_MUX_OFF=0x8 -DBENCH_PAD_MUX_GPIO=0x2 -DBENCH_PAD_CFG_OFF=0x4

# # trigger sul solo kernel (sincronizzato con Elapsed); simbolo valido solo per l'AES
# ifeq ($(BENCH_TRIGGER_KERNEL),yes)
# BENCH_TRIG_FLAGS += -DBENCH_TRIGGER_KERNEL
# LDFLAGS += -Wl,--wrap=_Z17process_benchmarkP10AES_data_tP10AES_time_t
# endif

# CFLAGS   += $(BENCH_TRIG_FLAGS)
# CXXFLAGS += $(BENCH_TRIG_FLAGS)
# endif

MINI_GOMP_DIR := /data2/fseratini/cheshirepulito/sw/deps/cpu-benchmarks-collection/target/astral/ports/rtems

$(EXE): $(BUILD_DIR) \
        $(OBJ_DIR)/init_$(BENCH_SLUG).o \
        $(OBJ_DIR)/main_wrapper_$(BENCH_SLUG).o \
        $(BENCH_OBJS) \
        $(ROOTFS_OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) \
		-L$(MINI_GOMP_DIR) \
		$(if $(KSYM),-Xlinker --wrap=$(KSYM)) \
		-Wl,--gc-sections -o $@ \
		$(OBJ_DIR)/init_$(BENCH_SLUG).o \
		$(OBJ_DIR)/main_wrapper_$(BENCH_SLUG).o \
		$(BENCH_OBJS) \
		$(ROOTFS_OBJ) \
		-lgomp
		
endif  # OS

# ------------------------------------------------------------
# Clean — scoped to the current PLATFORM only
# ------------------------------------------------------------
clean:
	@echo "[OBPMark] Cleaning build/$(PLATFORM)/obpmark..."
	rm -rf $(ROOT_DIR)/build/$(PLATFORM)/obpmark
