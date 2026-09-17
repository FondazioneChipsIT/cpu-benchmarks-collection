// main_wrapper.cpp — rinomina main di OBPMark per RTEMS
#define main BENCH_MAIN
#include BENCH_MAIN_INCLUDE
#undef main

extern "C" int run_benchmark(int argc, char **argv) {
    return BENCH_MAIN(argc, argv);
}