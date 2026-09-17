#include <stdio.h>
#include <stdlib.h>
#include <rtems.h>
#include <rtems/malloc.h>


#ifndef SIZE
#define SIZE 32
#endif

#ifndef NUM_CORES
#define NUM_CORES 1
#endif


extern int run_benchmark(int argc, char **argv);

void *POSIX_Init(void *arg)
{
    (void)arg;

    Heap_Information_block info;
    malloc_info(&info);
    printf("[mem] free=%zu used=%zu\n",
           info.Free.total,
           info.Used.total);
    fflush(stdout);

    

    char w_str[32];
    char h_str[32];
    snprintf(w_str, sizeof(w_str), "%d", SIZE);
    snprintf(h_str, sizeof(h_str), "%d", SIZE);
    fflush(stdout);

    char *argv[] = {
        "obpmark_image",
        "-f", "2",
        "-w", w_str,
        "-h", h_str,
        "-r",
        "-t",
        NULL
    };
    int argc = 9;

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
#define CONFIGURE_MAXIMUM_POSIX_THREADS          1

#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS       3
#define CONFIGURE_MAXIMUM_TASKS                  0
#define CONFIGURE_MAXIMUM_TIMERS                 0
#define CONFIGURE_MAXIMUM_SEMAPHORES             0
#define CONFIGURE_MAXIMUM_BARRIERS               0
#define CONFIGURE_MAXIMUM_MESSAGE_QUEUES         0

#define CONFIGURE_INIT_TASK_STACK_SIZE           (8 * 1024U)
#define CONFIGURE_MINIMUM_TASK_STACK_SIZE        (2 * 1024U)
#define CONFIGURE_IDLE_TASK_STACK_SIZE           (2 * 1024U)
#define CONFIGURE_INTERRUPT_STACK_SIZE           (2 * 1024U)

//#define CONFIGURE_EXECUTIVE_RAM_SIZE             (6 * 1024U)
#define CONFIGURE_UNIFIED_WORK_AREAS
#define CONFIGURE_MAXIMUM_PRIORITY               3
#define CONFIGURE_DISABLE_NEWLIB_REENTRANCY

#define CONFIGURE_INIT
#include <rtems/confdefs.h>