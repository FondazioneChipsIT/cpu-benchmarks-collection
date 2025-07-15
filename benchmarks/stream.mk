# Sources and headers
# Function to resolve sources for a given benchmark name

bmarks := stream

define get-bmark-srcs
$(filter-out $(BENCH_DIR)/mysecond.c, $(wildcard $(BENCH_DIR)/*.c))
endef
BENCH_INCLUDES := -I$(BENCH_DIR)
BENCH_CFLAGS   := -DCUSTOM_MYSECOND -DSTREAM_ARRAY_SIZE=1000
