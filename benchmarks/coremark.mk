# Sources and headers
# Function to resolve sources for a given benchmark name

bmarks := coremark

define get-bmark-srcs
$(wildcard $(BENCH_DIR)/*.c)
endef
BENCH_INCLUDES := -I$(BENCH_DIR)

# Benchmark-specific macros
ITERATIONS     ?= 5000
CLOCKS_PER_SEC ?= 200000000
BENCH_CFLAGS   := -DFLAGS_STR="\"$(PLAT_CFLAGS)\"" -DITERATIONS=$(ITERATIONS) -DCLOCKS_PER_SEC=$(CLOCKS_PER_SEC)
