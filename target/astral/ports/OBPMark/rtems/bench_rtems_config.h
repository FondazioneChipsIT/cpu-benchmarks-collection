/*
 * bench_rtems_config.h - configurazione RTEMS comune ai tre init (AES, image, compression)
 *
 * Va incluso UNA volta, in fondo all'init, dopo POSIX_Init. Definisce i CONFIGURE_*
 * e include <rtems/confdefs.h> (con CONFIGURE_INIT).
 *
 * Override per benchmark: definire PRIMA dell'include una o piu' di
 *   BENCH_MIN_STACK, BENCH_INIT_STACK, BENCH_IDLE_STACK, BENCH_ISR_STACK,
 *   BENCH_MAX_FDS, BENCH_MAX_TASKS, BENCH_MAX_SEMS
 * e gli eventuali CONFIGURE_* specifici (es. CONFIGURE_USE_IMFS_AS_BASE_FILESYSTEM
 * per data compression con USE_ROOTFS_TAR).
 *
 * GOMP_WORKER_STACK deve essere uguale a quello di mini_gomp.c: passalo come
 * -DGOMP_WORKER_STACK=... a entrambi dal Makefile. Il valore usato davvero a
 * runtime e' nella riga "[gomp] pool_init OK: ... stack=".
 */
#ifndef BENCH_RTEMS_CONFIG_H
#define BENCH_RTEMS_CONFIG_H

#include <rtems.h>

#if NUM_CORES > 1
#ifndef RTEMS_SMP
#error "RTEMS non compilato con SMP, ma NUM_CORES > 1 (richiesto RTEMS_SMP = True nel config.ini)"
#endif
#endif

#ifndef NUM_CORES
#define NUM_CORES 1
#endif

/* Stack del worker di mini_gomp (stesso valore di mini_gomp.c) */
#ifndef GOMP_WORKER_STACK
#define GOMP_WORKER_STACK (16 * 1024)
#endif

/* Default comuni (override: definire prima dell'include) */
#ifndef BENCH_MIN_STACK
#define BENCH_MIN_STACK   (4 * 1024U)
#endif
#ifndef BENCH_INIT_STACK
#define BENCH_INIT_STACK  (4 * 1024U)
#endif
#ifndef BENCH_IDLE_STACK
#define BENCH_IDLE_STACK  (4 * 1024U)
#endif
#ifndef BENCH_ISR_STACK
#define BENCH_ISR_STACK   (4 * 1024U)
#endif
#ifndef BENCH_MAX_FDS
#define BENCH_MAX_FDS     8
#endif
#ifndef BENCH_MAX_TASKS
#define BENCH_MAX_TASKS   2     /* worker WKR1 (+1 di margine) */
#endif
#ifndef BENCH_MAX_SEMS
#define BENCH_MAX_SEMS    2     /* sem_start + sem_done */
#endif

#define CONFIGURE_APPLICATION_NEEDS_CONSOLE_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_CLOCK_DRIVER

#define CONFIGURE_POSIX_APPLICATION
#define CONFIGURE_POSIX_INIT_THREAD_TABLE

/* NUM_CORES guida i processori; con 1 core resta lo scheduler di default */
// #define CONFIGURE_MAXIMUM_PROCESSORS NUM_CORES
// #if NUM_CORES > 1
//   #define CONFIGURE_SCHEDULER_SIMPLE_SMP
//   #include <rtems/scheduler.h>
// #endif
#define CONFIGURE_MAXIMUM_PROCESSORS NUM_CORES
#ifdef RTEMS_SMP
  #define CONFIGURE_SCHEDULER_SIMPLE_SMP
  #include <rtems/scheduler.h>
#endif

/* Workspace piccolo (L2): solo cio' che serve a mini_gomp */
#define CONFIGURE_MAXIMUM_POSIX_THREADS     1
#define CONFIGURE_MAXIMUM_TASKS             BENCH_MAX_TASKS
#define CONFIGURE_MAXIMUM_SEMAPHORES        BENCH_MAX_SEMS
#define CONFIGURE_MAXIMUM_FILE_DESCRIPTORS  BENCH_MAX_FDS

#define CONFIGURE_MINIMUM_TASK_STACK_SIZE      BENCH_MIN_STACK
#define CONFIGURE_POSIX_INIT_THREAD_STACK_SIZE BENCH_INIT_STACK
#define CONFIGURE_IDLE_TASK_STACK_SIZE         BENCH_IDLE_STACK
#define CONFIGURE_INTERRUPT_STACK_SIZE         BENCH_ISR_STACK

/*
 * Lo stack del worker (GOMP_WORKER_STACK) non e' contato da MAXIMUM_TASKS x
 * MINIMUM_TASK_STACK_SIZE: va riservato a parte nel workspace. Se compare
 * INTERNAL_ERROR_TOO_LITTLE_WORKSPACE all'avvio, abbassa GOMP_WORKER_STACK
 * (o allarga la regione L2).
 */
#define CONFIGURE_EXTRA_TASK_STACKS            (GOMP_WORKER_STACK)

#ifdef STACK_CHECK
  #define CONFIGURE_STACK_CHECKER_ENABLED
#endif

#define CONFIGURE_INIT
#include <rtems/confdefs.h>

#endif /* BENCH_RTEMS_CONFIG_H */