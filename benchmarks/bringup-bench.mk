# ============================================================
# Bringup-Bench — cpu-benchmarks-collection integration
# ============================================================

BB_DIR     := $(ROOT)/benchmarks/deps/bringup-bench
BB_BSP_DIR := $(ROOT)/target/cheshire/ports/bringup-bench

# Lista completa benchmark (esclude quelli con file speciali)
bmarks := ackermann aes audio-codec avl-tree banner bit-kernels blake2b\
          bloom-filter boyer-moore-search bubble-sort cipher congrad\
          dhrystone distinctness fft-int flood-fill frac-calc fuzzy-match\
          fy-shuffle gcd-list grad-descent graph-tests hanoi heapsort\
          heat-calc idct-alg indirect-test k-means kadane kepler knapsack\
          knights-tour life longdiv lu-decomp mandelbrot matmult\
          max-subseq mersenne minspan monte-carlo moving-average\
          murmur-hash n-queens natlog nbody-sim nr-solver packet-filter\
          parrondo pascal primal-test priority-queue qsort-demo qsort-test\
          quaternions rand-test regex-parser rle-compress shortest-path\
          sieve simple-grep skeleton spelt2num spirograph strange\
          sudoku-solver tetris-sim tiny-NN topo-sort totient vectors-3d weekday

# Funzione sorgenti per ogni benchmark
define get-bmark-srcs
$(wildcard $(BB_DIR)/$(1)/*.c)
endef

# Sorgenti comuni libmin (NO libtarg vanilla — usiamo il nostro)
BENCH_COMMON_SRCS := $(wildcard $(BB_DIR)/common/libmin_*.c)


# Include
BENCH_INCLUDES :=\
	-I$(BB_BSP_DIR)/include\
	-I$(BB_DIR)/common\
	-I$(BB_DIR)/target


# Flag
BENCH_CFLAGS := -DTARGET_CHESHIRE 


# Startup e linker — override per bringup-bench
PLAT_STARTUP  := $(BB_BSP_DIR)/cheshire-crt0.S
PLAT_LD_DIR   := $(BB_BSP_DIR)/link
PLAT_LDFLAGS  := -march=rv64gc_zifencei -mabi=lp64d -mstrict-align -O3\
                 -static -mcmodel=medany -nostdlib -nostartfiles\
                 -fno-builtin -Wl,--no-relax
PLAT_LIBS     :=
