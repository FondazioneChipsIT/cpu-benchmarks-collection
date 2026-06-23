#include "libtarg.h"
#include "libmin.h"


#ifndef STACK_SIZE
#define STACK_SIZE (64*1024)
#endif
#ifndef MAX_HEAP
#define MAX_HEAP (2*1024*1024)
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

#define UART_BASE 0x3002000UL
#define UART_THR  0x0UL

#define UART_THR_OFF     (0  * 4)   /* Transmit Holding Register */
#define UART_IER_OFF     (1  * 4)   /* Interrupt Enable */
#define UART_FCR_OFF     (2  * 4)   /* FIFO Control */
#define UART_LCR_OFF     (3  * 4)   /* Line Control */
#define UART_MCR_OFF     (4  * 4)   /* Modem Control */
#define UART_LSR_OFF     (5  * 4)   /* Line Status */
#define UART_LSR_THRE    (1u << 5)  /* THR Empty — pronto per scrivere */
#define UART_LSR_TEMT    (1u << 6)  /* Transmitter Empty — tutto trasmesso */

#define CHESHIRE_REGS_BASE  0x3000000UL
#define CHESHIRE_SCRATCH2   (CHESHIRE_REGS_BASE + 8UL)

/* Accesso ai registri UART — uint32_t perché reg-shift=2 */
static inline volatile uint32_t *uart_reg(uint32_t off) {
    return (volatile uint32_t *)(UART_BASE + off);
}

/* Clock UART:
 *   FPGA:      50  MHz → divisore = 50000000  / (16 * 115200) = 27
 *   QuestaSim: 200 MHz → divisore = 200000000 / (16 * 115200) = 108
 */
#ifdef TARGET_CHESHIRE_SIMULATOR
#define UART_DIVISOR 108
#else
#define UART_DIVISOR 27
#endif

/* ============================================================
 * UART initialization
 * Must be called ONCE before any write operation
 * ============================================================ */
void uart_init_cheshire(void) {
    /* 1. Disable all interrupts */
    *uart_reg(UART_IER_OFF) = 0x00;

    /* 2. Enable DLAB to configure the baud rate divisor */
    *uart_reg(UART_LCR_OFF) = 0x80;

    /* 3. Write divisor: DLL shares the same offset as THR when DLAB=1, DLM shares with IER */
    *uart_reg(UART_THR_OFF) = UART_DIVISOR;   /* DLL (LSB) */
    *uart_reg(UART_IER_OFF) = 0;              /* DLM = 0 */

    /* 4. Disable DLAB, configure 8N1 (8 data bits, no parity, 1 stop bit) */
    *uart_reg(UART_LCR_OFF) = 0x03;

    /* 5. Enable FIFO, reset TX and RX FIFO, set threshold to 14 bytes */
    *uart_reg(UART_FCR_OFF) = 0xC7;

    /* 6. MCR: enable autoflow control matching the official Cheshire uart.c driver */
    *uart_reg(UART_MCR_OFF) = 0x20;
}
   
extern inline void
_mmio_write32(uintptr_t addr, uint32_t value)
{
  *(volatile uint32_t *)addr = value;
}

static int _cheshire_putchar(char c) {
/* Wait until the Transmit Holding Register (THR) is ready (LSR bit 5 set) */
    while ((*uart_reg(UART_LSR_OFF) & UART_LSR_THRE) == 0)
        ;
    *uart_reg(UART_THR_OFF) = (uint32_t)(unsigned char)c;
    return c;
}

void _cheshire_exit(int code) __attribute__((noreturn));

void _cheshire_exit(int code) {
    volatile uint32_t *scratch2 = (volatile uint32_t *)CHESHIRE_SCRATCH2;

    volatile int timeout = 10000000;
    while (((*uart_reg(UART_LSR_OFF) & UART_LSR_TEMT) == 0) && timeout > 0)
        timeout--;

    *scratch2 = (code << 1) | 1;
    while(1) __asm__ volatile("wfi");
}

__attribute__((used, noinline))
void * memset(void *dest, int val, size_t len)
{
  return libmin_memset(dest, val, len);
}

__attribute__((used, noinline))
void * memcpy(void *dest, const void *src, size_t len)
{
  return libmin_memcpy(dest, src, len);
}

// uint8_t _boot_stack[64*1024] __attribute__((aligned(16)));  
 
/* ============================================================
 * Helpers per exception handler
 * ============================================================ */

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

// OUT OF TARGET SPECIFIC BLOCK

// OUT OF TARGET SPECIFIC BLOCK

/* benchmark completed successfully */
__attribute__((noreturn)) void libtarg_success(void) {
#ifdef MEASURE_MEMORY
    uint32_t stack_used = stack_canary_measure();
    uint32_t heap_used  = __heap_peak;
    /* Stampa misure su UART come commento dopo l'output normale */
    libmin_printf("# STACK_USED: %u bytes\n", stack_used);
    libmin_printf("# HEAP_USED:  %u bytes\n", heap_used);
#endif
    _cheshire_exit(0);
}

/*
__attribute__((noreturn)) void
libtarg_success(void)
{
  _cheshire_exit(0);
}
  */

/* benchmark completed with error CODE */
__attribute__((noreturn)) void
libtarg_fail(int code)
{
  _cheshire_exit(code);
}

/* output a single character, to wherever the target wants to send it... */
void libtarg_putc(char c)
{
    _cheshire_putchar(c);
}


// SPM heap configuration
//#define MAX_HEAP (16*1024)  // 32KB heap

// Align the base buffer for memory safety and cleanliness
// static uint8_t __heap[MAX_HEAP] __attribute__((aligned(16)));
static uint32_t __heap_ptr = 0;

// test:

/* Stack canary — rileva high watermark dello stack */


#ifdef MEASURE_MEMORY
void stack_canary_init(void) {
    /* Lo stack cresce dall'alto verso il basso.
     * sp parte da _boot_stack + STACK_SIZE.
     * Inizializziamo tutta la zona bassa con il pattern —
     * lasciando 256 byte in cima per il frame corrente. */
    uint64_t *p   = (uint64_t *)_boot_stack;
    uint64_t *end = (uint64_t *)(_boot_stack + STACK_SIZE - 256);
    while (p < end) *p++ = STACK_CANARY_PATTERN;
}

uint32_t stack_canary_measure(void) {
    /* Conta quanti byte dal BASSO sono ancora intatti (= non usati).
     * Lo stack usato = STACK_SIZE - byte_intatti */
    uint64_t *p   = (uint64_t *)_boot_stack;
    uint64_t *end = (uint64_t *)(_boot_stack + STACK_SIZE);
    uint32_t intact = 0;
    while (p < end && *p == STACK_CANARY_PATTERN) {
        p++;
        intact += 8;
    }
    /* Byte effettivamente usati dallo stack */
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



/*
void *
libtarg_sbrk(size_t inc)
{
  // Keep __heap_ptr 8-byte aligned for RV64 cores
  // ld and sd instructions require 8-byte aligned memory addresses
  __heap_ptr = (__heap_ptr + 7u) & ~7u;

  uint32_t old_ptr = __heap_ptr;

  if (inc == 0) {
    return &__heap[old_ptr];
  }

  if (old_ptr + inc > MAX_HEAP) {
    libtarg_fail(1);
  }

  __heap_ptr = old_ptr + inc;
  return &__heap[old_ptr];
}
  */