/*
 * init_data_compression.c - init RTEMS 7 SMP per OBPMark 2.1 data compression
 *
 * Opzioni: -DOMP_DIAG       test esplicito dei 2 core (omp_diag.h)
 *          -DFS_DEBUG       se il file di input manca, stampa l'albero del filesystem
 *          -DSTACK_CHECK    stack checker + report finale
 *          -DUSE_ROOTFS_TAR estrae rootfs.tar (input del benchmark) in IMFS
 * Output uniforme: bench_common.h. Configurazione RTEMS: bench_rtems_config.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <rtems.h>

#if defined(OPENMP) || defined(_OPENMP)
  #include <omp.h>
#endif

#ifdef OMP_DIAG
  #include "omp_diag.h"
#endif

#ifdef FS_DEBUG
  #include <dirent.h>
#endif

#ifdef USE_ROOTFS_TAR
  #include <rtems/untar.h>
  extern char _binary_rootfs_tar_start[];
  extern char _binary_rootfs_tar_end[];
#endif

#ifdef STACK_CHECK
  #include <rtems/stackchk.h>
#endif

#include "bench_common.h"

#ifndef SIZE
#define SIZE 128
#endif

/* SIZE = lato dell'array di input (file ..._SIZExSIZE.bin).
 * Come nello script di default di OBPMark, -s e' il NUMERO DI CAMPIONI = SIZE^2
 * (1024x1024 -> 1048576). */
#define NUM_SAMPLES (SIZE * SIZE)

/* OBPMark 2.1 calcola: steps = NUM_SAMPLES / (r * j)   (divisione intera!)
 * quindi servono NUM_SAMPLES multiplo di r*j e almeno r*j, altrimenti steps = 0
 * e il benchmark non elabora nulla ("Number of samples = 0").
 * Script di default: 1048576 / (4096 * 64) = 4 step.
 * Qui: r (-r) = min(4096, NUM_SAMPLES / J)  ->  SIZE=32: r=16, 1 step. */
#ifndef RSAMPLES   /* reference sample interval (-r) */
#define RSAMPLES ((NUM_SAMPLES / JBLOCK) < 4096 ? (NUM_SAMPLES / JBLOCK) : 4096)
#endif

#ifndef JBLOCK     /* block size (-j): 8/16/32/64 (default dello script: 64) */
#define JBLOCK 64
#endif

#ifndef NBITS      /* bit per campione (-n) */
#define NBITS 16
#endif

#if (RSAMPLES * JBLOCK) > (NUM_SAMPLES)
#error "RSAMPLES*JBLOCK > NUM_SAMPLES: steps = 0, il benchmark non elabora nulla"
#endif
#if (NUM_SAMPLES % (RSAMPLES * JBLOCK)) != 0
#error "NUM_SAMPLES (SIZE*SIZE) deve essere multiplo di RSAMPLES*JBLOCK"
#endif

extern int run_benchmark(int argc, char **argv);

/* Report opzionale di mini_gomp (weak: se non esiste il link non fallisce) */
extern void gomp_report(void) __attribute__((weak));

struct bench_args { int argc; char **argv; };

static int run_once(void *p)
{
    struct bench_args *a = (struct bench_args *)p;
    return run_benchmark(a->argc, a->argv);
}

#ifdef FS_DEBUG
static void list_tree(const char *path, int depth)
{
    DIR *d = opendir(path);
    if (!d) { printf("[fs] opendir(%s) FALLITA: %s\n", path, strerror(errno)); return; }
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char full[160];
        snprintf(full, sizeof(full), "%s%s%s", path,
                 path[strlen(path) - 1] == '/' ? "" : "/", e->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        printf("[fs] %c %s (%ld B)\n", S_ISDIR(st.st_mode) ? 'D' : 'F', full, (long)st.st_size);
        if (S_ISDIR(st.st_mode) && depth < 5) list_tree(full, depth + 1);
    }
    closedir(d);
}
#endif

