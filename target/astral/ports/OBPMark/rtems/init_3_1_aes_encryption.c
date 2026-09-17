#include <stdio.h>
#include <stdlib.h>
#include <rtems.h>
#include <errno.h>


#ifndef NUM_THREADS
#define NUM_THREADS 1
#endif

#ifndef NUM_CORES
#define NUM_CORES 1
#endif

/* Dichiarata in main_wrapper.cpp */
extern int run_benchmark(int argc, char **argv);

void *POSIX_Init(void *arg)
{
    (void)arg;

    char *argv[] = {
        "obpmark_aes",
        "-l", "4096",
        "-k", "128",
        "-r",
        "-t",
        NULL
    };
    int argc = 7;

    printf("[rtems] Starting OBPMark AES...\n");
    int rc = run_benchmark(argc, argv);
    printf("[rtems] OBPMark AES done, rc=%d\n", rc);

//da togliere errno:    #include <errno.h>

    printf("errno = %d\n", errno);
    
    exit(rc);

    return NULL;
}

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER

#define CONFIGURE_POSIX_APPLICATION
#define CONFIGURE_POSIX_INIT_THREAD_TABLE
#define CONFIGURE_MAXIMUM_POSIX_THREADS   1

#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS 8
#define CONFIGURE_MAXIMUM_TASKS 0

#define CONFIGURE_INIT_TASK_STACK_SIZE      (4 * 1024U)
#define CONFIGURE_MINIMUM_TASK_STACK_SIZE   (2 * 1024U)
#define CONFIGURE_IDLE_TASK_STACK_SIZE      (2 * 1024U)
#define CONFIGURE_INTERRUPT_STACK_SIZE      (4 * 1024U)

#define CONFIGURE_EXECUTIVE_RAM_SIZE        (6 * 1024U)
#define CONFIGURE_MAXIMUM_PRIORITY 3


#define CONFIGURE_DISABLE_NEWLIB_REENTRANCY
//#define CONFIGURE_UNIFIED_WORK_AREAS

#define CONFIGURE_INIT
#include <rtems/confdefs.h>
