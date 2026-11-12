/*
 * init_image.c - init RTEMS 7 SMP per OBPMark 1.1 image calibration and correction
 *
 * Opzioni: -DOMP_DIAG    test esplicito dei 2 core (omp_diag.h)
 *          -DSTACK_CHECK stack checker + report finale
 * Output uniforme: bench_common.h. Configurazione RTEMS: bench_rtems_config.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <rtems.h>

#if defined(OPENMP) || defined(_OPENMP)
  #include <omp.h>
#endif

#ifdef OMP_DIAG
  #include "omp_diag.h"
#endif

#ifdef STACK_CHECK
  #include <rtems/stackchk.h>
#endif

#include "bench_common.h"

#ifndef SIZE
#define SIZE 32
#endif

#ifndef NUM_FRAMES
#define NUM_FRAMES 2
#endif

extern int run_benchmark(int argc, char **argv);

struct bench_args { int argc; char **argv; };

static int run_once(void *p)
{
    struct bench_args *a = (struct bench_args *)p;
    return run_benchmark(a->argc, a->argv);
}

void *POSIX_Init(void *arg)
{
    (void)arg;

    char f_str[16], w_str[16], h_str[16];
    snprintf(f_str, sizeof(f_str), "%d", NUM_FRAMES);
    snprintf(w_str, sizeof(w_str), "%d", SIZE);
    snprintf(h_str, sizeof(h_str), "%d", SIZE);

    char *argv[] = {
        "obpmark_image",
        "-f", f_str,
        "-w", w_str,
        "-h", h_str,
        "-r",
        "-t",
        NULL
    };
    struct bench_args args = { (int)(sizeof(argv) / sizeof(argv[0])) - 1, argv };

#if defined(OPENMP) || defined(_OPENMP)
    omp_set_num_threads(NUM_THREADS);
#endif

    bench_print_config("image", BENCH_IMPL, SIZE);
    printf("[bench] PARAMS   width=%d height=%d frames=%d\n", SIZE, SIZE, NUM_FRAMES);

#if (defined(OPENMP) || defined(_OPENMP)) && defined(OMP_DIAG)
    printf("[diag] MACRO: NUM_THREADS=%d NUM_CORES=%d num_procs=%d max_threads=%d\n",
           NUM_THREADS, NUM_CORES, omp_get_num_procs(), omp_get_max_threads());
    omp_diag_run(NUM_THREADS);
#endif

    bench_print_topology();

    int rc = bench_run("image", SIZE, run_once, &args,
                       (uint64_t)SIZE * SIZE * NUM_FRAMES, "pixel");
    bench_print_end("image", rc);

#ifdef STACK_CHECK
    rtems_stack_checker_report_usage();
#endif

    exit(rc);
    return NULL;
}

#include "bench_rtems_config.h"