# ==============================================================================s
# NOTE ON EMBENCH-IOT SUBMODULE:
# Ensure the submodule in 'benchmarks/deps/embench-iot' points to 
# 'upstream/embench-2.0-branch' or 'upstream/master'.
# ==============================================================================

# ------------------------------------------------------------
# Benchmark list
# ------------------------------------------------------------
base_bmarks = \
    aha-mont64     \
    crc32          \
    depthconv      \
    edn            \
    huffbench      \
    matmult-int    \
    md5sum         \
    nettle-aes     \
    nettle-sha256  \
    nsichneu       \
    picojpeg       \
    qrduino        \
    sglib-combined \
    slre           \
    statemate      \
    tarfind        \
    ud             \
    wikisort       \
    xgboost

ifdef TEST
bmarks := $(filter $(TEST),$(base_bmarks))
else
bmarks := $(base_bmarks)
endif
# ------------------------------------------------------------
# Source resolution
# embench-iot sources live under src/<benchmark>/
# ------------------------------------------------------------
define get-bmark-srcs
$(wildcard $(BENCH_DIR)/src/$(1)/*.c)
endef

# Include paths
BENCH_INCLUDES := \
	$(addprefix -I$(BENCH_DIR)/benchmarks/, $(bmarks))
# ------------------------------------------------------------
# Compiler flags
# ------------------------------------------------------------
# embench-iot requires CPU_MHZ for its timing model and
# GLOBAL_SCALE_FACTOR to normalise scores.
BENCH_CFLAGS += -DCPU_MHZ=50 -DGLOBAL_SCALE_FACTOR=1 


# wikisort defines a local 'bool' that conflicts with <stdbool.h>.
# Rename it only for wikisort targets to avoid polluting other benchmarks.
%wikisort.spm.elf %wikisort.dram.elf: BENCH_CFLAGS += -Dbool=wikisort_bool_t

# ADD libc e libgcc only for embench-iot
PLAT_LIBS += -lc -lgcc

