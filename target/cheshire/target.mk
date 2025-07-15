# Root of the Cheshire software install (override as needed)
PLATFORM_SDK_ROOT ?=

# Directories within PLATFORM_SDK_ROOT
PLAT_MEMORY      ?= dram
PLAT_INCLUDE_DIR := $(PLATFORM_SDK_ROOT)/include
PLAT_LIB_DIR     := $(PLATFORM_SDK_ROOT)/lib
PLAT_LD_DIR      := $(PLATFORM_SDK_ROOT)/link
PLAT_LD_SCRIPT   := $(PLAT_LD_DIR)/$(PLAT_MEMORY).ld

PLAT_FLAGS   = -DOT_PLATFORM_RV32 -march=rv64gc_zifencei -mabi=lp64d -mstrict-align -O2 -Wall -Wextra -static -ffunction-sections -fdata-sections -frandom-seed=cheshire -fuse-linker-plugin -flto -Wl,-flto
PLAT_CFLAGS  = $(PLAT_FLAGS) -ggdb -mcmodel=medany -mexplicit-relocs -fno-builtin -fverbose-asm -pipe 
PLAT_LDFLAGS = $(PLAT_FLAGS) -nostartfiles -Wl,--gc-sections -Wl,-L$(PLAT_LD_DIR) -Wl,-L$(PLAT_LIB_DIR)

# Include paths
PLAT_INCLUDES = \
	-I$(PLAT_INCLUDE_DIR) \
	-I$(PLATFORM_SDK_ROOT)/deps/printf \
        -I$(CHS_LLC_DIR)/sw/include \
        -I$(AXIRTROOT)/sw/lib \
        -I$(OTPROOT) \
        -I$(OTPROOT)/sw/include

# Libraries to link against
# Assumes libcheshire.a lives in $(PLAT_LIB_DIR)
PLAT_LIBS     := -lcheshire
PLAT_LIBS     += -lm
