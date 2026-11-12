/*
 * mini_gomp.c - minimal libgomp replacement for dual-core RTEMS SMP
 *
 * Design choices:
 *  - A single PERSISTENT worker (created at the first parallel region); no
 *    task_create/task_delete/wake_after on every GOMP_parallel call.
 *  - Synchronization with two semaphores (start / done): no busy-waiting,
 *    and the semaphore release acts as a memory barrier (the 'team' data
 *    written by the main thread is visible to the worker).
 *  - Per-thread ID and thread count live in TLS (no more comparison
 *    against team.main_id).
 *  - NESTED parallel regions are serialized (default OpenMP behavior,
 *    max-active-levels = 1): a GOMP_parallel called inside a parallel
 *    region runs fn() with a team of 1 thread, without touching global state.
 *
 * Limits: max 2 threads (main + 1 worker), only one active parallel region
 * at a time, GOMP_barrier not implemented (not needed for "parallel for"
 * with static schedule and no explicit nowait/barrier).
 */
#include <rtems.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <sys/cpuset.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GOMP_WORKER_STACK
#define GOMP_WORKER_STACK (16 * 1024)   /* RTEMS_MINIMUM_STACK_SIZE is too small */
#endif

#ifdef GOMP_DEBUG
#define GOMP_LOG(...) do { printf(__VA_ARGS__); fflush(stdout); } while (0)
#else
#define GOMP_LOG(...) do { } while (0)
#endif

typedef void (*gomp_fn_t)(void *);

/* ---------- per-thread state (TLS) ---------- */
static __thread int tls_tid      = 0;   /* omp_get_thread_num()  */
static __thread int tls_nthreads = 1;   /* omp_get_num_threads() */
static __thread int tls_level    = 0;   /* parallel region nesting depth */

/* ---------- global state ---------- */
static int current_num_threads = 2;   

static rtems_id worker_id     = 0;
static rtems_id sem_start     = 0;   /* main  -> worker: "there is work"  */
static rtems_id sem_done      = 0;   /* worker -> main: "I am done"       */
static bool     pool_ready    = false;
static bool     pool_failed   = false;

/* Work descriptor: written by the main thread BEFORE release(sem_start),
 * read by the worker AFTER obtain(sem_start). */
static struct {
    gomp_fn_t fn;
    void     *data;
    int       nthreads;
} team;   /* the message that main passes to the worker */

static inline int get_max_procs(void) {
    uint32_t n = rtems_configuration_get_maximum_processors();
    return (n > 0) ? (int)n : 1;
}

/* ---------- persistent worker ---------- */
static rtems_task worker_body(rtems_task_argument arg) {
    (void)arg;
    for (;;) {
        rtems_semaphore_obtain(sem_start, RTEMS_WAIT, RTEMS_NO_TIMEOUT);

        tls_tid      = 1;
        tls_nthreads = team.nthreads;
        tls_level    = 1;

        GOMP_LOG("[mini_gomp] worker on cpu %u, nthreads=%d\n",
                 (unsigned)rtems_scheduler_get_processor(), team.nthreads);

        team.fn(team.data);

        tls_level    = 0;
        tls_nthreads = 1;
        tls_tid      = 0;

        rtems_semaphore_release(sem_done);
    }
}

/* Creates the semaphores and the worker exactly once. Returns true if the pool is usable. */
/* Error log: ALWAYS enabled, does not depend on GOMP_DEBUG */
#define GOMP_FAIL(what, sc) \
    printf("[gomp] pool_init: %s FALLITO sc=%d (prio=%d stack=%d)\n", \
           (what), (int)(sc), (int)prio, (int)GOMP_WORKER_STACK)

static bool pool_init(void) {
    rtems_status_code   sc;
    rtems_task_priority prio = 0;

    if (pool_ready)  return true;
    if (pool_failed) return false;

    /* 1. Semaphores */
    sc = rtems_semaphore_create(rtems_build_name('G','S','T','A'), 0,
                                RTEMS_COUNTING_SEMAPHORE, 0, &sem_start);
    if (sc != RTEMS_SUCCESSFUL) { GOMP_FAIL("sem_start create", sc); goto fail; }

    sc = rtems_semaphore_create(rtems_build_name('G','D','O','N'), 0,
                                RTEMS_COUNTING_SEMAPHORE, 0, &sem_done);
    if (sc != RTEMS_SUCCESSFUL) {
        GOMP_FAIL("sem_done create", sc);
        rtems_semaphore_delete(sem_start);
        goto fail;
    }

    /* 2. Caller's priority (RTEMS_CURRENT_PRIORITY = read without changing it) */
    sc = rtems_task_set_priority(RTEMS_SELF, RTEMS_CURRENT_PRIORITY, &prio);
    if (sc != RTEMS_SUCCESSFUL || prio == 0) {
        printf("[gomp] pool_init: lettura priorita' sc=%d prio=%d -> uso 10\n",
               (int)sc, (int)prio);
        prio = 10;
    }

    /* 3. Worker task */
    sc = rtems_task_create(rtems_build_name('W','K','R','1'), prio,
                           GOMP_WORKER_STACK,
                           RTEMS_DEFAULT_MODES, RTEMS_DEFAULT_ATTRIBUTES,
                           &worker_id);
    if (sc != RTEMS_SUCCESSFUL) {
        GOMP_FAIL("task_create", sc);
        rtems_semaphore_delete(sem_done);
        rtems_semaphore_delete(sem_start);
        goto fail;
    }

    /* 4. Affinity: with Simple SMP it may fail, this is not fatal. */
    {
        cpu_set_t s;
        CPU_ZERO(&s); CPU_SET(1, &s);
        sc = rtems_task_set_affinity(worker_id, sizeof(s), &s);
        printf("[gomp] affinity worker->cpu1 sc=%d\n", (int)sc);

        CPU_ZERO(&s); CPU_SET(0, &s);
        sc = rtems_task_set_affinity(RTEMS_SELF, sizeof(s), &s);
        printf("[gomp] affinity main->cpu0   sc=%d\n", (int)sc);
    }

    /* 5. Start */
    sc = rtems_task_start(worker_id, worker_body, 0);
    if (sc != RTEMS_SUCCESSFUL) {
        GOMP_FAIL("task_start", sc);
        rtems_task_delete(worker_id);
        rtems_semaphore_delete(sem_done);
        rtems_semaphore_delete(sem_start);
        goto fail;
    }

    printf("[gomp] pool_init OK: worker prio=%d stack=%d\n",
           (int)prio, (int)GOMP_WORKER_STACK);
    pool_ready = true;
    return true;

fail:
    pool_failed = true;
    return false;
}

