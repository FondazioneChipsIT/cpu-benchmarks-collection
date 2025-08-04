# benchmarks/riscv-tests/benchmark.mk

# WIP!!!

# List of sub-benchmarks
# base_bmarks = \
# 	core \
# 	linear_alg-mid-100x100-sp \
# 	loops-all-mid-10k-sp \
# 	nnet_test \
# 	parser-125k \
# 	radix2-big-64k \
#         mt-sha-test \
#         zip-test
# 
# bmarks := $(base_bmarks)
# 
# # Directory paths
# BENCH_MITH_DIR := $(BENCH_DIR)/mith
# BENCH_KERNELS_DIR := $(BENCH_DIR)/benchmarks
# 
# BENCH_MITH_SRCS := $(BENCH_MITH_DIR)/src/*.c
# BENCH_COMMON_SRCS := $(wildcard $(BENCH_MITH_SRCS))
# 
# # Function to resolve sources for a given benchmark name
# define get-bmark-srcs
# $(wildcard $(BENCH_DIR)/benchmarks/$(1)/*.c)
# endef
# 
# # Include paths
# BENCH_INCLUDES := \
# 	-I$(BENCH_MITH_DIR)/include \
# 	-I$(BENCH_MITH_DIR)/al/include \
# 	-I$(BENCH_KERNELS_DIR)
