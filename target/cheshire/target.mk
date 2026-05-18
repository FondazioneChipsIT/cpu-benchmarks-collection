PLATFORM_SW_ROOT ?=
PLAT_MEMORY      ?= dram spm
PLAT_INCLUDE_DIR := $(PLATFORM_SW_ROOT)/include
PLAT_LIB_DIR     := $(PLATFORM_SW_ROOT)/lib
PLAT_LD_DIR      := $(PLATFORM_SW_ROOT)/link
PLAT_FLAGS   = -DOT_PLATFORM_RV32 -march=rv64gc_zifencei -mabi=lp64d -mstrict-align -O3 -Wall -Wextra -static -ffunction-sections -fdata-sections -frandom-seed=cheshire -fuse-linker-plugin -flto -Wl,-flto
PLAT_CFLAGS  = $(PLAT_FLAGS) -ggdb -mcmodel=medany -mexplicit-relocs -fno-builtin -fverbose-asm -pipe -Wno-error=return-mismatch -Wno-error=implicit-int -Wno-error=implicit-function-declaration
PLAT_LDFLAGS = $(PLAT_FLAGS) -nostartfiles -nostdlib -Wl,--gc-sections -Wl,-L$(PLAT_LD_DIR) -Wl,-L$(PLAT_LIB_DIR)
PLAT_INCLUDES = \
        -I$(PLAT_INCLUDE_DIR) \
        -I$(PLATFORM_SW_ROOT)/deps/printf \
        -I$(CHS_LLC_DIR)/sw/include \
        -I$(AXIRTROOT)/sw/lib \
        -I$(OTPROOT) \
        -I$(OTPROOT)/sw/include
PLAT_LIBS  := -lcheshire -lm
PORTS_DIR  := $(ROOT)/target/$(PLATFORM)/ports
DEPS_DIR   := $(ROOT)/benchmarks/deps
