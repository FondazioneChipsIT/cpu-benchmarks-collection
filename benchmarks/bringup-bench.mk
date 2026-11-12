# ============================================================
# Bringup-Bench — cpu-benchmarks-collection integration
# ============================================================

BB_DIR     := $(ROOT)/benchmarks/deps/bringup-bench
BB_BSP_DIR := $(ROOT)/target/cheshire/ports/bringup-bench

# build for simulator or FPGA (default)
ifeq ($(SIMULATOR),1)
BUILD_DIR := build/$(PLATFORM)/bringup-bench-sim/$(BENCHMARK)
else
BUILD_DIR := build/$(PLATFORM)/$(BENCHMARK)
endif

# ------------------------------------------------------------
# Full benchmark list (rsa-cipher and lz-compress excluded)
# ------------------------------------------------------------
bmarks := ackermann aes anagram audio-codec avl-tree banner bit-kernels blake2b \
          bloom-filter boyer-moore-search bubble-sort c-interp ccmac checkers chi-squared \
          cipher congrad connect4-minimax convex-hull dhrystone \
          distinctness donut edit-distance eulers-approx fft-int flood-fill \
          frac-calc fuzzy-match fy-shuffle gcd-list gemm grad-descent graph-tests \
          hanoi heapsort heat-calc heldkarp-tsp highlife huff-encode idct-alg \
          indirect-test k-means kadane kepler knapsack knights-tour lda life \
          longdiv lu-decomp mandelbrot matmult max-subseq mersenne \
          minspan monte-carlo moving-average murmur-hash n-queens natlog nbody-sim \
          nonlinear-nn nr-solver ntt-kernel packet-filter parrondo partition-equal \
          pascal pca-analysis pi-calc primal-test priority-queue qsort-demo qsort-test \
          quartile-stats quaternions quine rabinkarp-search rad-to-deg rand-test \
          randshell-sort ransac regex-parser rho-factor rle-compress sat-solver shortest-path sieve simple-grep \
          skeleton spelt2num spirograph strange sudoku-solver tea-cipher tetris-sim \
          tiny-NN topo-sort totient transcend triangle-count uniquify \
          variability-sample vectors-3d verlet weekday

# ------------------------------------------------------------
# SPM subset — benchmarks whose binary + stack + heap fit
# within the scratchpad constraints. 
# See STACK_SIZE_SPM / HEAP_SIZE_SPM below.
# ------------------------------------------------------------
bmarks_spm  := aes audio-codec banner bit-kernels blake2b \
               boyer-moore-search bubble-sort ccmac \
               cipher convex-hull dhrystone distinctness donut \
               eulers-approx fft-int flood-fill frac-calc fuzzy-match \
               fy-shuffle gcd-list grad-descent graph-tests hanoi \
               heat-calc highlife idct-alg indirect-test \
               kadane kepler knights-tour life longdiv lu-decomp \
               mandelbrot max-subseq mersenne minspan monte-carlo \
               moving-average murmur-hash n-queens natlog nbody-sim \
               nonlinear-nn nr-solver packet-filter parrondo \
               partition-equal pascal pca-analysis primal-test \
               qsort-demo qsort-test quartile-stats quaternions \
               quine rad-to-deg rand-test \
               randshell-sort ransac regex-parser rho-factor \
               rle-compress sat-solver shortest-path sieve \
               simple-grep skeleton spelt2num strange \
               sudoku-solver tea-cipher tetris-sim tiny-NN topo-sort \
               totient transcend triangle-count uniquify \
               variability-sample vectors-3d verlet weekday

# ------------------------------------------------------------
# Optional single-benchmark filter: make ... TEST=aes
# ------------------------------------------------------------
ifdef TEST
bmarks     := $(filter $(TEST),$(bmarks))
bmarks_spm := $(filter $(TEST),$(bmarks_spm))
endif

# bmarks_dram is always the full (possibly filtered) bmarks list
bmarks_dram := $(bmarks)

# Route each memory target to the right list
bmarks_for_mem = $(if $(filter spm,$(1)),$(bmarks_spm),$(bmarks_dram))

# ------------------------------------------------------------
# Source resolution
# ------------------------------------------------------------
# c-interp ships as a single amalgamated file; all others use *.c
define get-bmark-srcs
$(if $(filter c-interp,$(1)),\
    $(BB_DIR)/c-interp/c-interp.c,\
    $(wildcard $(BB_DIR)/$(1)/*.c))
endef

# Common libmin source files (ignoring stock libtarg to use our custom port)
BENCH_COMMON_SRCS := $(wildcard $(BB_DIR)/common/libmin_*.c)

# ------------------------------------------------------------
# Include paths
# ------------------------------------------------------------
CHESHIRE_SW_ROOT ?= ../..

BENCH_INCLUDES :=\
	-I$(BB_BSP_DIR)/include\
	-I$(BB_DIR)/common\
	-I$(BB_DIR)/target\
	-I$(CHESHIRE_SW_ROOT)/include
    
#------------------------------------------------------------
# Compiler flags
# bringup-bench uses its own self-contained BSP and does not
# need the Cheshire SDK headers beyond a minimal set, so we
# override PLAT_CFLAGS entirely rather than appending.
# ------------------------------------------------------------
BENCH_CFLAGS := -DTARGET_CHESHIRE

PLAT_CFLAGS := \
	-DTARGET_CHESHIRE \
	-march=rv64gc_zifencei -mabi=lp64d -mstrict-align \
	-O3 -Wall -Wextra -static \
	-ffunction-sections -fdata-sections \
	-ggdb -mcmodel=medany -mexplicit-relocs \
	-fno-builtin -fno-builtin-memset -fno-builtin-memcpy \
	-fverbose-asm -pipe \
	-Wno-error=return-mismatch \
	-Wno-error=implicit-int \
	-Wno-error=implicit-function-declaration \
	-DMEASURE_MEMORY

# Targets FPGA by default. For SIMULATOR environments (Questasim, VCS, etc.), run: make SIMULATOR=1
ifeq ($(SIMULATOR),1)
PLAT_CFLAGS += -DTARGET_CHESHIRE_SIMULATOR
endif

# ------------------------------------------------------------
# Startup, linker scripts, and libraries
# ------------------------------------------------------------
PLAT_STARTUP  := $(BB_BSP_DIR)/crt0.S
PLAT_LD_DIR   := $(BB_BSP_DIR)/link

# Linker flags and library paths
PLAT_LDFLAGS  := -march=rv64gc_zifencei -mabi=lp64d -mstrict-align -O3\
                 -static -mcmodel=medany -nostdlib -nostartfiles\
                 -fno-builtin -fno-builtin-memset -fno-builtin-memcpy\
                 -fno-builtin -Wl,--no-relax\
                 -L$(CHESHIRE_SW_ROOT)/lib\
                 -Wl,-L$(PLAT_LD_DIR)
PLAT_LIBS     := -lcheshire

# ------------------------------------------------------------
# Stack / heap sizes per memory target
# Consumed by get_mem_flags in the root Makefile.
# ------------------------------------------------------------
STACK_SIZE_SPM := 16384
HEAP_SIZE_SPM  := 2048
STACK_SIZE_DRAM := 65536
HEAP_SIZE_DRAM  := 2097152