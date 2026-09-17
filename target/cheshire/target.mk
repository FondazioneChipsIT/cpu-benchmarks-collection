CHESHIRE_SW_ROOT ?= $(abspath $(ROOT)/../..)

PLATFORM_SW_ROOT := $(ROOT)/target/cheshire/sw

PLAT_MEMORY      ?= dram spm
PLAT_INCLUDE_DIR := $(PLATFORM_SW_ROOT)/include
PLAT_LIB_DIR     := $(CHESHIRE_SW_ROOT)/lib
PLAT_LD_DIR      := $(CHESHIRE_SW_ROOT)/link

PLAT_FLAGS   = -DOT_PLATFORM_RV32 -march=rv64gc_zifencei -mabi=lp64d -mstrict-align -O3 -Wall -Wextra -static -ffunction-sections -fdata-sections -frandom-seed=cheshire -fuse-linker-plugin -flto -Wl,-flto
PLAT_CFLAGS  = $(PLAT_FLAGS) -ggdb -mcmodel=medany -mexplicit-relocs -fno-builtin -fverbose-asm -pipe -Wno-error=return-mismatch -Wno-error=implicit-int -Wno-error=implicit-function-declaration
PLAT_LDFLAGS = $(PLAT_FLAGS) -nostartfiles -nostdlib -Wl,--gc-sections -Wl,-L$(PLAT_LD_DIR) -Wl,-L$(PLAT_LIB_DIR)

PLAT_INCLUDES = \
        -I$(PLAT_INCLUDE_DIR) \
        -I$(CHESHIRE_SW_ROOT)/include \
        -I$(CHESHIRE_SW_ROOT)/deps/printf \
        -I$(PLATFORM_SW_ROOT)/deps/printf \
        -I$(PLATFORM_SW_ROOT)/include

ifneq ($(strip $(CHS_LLC_DIR)),)
PLAT_INCLUDES += -I$(CHS_LLC_DIR)/sw/include
endif

ifneq ($(strip $(AXIRTROOT)),)
PLAT_INCLUDES += -I$(AXIRTROOT)/sw/lib
endif

ifneq ($(strip $(OTPROOT)),)
PLAT_INCLUDES += -I$(OTPROOT) -I$(OTPROOT)/sw/include
endif

PLAT_LIBS  := -lcheshire -lm
PORTS_DIR  := $(ROOT)/target/$(PLATFORM)/ports
DEPS_DIR   := $(ROOT)/benchmarks/deps
