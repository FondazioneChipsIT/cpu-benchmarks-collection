#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <rtems.h>
#include <rtems/untar.h>

// #if defined(OPENMP) || defined(_OPENMP)
//   #include <omp.h>
// #endif

// #ifdef RTEMS_SMP
//   #warning "RTEMS 7 SMP attivo nel kernel!"
// #else
//   #error "RTEMS 7 NON e' stato compilato con RTEMS_SMP = True nel config.ini!"
// #endif

#ifndef NUM_THREADS
#define NUM_THREADS 1
#endif

#ifndef NUM_CORES
#define NUM_CORES 1
#endif

/* Simboli generati da objcopy su rootfs.tar */
extern char _binary_rootfs_tar_start[];
extern char _binary_rootfs_tar_end[];

/* Dichiarata in main_wrapper.cpp */
extern int run_benchmark(int argc, char **argv);

void *POSIX_Init(void *arg)
{

    (void)arg;

    size_t tar_size = (size_t)(_binary_rootfs_tar_end - _binary_rootfs_tar_start);
    printf("[rtems] Extracting tar archive (%zu bytes)...\n", tar_size);

    int rc = Untar_FromMemory((void *)_binary_rootfs_tar_start, tar_size);
    if (rc != 0) {
        perror("[rtems] Untar_FromMemory FAILED");
        exit(1);
    }
    printf("[rtems] Tar extraction OK\n");

    /* Test con path CORRETTO (con 128/) */
    
    /* Debug: verifica dimensione file estratto nell IMFS */
    struct stat st;
    if (stat("data/input_data/3.1-aes_encryption/128/3.1-aes_encryption-data_1048576.bin", &st) == 0) {
        printf("[rtems] STAT DEBUG: file size in IMFS = %ld bytes\n", (long)st.st_size);
    } else {
        perror("[rtems] STAT DEBUG: stat failed");
    }

    FILE *f = fopen("data/input_data/3.1-aes_encryption/128/3.1-aes_encryption-key_128.bin", "rb");
    if (!f) {
        perror("[rtems] TEST: key file non trovato");
    } else {
        printf("[rtems] TEST: key file OK\n");
        fclose(f);
    }

    /* Argv: path assoluto per evitare problemi di CWD */
    char *argv[] = {
        "obpmark_aes",
        "-l", "4096", // DATA_LENGTH  ridotto da 1048576 a 4096 per ridurre footprint RAM
        "-k", "128",     // KEY_LENGTH
        "-r",            // random data
        "-t",            // no generate output file
        NULL
    };
    int argc = 7;

    // #if defined(OPENMP) || defined(_OPENMP)
    // omp_set_dynamic(0);
    // omp_set_num_threads(NUM_THREADS);

    // printf("[diag] NUM_THREADS=%d NUM_CORES=%d num_procs=%d max_threads=%d dynamic=%d thread_limit=%d\n",
    //    NUM_THREADS, NUM_CORES,
    //    omp_get_num_procs(), omp_get_max_threads(),
    //    omp_get_dynamic(), omp_get_thread_limit());

    // #pragma omp parallel
    // {
    //     printf("[diag] team_size_reale=%d tid=%d core_fisico=%u proc_bind=%d\n",
    //        omp_get_num_threads(), omp_get_thread_num(),
    //        rtems_scheduler_get_processor(), omp_get_proc_bind());
    // }
    // #endif

    printf("[rtems] Starting OBPMark AES...\n");
    rc = run_benchmark(argc, argv);
    printf("[rtems] OBPMark AES done, rc=%d\n", rc);

        Heap_Information_block info;                    /* tipo corretto */
    rtems_workspace_get_information(&info);         /* dichiarata in rtems/rtems/support.h via rtems.h */
    printf("Workspace used:  %zu bytes\n", info.Used.total);
    printf("Workspace free:  %zu bytes\n", info.Free.total);
    exit(rc);
    
    return NULL;
}



// /* ============================================================
//  * Configurazione RTEMS
//  * ============================================================ */
// #define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
// #define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER

// /* POSIX */
// #define CONFIGURE_POSIX_APPLICATION
// #define CONFIGURE_POSIX_INIT_THREAD_TABLE

// /* Filesystem */
// #define CONFIGURE_USE_IMFS_AS_BASE_FILESYSTEM
// #define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 32

// // /* SMP */
// // #define CONFIGURE_MAXIMUM_PROCESSORS NUM_CORES

// /* Stack */
// #define CONFIGURE_INIT_TASK_STACK_SIZE (64 * 1024)

// /* 
//  * Memoria e Heap:
//  * CONFIGURE_UNIFIED_WORK_AREAS unifica l'heap applicativo e il workspace RTEMS.
//  * RTEMS calcola automaticamente lo spazio necessario per i task e le chiavi POSIX.
//  */
// #define CONFIGURE_IMFS_MEMFILE_BYTES_PER_BLOCK 1024 
// #define CONFIGURE_UNIFIED_WORK_AREAS

// // /* Risorse POSIX richieste da OpenMP (libgomp) */
// // #define CONFIGURE_MAXIMUM_POSIX_KEYS              16
// // #define CONFIGURE_MAXIMUM_POSIX_KEY_VALUE_PAIRS    64
// // #define CONFIGURE_MAXIMUM_POSIX_THREADS           16

// /* Consenti la gestione dinamica degli oggetti se superano le stime */
// #define CONFIGURE_UNLIMITED_OBJECTS
// #define CONFIGURE_UNLIMITED_ALLOCATION_SIZE       16

// #define CONFIGURE_INIT
// #include <rtems/confdefs.h>


#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER

#define CONFIGURE_POSIX_APPLICATION
#define CONFIGURE_POSIX_INIT_THREAD_TABLE
#define CONFIGURE_MAXIMUM_POSIX_THREADS   1

#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 4

#define CONFIGURE_MAXIMUM_TASKS 1

#define CONFIGURE_INIT_TASK_STACK_SIZE      (16 * 1024U)
#define CONFIGURE_MINIMUM_TASK_STACK_SIZE   (2 * 1024U)
#define CONFIGURE_IDLE_TASK_STACK_SIZE      (2 * 1024U)
#define CONFIGURE_INTERRUPT_STACK_SIZE      (4 * 1024U)

#define CONFIGURE_EXECUTIVE_RAM_SIZE        (10 * 1024U)
#define CONFIGURE_MAXIMUM_PRIORITY 31

#define CONFIGURE_IMFS_MEMFILE_BYTES_PER_BLOCK 1024
#define CONFIGURE_UNIFIED_WORK_AREAS

#define CONFIGURE_INIT
#include <rtems/confdefs.h>

