#include <stdio.h>
#include <stdlib.h>
#include <rtems.h>
#include <rtems/untar.h>
#include <time.h>

#if defined(OPENMP) || defined(_OPENMP)
  #include <omp.h>
#endif

#ifndef SIZE
#define SIZE 1024
#endif

#ifdef RTEMS_SMP
  #warning "RTEMS 7 SMP attivo nel kernel!"
#else
  #error "RTEMS 7 NON e' stato compilato con RTEMS_SMP = True nel config.ini!"
#endif

#ifndef NUM_THREADS
#define NUM_THREADS 2
#endif

#ifndef NUM_CORES
#define NUM_CORES 2
#endif

extern char _binary_rootfs_tar_start[];
extern char _binary_rootfs_tar_end[];
extern int run_benchmark(int argc, char **argv);

void *POSIX_Init(void *arg)
{
    (void)arg;

    size_t tar_size = (size_t)(_binary_rootfs_tar_end - _binary_rootfs_tar_start);
    printf("[rtems] Extracting tar archive (%zu bytes)...\n", tar_size);
    if (Untar_FromMemory((void *)_binary_rootfs_tar_start, tar_size) != 0) {
        perror("[rtems] Untar FAILED"); exit(1);
    }
    printf("[rtems] Tar extraction OK\n");

    /* Genera dinamicamente le stringhe per width e height basate su SIZE */
    char w_str[32];
    char h_str[32];
    snprintf(w_str, sizeof(w_str), "%d", SIZE);
    snprintf(h_str, sizeof(h_str), "%d", SIZE);

    /* Argomenti dinamici per 1.1-image */
    char *argv[] = {
        "obpmark_image",
        "-f", "8",       // IMAGE_FRAMES
        "-w", w_str,     // IMAGE_WIDTH dinamico
        "-h", h_str,     // IMAGE_HEIGHT dinamico
        "-r",            // random data
        "-t",            // no generate output file
        NULL
    };
    int argc = 9;

#if defined(OPENMP) || defined(_OPENMP)
    omp_set_dynamic(0);
    omp_set_num_threads(NUM_THREADS);

    printf("[diag] NUM_THREADS=%d NUM_CORES=%d num_procs=%d max_threads=%d dynamic=%d thread_limit=%d\n",
       NUM_THREADS, NUM_CORES,
       omp_get_num_procs(), omp_get_max_threads(),
       omp_get_dynamic(), omp_get_thread_limit());

    #pragma omp parallel
    {
        printf("[diag] team_size_reale=%d tid=%d core_fisico=%u proc_bind=%d\n",
           omp_get_num_threads(), omp_get_thread_num(),
           rtems_scheduler_get_processor(), omp_get_proc_bind());
    }
#endif

    printf("[rtems] Starting OBPMark 1.1-image (SIZE=%d)...\n", SIZE);
    int rc = run_benchmark(argc, argv);
    printf("[rtems] Done, rc=%d\n", rc);
    exit(rc);
    return NULL;
}

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER

#define CONFIGURE_POSIX_APPLICATION
#define CONFIGURE_POSIX_INIT_THREAD_TABLE
#define CONFIGURE_MAXIMUM_POSIX_THREADS     16
#define CONFIGURE_MAXIMUM_POSIX_KEYS            16
#define CONFIGURE_MAXIMUM_POSIX_KEY_VALUE_PAIRS 64


#define CONFIGURE_USE_IMFS_AS_BASE_FILESYSTEM
#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS  32
/* SMP */
#define CONFIGURE_MAXIMUM_PROCESSORS NUM_CORES
#define CONFIGURE_INIT_TASK_STACK_SIZE      (256 * 1024)
#define CONFIGURE_UNIFIED_WORK_AREAS

#define CONFIGURE_UNLIMITED_OBJECTS
#define CONFIGURE_UNLIMITED_ALLOCATION_SIZE      16

#define CONFIGURE_INIT
#include <rtems/confdefs.h>