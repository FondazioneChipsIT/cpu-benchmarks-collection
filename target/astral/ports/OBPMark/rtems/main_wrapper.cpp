// per misure buffer che rallenta esecuzione kernel l1 o spm 

// // main_wrapper.cpp — rinomina main di OBPMark per RTEMS
// #define main BENCH_MAIN
// #include BENCH_MAIN_INCLUDE
// #undef main

// #include <stdio.h>
// #include <stdint.h>
// #include <time.h>
// #include <termios.h>
// #include <unistd.h>
// #include "bench_trigger.h"

// extern "C" int run_benchmark(int argc, char **argv) {
//     return BENCH_MAIN(argc, argv);
// }

// #ifdef BENCH_TRIGGER_KERNEL
// #ifndef BENCH_KERNEL_SYM
// #error "BENCH_TRIGGER_KERNEL richiede -DBENCH_KERNEL_SYM=<simbolo mangled di process_benchmark>"
// #endif

// #define BENCH_CAT_(a, b) a##b
// #define BENCH_CAT(a, b)  BENCH_CAT_(a, b)
// #define BENCH_REAL BENCH_CAT(__real_, BENCH_KERNEL_SYM)
// #define BENCH_WRAP BENCH_CAT(__wrap_, BENCH_KERNEL_SYM)

// extern "C" {
// volatile int bench_trigger_armed = 0;

// typedef uintptr_t bench_reg_t;

// bench_reg_t BENCH_REAL(bench_reg_t, bench_reg_t, bench_reg_t, bench_reg_t,
//                        bench_reg_t, bench_reg_t, bench_reg_t, bench_reg_t);

// bench_reg_t BENCH_WRAP(bench_reg_t a0, bench_reg_t a1, bench_reg_t a2, bench_reg_t a3,
//                        bench_reg_t a4, bench_reg_t a5, bench_reg_t a6, bench_reg_t a7)
// {
//     int on = bench_trigger_armed;

// #ifdef BENCH_DEBUG_PTRS
//     {
//         const unsigned long *w = (const unsigned long *)a0;
//         printf("[bench] KPTR on=%d a0=%lx w0=%lx w1=%lx w2=%lx w10=%lx w11=%lx\n",
//                on, (unsigned long)a0, w[0], w[1], w[2], w[10], w[11]);
//     }
// #endif

// #ifdef BENCH_ALT_TEST
//     if (on) {
//         unsigned long us[8];
//         for (int k = 0; k < 8; k++) {
//             struct timespec t0, t1;
//             clock_gettime(CLOCK_MONOTONIC, &t0);
//             BENCH_REAL(a0, a1, a2, a3, a4, a5, a6, a7);
//             clock_gettime(CLOCK_MONOTONIC, &t1);
//             us[k] = (unsigned long)((t1.tv_sec - t0.tv_sec) * 1000000L
//                                     + (t1.tv_nsec - t0.tv_nsec) / 1000L);
//         }
//         for (int k = 0; k < 8; k++)
//             printf("[alt] k=%d us=%lu\n", k, us[k]);
//     }
// #endif

//     fflush(stdout);
//     tcdrain(STDOUT_FILENO);          /* UART ferma prima del kernel */

//     if (on) bench_trigger(1);
//     bench_reg_t r = BENCH_REAL(a0, a1, a2, a3, a4, a5, a6, a7);
//     if (on) bench_trigger(0);
//     return r;
// }
// }
// #endif



// main_wrapper.cpp — rinomina main di OBPMark per RTEMS
#define main BENCH_MAIN
#include BENCH_MAIN_INCLUDE
#undef main

#include "bench_trigger.h"
#include <stdint.h>   
#include <termios.h>
#include <unistd.h>

extern "C" int run_benchmark(int argc, char **argv) {
    return BENCH_MAIN(argc, argv);
}

#ifdef BENCH_TRIGGER_KERNEL
#ifndef BENCH_KERNEL_SYM
#error "BENCH_TRIGGER_KERNEL richiede -DBENCH_KERNEL_SYM=<simbolo mangled di process_benchmark>"
#endif

#define BENCH_CAT_(a, b) a##b
#define BENCH_CAT(a, b)  BENCH_CAT_(a, b)
#define BENCH_REAL BENCH_CAT(__real_, BENCH_KERNEL_SYM)
#define BENCH_WRAP BENCH_CAT(__wrap_, BENCH_KERNEL_SYM)

extern "C" {
volatile int bench_trigger_armed = 0;

typedef uintptr_t bench_reg_t;

bench_reg_t BENCH_REAL(bench_reg_t, bench_reg_t, bench_reg_t, bench_reg_t,
                       bench_reg_t, bench_reg_t, bench_reg_t, bench_reg_t);

bench_reg_t BENCH_WRAP(bench_reg_t a0, bench_reg_t a1, bench_reg_t a2, bench_reg_t a3,
                       bench_reg_t a4, bench_reg_t a5, bench_reg_t a6, bench_reg_t a7)
{
    int on = bench_trigger_armed;
    if (on) bench_trigger(1);
    bench_reg_t r = BENCH_REAL(a0, a1, a2, a3, a4, a5, a6, a7);
    if (on) bench_trigger(0);
    return r;
}
}
#endif







// #define BENCH_CAT_(a, b) a##b
// #define BENCH_CAT(a, b)  BENCH_CAT_(a, b)
// #define BENCH_REAL BENCH_CAT(__real_, BENCH_KERNEL_SYM)
// #define BENCH_WRAP BENCH_CAT(__wrap_, BENCH_KERNEL_SYM)

// extern "C" {
// volatile int bench_trigger_armed = 0;

// void BENCH_REAL(void *a, void *b);

// void BENCH_WRAP(void *a, void *b)
// {
//     int on = bench_trigger_armed;
//     if (on) bench_trigger(1);
//     BENCH_REAL(a, b);
//     if (on) bench_trigger(0);
// }
// }
// #endif