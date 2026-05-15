#include "libtarg.h"
#include "libmin.h"

#if defined(TARGET_CHESHIRE)

#define UART_BASE 0x3002000UL
#define UART_THR  0x0UL

extern inline void
_mmio_write32(uintptr_t addr, uint32_t value)
{
  *(volatile uint32_t *)addr = value;
}

extern inline int
_cheshire_putchar(char c)
{
    volatile uint32_t *lsr = (volatile uint32_t *)(UART_BASE + 5 * 4);
    volatile uint32_t *thr = (volatile uint32_t *)(UART_BASE + 0 * 4);

    // Aspetta che THR sia vuoto (bit 5)
    while (((*lsr) & (1u << 5)) == 0)
        ;

    *thr = (uint32_t)c;

    return c;
}

#define CHESHIRE_REGS_BASE  0x3000000UL
#define CHESHIRE_SCRATCH2   (CHESHIRE_REGS_BASE + 8UL)

void _cheshire_exit(int code) __attribute__((noreturn));

void _cheshire_exit(int code) {
    volatile uint32_t *scratch2 = (volatile uint32_t *)CHESHIRE_SCRATCH2;
    *scratch2 = (code << 1) | 1;
    while(1) __asm__ volatile("wfi");
}
extern inline void *
memset(void *dest, int val, size_t len)
{
  return libmin_memset(dest, val, len);
}

extern inline void *
memcpy(void *dest, const void *src, size_t len)
{
  return libmin_memcpy(dest, src, len);
}

// Small local stack, since we are not using the C runtime startup files.
// NOTE: If you change stack size, you must also update the cva6-crt0.S file.
uint8_t _boot_stack[8*4096] __attribute__((aligned(16)));

extern inline uint64_t
_cheshire_get_mepc(void)
{
  uint64_t result;
  __asm__ volatile("csrr %0, mepc;" : "=r"(result));
  return result;
}

extern inline uint64_t
_cheshire_get_mcause(void)
{
  uint64_t result;
  __asm__ volatile("csrr %0, mcause;" : "=r"(result));
  return result;
}

extern inline uint64_t
_cheshire_get_mtval(void)
{
  uint64_t result;
  __asm__ volatile("csrr %0, mtval;" : "=r"(result));
  return result;
}

// GCC/Clang RISC-V interrupt attribute emits an M-mode trap epilogue.
// This handler never returns, but the attribute still gives a safe prologue.
void _cheshire_exc_handler(void) __attribute__((interrupt("machine"), aligned(16)));

void
_cheshire_exc_handler(void)
{
  libmin_printf("EXCEPTION!!!\n");
  libmin_printf("============\n");
  libmin_printf("MEPC:0x%lx, CAUSE:0x%lx, MTVAL:0x%lx\n",
                _cheshire_get_mepc(), _cheshire_get_mcause(), _cheshire_get_mtval());

  _cheshire_exit(-1);
}

void
_cheshire_timer_handler(void)
{
  libmin_printf("TIMER EXCEPTION!!!\n");

  _cheshire_exit(-1);
}

#else /* undefined target */
#error Co-simulation platform not defined, define TARGET_HOST or a target-dependent definition.
#endif

//OUT OF TARGET SPECIFIC BLOCK

/* benchmark completed successfully */
__attribute__((noreturn)) void
libtarg_success(void)
{
  _cheshire_exit(0);
}

/* benchmark completed with error CODE */
__attribute__((noreturn)) void
libtarg_fail(int code)
{
  _cheshire_exit(code);
}

/* output a single character, to wherever the target wants to send it... */
void
libtarg_putc(char c)
{
  _cheshire_putchar(c);
}

#define MAX_HEAP    (512*1024)
static uint8_t __heap[MAX_HEAP];
static uint32_t __heap_ptr = 0;

/* get some memory */
void *
libtarg_sbrk(size_t inc)
{
  uint8_t *ptr = &__heap[__heap_ptr];
  if (inc == 0)
    return ptr;
  
  __heap_ptr += inc;
  if (__heap_ptr >= MAX_HEAP)
    libtarg_fail(1);

  return ptr;
}

#ifdef TARGET_PERFHOOKS
#include <time.h>

#if defined(TARGET_HOST)
static long long __start_time = 0;

long long
__now_us(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
}
#endif /* TARGET_HOST */

void
libtarg_start_perf(void)
{
  /* optional hook for starting perfomance monitoring/instrumentation */

#if defined(TARGET_HOST)
  __start_time = __now_us();
#else
#error Build requested TARGET_PERFHOOKS, but TARGET does not define libtarg_start_perf() and libtarg_stop_perf().
#endif
}

void
libtarg_stop_perf(void)
{
  /* optional hook for starting perfomance monitoring/instrumentation */
#if defined(TARGET_HOST)
  uint64_t runtime = __now_us() - __start_time; 
  fprintf(stderr, "PERF: program ran for %lu us.\n", runtime);
#else
#error Build requested TARGET_PERFHOOKS, but TARGET does not define libtarg_start_perf() and libtarg_stop_perf().
#endif
}
#endif /* TARGET_PERFHOOKS */