/* ---------- #pragma omp parallel ---------- */
void GOMP_parallel(gomp_fn_t fn, void *data, unsigned num_threads, unsigned flags) {
    (void)flags;

    /* A: Nested region: serialize, without touching global state. */
    if (tls_level > 0) {
        int st = tls_tid, sn = tls_nthreads;
        tls_tid = 0; tls_nthreads = 1; tls_level++;
        fn(data);
        tls_level--; tls_nthreads = sn; tls_tid = st;
        return;
    }

    int nthreads  = (num_threads > 0) ? (int)num_threads : current_num_threads;
    int max_cores = get_max_procs();
    if (nthreads > max_cores) nthreads = max_cores;
    if (nthreads > 2)         nthreads = 2;   /* only 1 worker */

    // Mettere un GOMP_VERBOSE se si vuole stampare, altrimenti la topologia sotto rallenta la performance (printf+fflush). La topologia reale e' stampata da bench_print_topology() dopo la regione.
    // printf("[gomp] parallel: num_threads=%u cur=%d max_cores=%d -> nthreads=%d\n",
    //    num_threads, current_num_threads, max_cores, nthreads);

    // B: Create the worker and semaphores if not done yet. If it fails, fallback to serial execution.
       if (nthreads > 1 && pool_init()) {
        team.fn       = fn;
        team.data     = data;
        team.nthreads = nthreads;

        tls_tid = 0; tls_nthreads = nthreads; tls_level = 1;

        rtems_semaphore_release(sem_start);              /* wake up the worker */
        fn(data);                                        /* main = thread 0    */
        rtems_semaphore_obtain(sem_done, RTEMS_WAIT,     /* final barrier      */
                               RTEMS_NO_TIMEOUT);

        tls_level = 0; tls_nthreads = 1; tls_tid = 0;
        return;
    }

    /*C: Serial fallback */
    tls_tid = 0; tls_nthreads = 1; tls_level = 1;
    fn(data);
    tls_level = 0;
}

/* ---------- loop schedules (only used by explicit schedule clauses) ---------- */
int GOMP_loop_static_start(uint64_t start, uint64_t end, uint64_t incr,
                           uint64_t chunk_size,
                           uint64_t *istart, uint64_t *iend) {
    (void)chunk_size;
    int tid      = tls_tid;
    int nthreads = tls_nthreads > 0 ? tls_nthreads : 1;

    if (incr == 0 || end <= start) return 0;

    uint64_t total       = (end - start + incr - 1) / incr;
    uint64_t per         = (total + nthreads - 1) / nthreads;
    uint64_t chunk_start = start + (uint64_t)tid * per * incr;
    uint64_t chunk_end   = chunk_start + per * incr;
    if (chunk_end > end) chunk_end = end;

    *istart = chunk_start;
    *iend   = chunk_end;
    return (chunk_start < end);
}

int  GOMP_loop_static_next(uint64_t *istart, uint64_t *iend) { (void)istart; (void)iend; return 0; }
void GOMP_loop_end_nowait(void) {}
void GOMP_loop_end(void)        {}

/* ---------- OMP API ---------- */
void omp_set_num_threads(int n)  { if (n > 0) current_num_threads = n; }
int  omp_get_num_threads(void)   { return tls_nthreads > 0 ? tls_nthreads : 1; }
int  omp_get_thread_num(void)    { return tls_tid; }
int  omp_get_num_procs(void)     { return get_max_procs(); }
int  omp_get_max_threads(void)   { return current_num_threads; }
int  omp_in_parallel(void)       { return tls_level > 0 && tls_nthreads > 1; }
int  omp_get_level(void)         { return tls_level; }
int  omp_get_dynamic(void)       { return 0; }
void omp_set_dynamic(int v)      { (void)v; }
int  omp_get_thread_limit(void)  { return get_max_procs(); }
int  omp_get_proc_bind(void)     { return 0; }

double omp_get_wtime(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

#ifdef __cplusplus
}
#endif