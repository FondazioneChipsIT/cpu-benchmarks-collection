# ============================================================
# Bringup-Bench port for Cheshire
# ============================================================
BENCH_INCLUDES := -I$(PORTS_DIR)/bringup-bench/include $(BENCH_INCLUDES)

BMARK_NAME    := bringup-bench
BMARK_DIR     := $(DEPS_DIR)/bringup-bench
BSP_DIR       := $(PORTS_DIR)/bringup-bench

PLAT_CFLAGS := -DTARGET_CHESHIRE \
               -march=rv64imafdc -mabi=lp64d \
               -mstrict-align -static -mcmodel=medany \
               -nostdlib -nostartfiles -ffreestanding \
               -fno-builtin -Wl,--no-relax \
               -Wall -O3 -g

PLAT_INCLUDES := -I$(PORTS_DIR)/bringup-bench/include $(PLAT_INCLUDES)