void *POSIX_Init(void *arg)
{
    (void)arg;

    char ns_str[16], r_str[16], j_str[16], n_str[16];
    snprintf(ns_str, sizeof(ns_str), "%d", NUM_SAMPLES);
    snprintf(r_str,  sizeof(r_str),  "%d", RSAMPLES);
    snprintf(j_str,  sizeof(j_str),  "%d", JBLOCK);
    snprintf(n_str,  sizeof(n_str),  "%d", NBITS);

    /* Percorso del file di input come e' dentro il tar (relativo a cwd = "/") */
    char input_path[128];
    snprintf(input_path, sizeof(input_path),
             "data/input_data/2.1-data_compression/%d/2.1-compression-data_%dx%d.bin",
             SIZE, SIZE, SIZE);

#ifdef USE_ROOTFS_TAR
    size_t tar_size = (size_t)(_binary_rootfs_tar_end - _binary_rootfs_tar_start);
    printf("[rtems] Extracting tar archive (%zu bytes)...\n", tar_size);
    if (Untar_FromMemory((void *)_binary_rootfs_tar_start, tar_size) != 0) {
        perror("[rtems] Untar FAILED");
        exit(1);
    }

    /* Untar_FromMemory puo' ritornare 0 anche senza aver estratto nulla:
     * verifica davvero che il file di input esista e non sia vuoto. */
    struct stat st;
    if (stat(input_path, &st) != 0) {
        printf("[rtems] ERRORE: input mancante: %s (%s)\n", input_path, strerror(errno));
#ifdef FS_DEBUG
        list_tree("/", 0);
#endif
        exit(1);
    }
    if (st.st_size <= 0) {
        printf("[rtems] ERRORE: input vuoto: %s\n", input_path);
        exit(1);
    }
    printf("[rtems] Input OK: %s (%ld bytes)\n", input_path, (long)st.st_size);
#endif

    char *argv[] = {
        "obpmark_data_compression",
        "-s", ns_str,        /* numero di campioni = SIZE*SIZE */
        "-n", n_str,
        "-j", j_str,
        "-r", r_str,
        "-f", input_path,
        "-t",                /* come nello script di default (niente file di output) */
        NULL
    };
    struct bench_args args = { (int)(sizeof(argv) / sizeof(argv[0])) - 1, argv };

#if defined(OPENMP) || defined(_OPENMP)
    omp_set_num_threads(NUM_THREADS);
#endif

    bench_print_config("compression", BENCH_IMPL, SIZE);
    printf("[bench] PARAMS   samples=%d r=%d j=%d n=%d\n",
           NUM_SAMPLES, RSAMPLES, JBLOCK, NBITS);

#if (defined(OPENMP) || defined(_OPENMP)) && defined(OMP_DIAG)
    printf("[diag] MACRO: NUM_THREADS=%d NUM_CORES=%d num_procs=%d max_threads=%d\n",
           NUM_THREADS, NUM_CORES, omp_get_num_procs(), omp_get_max_threads());
    omp_diag_run(NUM_THREADS);
#endif

    bench_print_topology();

    int rc = bench_run("compression", SIZE, run_once, &args,
                       NUM_SAMPLES, "sample");
    bench_print_end("compression", rc);

    if (gomp_report) gomp_report();
#ifdef STACK_CHECK
    rtems_stack_checker_report_usage();
#endif

    exit(rc);
    return NULL;
}

/* Override per data compression: stack piu' piccoli, IMFS per l'input da tar */
#define BENCH_MIN_STACK   (2 * 1024U)
#define BENCH_INIT_STACK  (8 * 1024U)
#define BENCH_IDLE_STACK  (2 * 1024U)
#define BENCH_ISR_STACK   (2 * 1024U)
#define BENCH_MAX_FDS     16
#define CONFIGURE_USE_IMFS_AS_BASE_FILESYSTEM

#include "bench_rtems_config.h"