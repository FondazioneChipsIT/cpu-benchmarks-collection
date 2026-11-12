/*
 * bench_common.h - stampa uniforme per OBPMark su Astral (AES, image, compression)
 *
 * Tutte le righe iniziano con "[bench]" e hanno campi chiave=valore, cosi'
 * si confrontano con grep/diff fra build a 1 core e a 2 core:
 *
 *   [bench] CONFIG   name= impl= size= cores_cfg= threads_cfg= procs_rtems=
 *   [bench] TOPOLOGY team= cpu_tid0= [cpu_tid1=]
 *   [bench] RUN      i= time_us=
 *   [bench] RESULT   name= size= cores= threads= reps= min_us= mean_us= rate= unit= rc=
 *   [bench] CHECKSUM name= fnv1a=0x........
 *
 * Solo aritmetica intera: niente %f (evita dtoa/Balloc e il "0.00" nei throughput).
 *
 * Uso nell'init di ogni benchmark:
 *
 *   struct args { int argc; char **argv; };
 *   static int run_once(void *p) {
 *       struct args *a = (struct args *)p;
 *       return run_benchmark(a->argc, a->argv);
 *   }
 *   ...
 *   bench_print_config("aes", "openmp", 4096);
 *   bench_print_topology();
 *   rc = bench_run("aes", 4096, run_once, &a, 4096, "B");
 *
 * 'work' = unita' di lavoro per singola run (byte per AES, width*height*frames
 * per image, campioni per compression), 'unit' = "B" / "pixel" / "sample".
 *
 * NOTA: bench_run chiama fn piu' volte (BENCH_WARMUP + BENCH_REPEATS). Se
 * run_benchmark non e' rientrante, compila con -DBENCH_WARMUP=0 -DBENCH_REPEATS=1.
 */
#ifndef BENCH_COMMON_H
#define BENCH_COMMON_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <errno.h>
#include <rtems.h>
#include "bench_trigger.h"      

#ifdef BENCH_TRIGGER_KERNEL
extern volatile int bench_trigger_armed;   /* definita in main_wrapper.cpp */
#endif

#if defined(OPENMP) || defined(_OPENMP)
  #include <omp.h>
#endif

#ifndef NUM_THREADS
#define NUM_THREADS 1
#endif
#ifndef NUM_CORES
#define NUM_CORES 1
#endif
#ifndef BENCH_WARMUP
#define BENCH_WARMUP 1          /* run scartate prima delle misure */
#endif
#ifndef BENCH_REPEATS
#define BENCH_REPEATS 6         /* run misurate */
#endif
#define BENCH_MAX_REPEATS 16

#if defined(OPENMP) || defined(_OPENMP)
  #define BENCH_IMPL "openmp"
#else
  #define BENCH_IMPL "cpu"
#endif


typedef int (*bench_fn_t)(void *ctx);

//converts to microseconds, using CLOCK_MONOTONIC (monotonic clock, not affected by system time changes)
static inline uint64_t bench_now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec / 1000u;
}

/* Checksum dell'output: serve a verificare che 1 core e 2 core diano lo stesso risultato. */
static inline uint32_t bench_fnv1a(const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t h = 2166136261u;
    while (n--) { h ^= *b++; h *= 16777619u; }
    return h;
}

static inline void bench_print_checksum(const char *name, const void *buf, size_t n)
{
    printf("[bench] CHECKSUM name=%s fnv1a=0x%08lx\n",
           name, (unsigned long)bench_fnv1a(buf, n));
}

static inline void bench_print_config(const char *name, const char *impl, unsigned size)
{
    printf("[bench] CONFIG   name=%s impl=%s size=%u cores_cfg=%d threads_cfg=%d procs_rtems=%u\n",
           name, impl, size, NUM_CORES, NUM_THREADS,
           (unsigned)rtems_configuration_get_maximum_processors());
}

/* Team reale e core fisico di ogni thread, stampato DOPO la regione (niente interleaving). */
static inline void bench_print_topology(void)
{
#if defined(OPENMP) || defined(_OPENMP)
    int team = 1;
    unsigned cpu[2] = { 0u, 0u };

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        if (tid == 0) team = omp_get_num_threads();
        if (tid >= 0 && tid < 2) cpu[tid] = (unsigned)rtems_scheduler_get_processor();
    }

    printf("[bench] TOPOLOGY team=%d cpu_tid0=%u", team, cpu[0]);
    if (team > 1) printf(" cpu_tid1=%u", cpu[1]);
    printf("\n");
#else
    printf("[bench] TOPOLOGY team=1 cpu_tid0=%u\n",
           (unsigned)rtems_scheduler_get_processor());
#endif
}

/* Warm-up + ripetizioni; stampa ogni RUN e il RESULT finale. Ritorna 0 se tutte le run hanno rc==0. */
static inline int bench_run(const char *name, unsigned size, bench_fn_t fn, void *ctx,
                            uint64_t work, const char *unit)
{

    uint64_t t[BENCH_MAX_REPEATS];
    int reps = (BENCH_REPEATS > BENCH_MAX_REPEATS) ? BENCH_MAX_REPEATS : BENCH_REPEATS;
    int rc = 0;
    int i;

    if (reps < 1) reps = 1;
    errno = 0;

    bench_trigger_init(); //trigger GPIO

    for (i = 0; i < BENCH_WARMUP; i++)
        rc |= fn(ctx);

    for (i = 0; i < reps; i++) {

    #ifdef BENCH_TRIGGER_KERNEL
            bench_trigger_armed = 1;      /* il wrapper alza il GPIO solo dentro il kernel */
    #else
            bench_trigger(1);
    #endif
            uint64_t t0 = bench_now_us();
            rc |= fn(ctx);
            t[i] = bench_now_us() - t0;
    #ifdef BENCH_TRIGGER_KERNEL
            bench_trigger_armed = 0;
    #else
            bench_trigger(0);
    #endif
            printf("[bench] RUN      i=%d time_us=%llu\n", i, (unsigned long long)t[i]);
        }

    uint64_t min = t[0], sum = 0;
    for (i = 0; i < reps; i++) {
        if (t[i] < min) min = t[i];
        sum += t[i];
    }
    uint64_t mean = sum / (uint64_t)reps;
    uint64_t rate = min ? (work * 1000000ull) / min : 0;   /* unita' al secondo, sulla run migliore */

    printf("[bench] RESULT   name=%s size=%u cores=%d threads=%d reps=%d "
           "min_us=%llu mean_us=%llu rate=%llu unit=%s/s rc=%d\n",
           name, size, NUM_CORES, NUM_THREADS, reps,
           (unsigned long long)min, (unsigned long long)mean,
           (unsigned long long)rate, unit, rc);

    return rc;
}

/* Riga finale uniforme (sostituisce i vari "[rtems] ... done" + "errno = ..."). */
static inline void bench_print_end(const char *name, int rc)
{
    printf("[bench] END      name=%s rc=%d errno=%d\n", name, rc, errno);
}

#endif /* BENCH_COMMON_H */