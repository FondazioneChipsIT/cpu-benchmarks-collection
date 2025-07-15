# benchmarks/riscv-tests/benchmark.mk

# List of sub-benchmarks
base_bmarks = \
	aha-mont64     \
	crc32	       \
	cubic	       \
	edn	       \
	huffbench      \
	matmult-int    \
	md5sum	       \
	minver	       \
	nbody	       \
	nettle-aes     \
	nettle-sha256  \
	nsichneu       \
	picojpeg       \
	primecount     \
	qrduino	       \
	sglib-combined \
	slre	       \
	st	       \
	statemate      \
	tarfind	       \
	ud	       

bmarks := $(base_bmarks)

# Function to resolve sources for a given benchmark name
define get-bmark-srcs
$(wildcard $(BENCH_DIR)/src/$(1)/*.c)
endef

# Include paths
BENCH_INCLUDES := \
	$(addprefix -I$(BENCH_DIR)/benchmarks/, $(bmarks))

BENCH_CFLAGS := -DCPU_MHZ=50
