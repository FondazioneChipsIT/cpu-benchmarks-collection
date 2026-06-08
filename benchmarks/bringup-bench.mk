# ============================================================
# Bringup-Bench — cpu-benchmarks-collection integration
# ============================================================

BB_DIR     := $(ROOT)/benchmarks/deps/bringup-bench
BB_BSP_DIR := $(ROOT)/target/cheshire/ports/bringup-bench

# Benchmark list
bmarks := ackermann aes anagram audio-codec avl-tree banner bit-kernels blake2b \
          bloom-filter boyer-moore-search bubble-sort c-interp ccmac checkers chi-squared \
          cipher congrad connect4-minimax convex-hull dhrystone \
          distinctness donut edit-distance eulers-approx fft-int flood-fill \
          frac-calc fuzzy-match fy-shuffle gcd-list gemm grad-descent graph-tests \
          hanoi heapsort heat-calc heldkarp-tsp highlife huff-encode idct-alg \
          indirect-test k-means kadane kepler knapsack knights-tour lda life \
          longdiv lu-decomp lz-compress mandelbrot matmult max-subseq mersenne \
          minspan monte-carlo moving-average murmur-hash n-queens natlog nbody-sim \
          nonlinear-nn nr-solver ntt-kernel packet-filter parrondo partition-equal \
          pascal pca-analysis pi-calc primal-test priority-queue qsort-demo \
          qsort-test quartile-stats quaternions quine rabinkarp-search rad-to-deg \
          rand-test randshell-sort ransac regex-parser rho-factor rle-compress rsa-cipher \
          sat-solver shortest-path sieve simple-grep skeleton spelt2num \
          spirograph strange sudoku-solver tea-cipher tetris-sim tiny-NN topo-sort \
          totient transcend triangle-count uniquify variability-sample vectors-3d \
          verlet weekday


BB_OVERRIDES_DIR := $(BB_BSP_DIR)/overrides

# $(if $(wildcard $(BB_OVERRIDES_DIR)/$(1).c),\
    $(BB_OVERRIDES_DIR)/$(1).c,\

# Function to resolve source files for each benchmark
define get-bmark-srcs
$(if $(wildcard $(BB_OVERRIDES_DIR)/$(1).c),\
    $(BB_OVERRIDES_DIR)/$(1).c,\
$(if $(filter c-interp,$(1)),\
    $(BB_DIR)/c-interp/c-interp.c,\
    $(wildcard $(BB_DIR)/$(1)/*.c)))
endef

# Common libmin source files (ignoring stock libtarg to use our custom port)
BENCH_COMMON_SRCS := $(wildcard $(BB_DIR)/common/libmin_*.c)

# Include directories
BENCH_INCLUDES :=\
	-I$(BB_BSP_DIR)/include\
	-I$(BB_DIR)/common\
	-I$(BB_DIR)/target

# Global benchmark flags 
BENCH_CFLAGS := -DTARGET_CHESHIRE

# Startup and linker flags — custom override for bringup-bench
PLAT_CFLAGS  := -DTARGET_CHESHIRE \
                -march=rv64gc_zifencei -mabi=lp64d -mstrict-align \
                -O3 -Wall -Wextra -static \
                -ffunction-sections -fdata-sections \
                -ggdb -mcmodel=medany -mexplicit-relocs \
                -fno-builtin -fno-builtin-memset -fno-builtin-memcpy \
                -fverbose-asm -pipe \
                -Wno-error=return-mismatch \
                -Wno-error=implicit-int \
                -Wno-error=implicit-function-declaration
                
# Targets FPGA by default. For SIMULATOR environments (Questasim, VCS, etc.), run: make SIMULATOR=1
ifeq ($(SIMULATOR),1)
PLAT_CFLAGS += -DTARGET_CHESHIRE_SIMULATOR
endif

PLAT_STARTUP  := $(BB_BSP_DIR)/cheshire-crt0.S
PLAT_LD_DIR   := $(BB_BSP_DIR)/link
PLAT_LDFLAGS  := -march=rv64gc_zifencei -mabi=lp64d -mstrict-align -O3\
                 -static -mcmodel=medany -nostdlib -nostartfiles\
                 -fno-builtin -fno-builtin-memset -fno-builtin-memcpy\
                 -fno-builtin -Wl,--no-relax

PLAT_LIBS     := 
