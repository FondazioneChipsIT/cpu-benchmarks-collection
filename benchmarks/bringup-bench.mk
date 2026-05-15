# Bringup-Bench integration for cpu-benchmarks-collection
bmarks := bringup-bench

define get-bmark-srcs
$(wildcard $(BENCH_DIR)/$(1)/*.c)
endef

BENCH_INCLUDES := -I$(BENCH_DIR)/common -I$(BENCH_DIR)/target
BENCH_COMMON_SRCS := $(wildcard $(BENCH_DIR)/common/*.c)
BENCH_CFLAGS := -DTARGET_CHESHIRE
