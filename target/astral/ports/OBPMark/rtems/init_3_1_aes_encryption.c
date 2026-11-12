/*
 * init_aes.c - init RTEMS 7 SMP per OBPMark 3.1 AES encryption
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

#ifndef PLAINTEXT_BYTES
#define PLAINTEXT_BYTES 4096
#endif

#ifndef KEY_BITS
#define KEY_BITS 128
#endif

/* Dichiarata in main_wrapper.cpp */
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

    char l_str[16], k_str[16];
    snprintf(l_str, sizeof(l_str), "%d", PLAINTEXT_BYTES);
    snprintf(k_str, sizeof(k_str), "%d", KEY_BITS);

    char *argv[] = {
        "obpmark_aes",
        "-l", l_str,
        "-k", k_str,
        "-r",
        "-t",
        NULL
    };
    struct bench_args args = { (int)(sizeof(argv) / sizeof(argv[0])) - 1, argv };

#if defined(OPENMP) || defined(_OPENMP)
    omp_set_num_threads(NUM_THREADS);
#endif

    bench_print_config("aes", BENCH_IMPL, PLAINTEXT_BYTES);
    printf("[bench] PARAMS   key_bits=%d\n", KEY_BITS);

#if (defined(OPENMP) || defined(_OPENMP)) && defined(OMP_DIAG)
    printf("[diag] MACRO: NUM_THREADS=%d NUM_CORES=%d num_procs=%d max_threads=%d\n",
           NUM_THREADS, NUM_CORES, omp_get_num_procs(), omp_get_max_threads());
    omp_diag_run(NUM_THREADS);
#endif

    bench_print_topology();

    int rc = bench_run("aes", PLAINTEXT_BYTES, run_once, &args,
                       PLAINTEXT_BYTES, "B");
    bench_print_end("aes", rc);

#ifdef STACK_CHECK
    rtems_stack_checker_report_usage();
#endif

    exit(rc);
    return NULL;
}

#include "bench_rtems_config.h"