#include "libtarg.h"
#include "libmin.h"

#ifndef STACK_SIZE
#define STACK_SIZE (16*1024)
#endif
#ifndef MAX_HEAP
#define MAX_HEAP (2*1024)
#endif

uint8_t _boot_stack[STACK_SIZE] __attribute__((aligned(16)));
static uint8_t __heap[MAX_HEAP]  __attribute__((aligned(16)));

#ifdef MEASURE_MEMORY
#define STACK_CANARY_PATTERN 0xDEADBEEFDEADBEEFULL
void stack_canary_init(void);
uint32_t stack_canary_measure(void);
static uint32_t __heap_peak = 0;
#endif

#if defined(TARGET_CHESHIRE)

#include "dif/uart.h"
#include "regs/cheshire.h"
#include "params.h"
#include "util.h"

#ifdef TARGET_CHESHIRE_SIMULATOR
#define UART_FREQ 200000000UL
#else
#define UART_FREQ 50000000UL
#endif
#define UART_BAUD 115200UL

static int initialized = 0;

#define UART_BASE ((void *)0x03002000UL)

static inline void uart_init_once(void) {
    if (!initialized) {
        uart_init(UART_BASE, UART_FREQ, UART_BAUD);
        initialized = 1;
    }
}

static int _cheshire_putchar(char c) {
    uart_init_once();
    uart_write(UART_BASE, (uint8_t)c);
    return c;
}

void _cheshire_exit(int code) __attribute__((noreturn));

void _cheshire_exit(int code) {
    uart_write_flush(UART_BASE);
    volatile uint32_t *scratch2 = (volatile uint32_t *)0x03000008UL;
    *scratch2 = (code << 1) | 1;
    while (1) __asm__ volatile("wfi");
}

__attribute__((used, noinline))
void *memset(void *dest, int val, size_t len) {
    return libmin_memset(dest, val, len);
}

__attribute__((used, noinline))
void *memcpy(void *dest, const void *src, size_t len) {
    return libmin_memcpy(dest, src, len);
}

static inline uint64_t
_cheshire_get_mepc(void)
{
  uint64_t result;
  __asm__ volatile("csrr %0, mepc;" : "=r"(result));
  return result;
}

static inline uint64_t
_cheshire_get_mcause(void)
{
  uint64_t result;
  __asm__ volatile("csrr %0, mcause;" : "=r"(result));
  return result;
}

static inline uint64_t
_cheshire_get_mtval(void)
{
  uint64_t result;
  __asm__ volatile("csrr %0, mtval;" : "=r"(result));
  return result;
}

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

#else
#error Co-simulation platform not defined, define TARGET_HOST or a target-dependent definition.
#endif

__attribute__((noreturn)) void libtarg_success(void) {
#ifdef MEASURE_MEMORY
    uint32_t stack_used = stack_canary_measure();
    uint32_t heap_used  = __heap_peak;
    libmin_printf("# STACK_USED: %u bytes\n", stack_used);
    libmin_printf("# HEAP_USED:  %u bytes\n", heap_used);
#endif
    _cheshire_exit(0);
}

__attribute__((noreturn)) void
libtarg_fail(int code)
{
  _cheshire_exit(code);
}

void libtarg_putc(char c)
{
    _cheshire_putchar(c);
}

static uint32_t __heap_ptr = 0;

#ifdef MEASURE_MEMORY
void stack_canary_init(void) {
    uint64_t *p   = (uint64_t *)_boot_stack;
    uint64_t *end = (uint64_t *)(_boot_stack + STACK_SIZE - 256);
    while (p < end) *p++ = STACK_CANARY_PATTERN;
}

uint32_t stack_canary_measure(void) {
    uint64_t *p   = (uint64_t *)_boot_stack;
    uint64_t *end = (uint64_t *)(_boot_stack + STACK_SIZE);
    uint32_t intact = 0;
    while (p < end && *p == STACK_CANARY_PATTERN) {
        p++;
        intact += 8;
    }
    return STACK_SIZE - intact;
}
#endif

void *libtarg_sbrk(size_t inc) {
    __heap_ptr = (__heap_ptr + 7u) & ~7u;
    uint32_t old = __heap_ptr;

    if (inc == 0) return &__heap[old];
    if (old + (uint32_t)inc > MAX_HEAP) libtarg_fail(1);

    __heap_ptr = old + (uint32_t)inc;

#ifdef MEASURE_MEMORY
    if (__heap_ptr > __heap_peak)
        __heap_peak = __heap_ptr;
#endif

    return &__heap[old];
}