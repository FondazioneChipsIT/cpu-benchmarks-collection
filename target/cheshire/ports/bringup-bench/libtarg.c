#include "libtarg.h"
#include "libmin.h"

#if defined(TARGET_CHESHIRE)

/*
#if defined(TARGET_CHESHIRE_QUESTASIM)
#define UART_CLK_HZ 200000000UL   // QuestaSim

#elif defined(TARGET_CHESHIRE_FPGA)
#define UART_CLK_HZ 50000000UL    // FPGA

#endif

#define UART_BAUDRATE 115200
#define UART_DIVISOR (UART_CLK_HZ / (16 * UART_BAUDRATE))

*/
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

/* ============================================================
 * Inizializzazione UART
 * Deve essere chiamata UNA VOLTA prima di qualsiasi scrittura
 * ============================================================ */
void uart_init_cheshire(void) {
    /* 1. Disabilita tutti gli interrupt */
    *uart_reg(UART_IER_OFF) = 0x00;

    /* 2. Abilita DLAB per scrivere il divisore del baud rate */
    *uart_reg(UART_LCR_OFF) = 0x80;

    /* 3. Scrivi divisore: 50MHz / (115200 * 16) = 27 , se vuoi 200MHz ->*/
    *uart_reg(UART_THR_OFF) = 27;   /* DLL = 27 (LSB) — stesso offset di THR con DLAB=1 */
    *uart_reg(UART_IER_OFF) = 0;    /* DLM = 0  (MSB) — stesso offset di IER con DLAB=1 */
/*
    uint32_t div = UART_DIVISOR;
    *uart_reg(UART_THR_OFF) = div & 0xFF;        // DLL
    *uart_reg(UART_IER_OFF) = (div >> 8) & 0xFF; // DLM
*/
    /* 4. Disabilita DLAB, imposta 8N1 (8 bit dati, no parità, 1 stop bit) */
    *uart_reg(UART_LCR_OFF) = 0x03;

    /* 5. Abilita FIFO, resetta TX e RX FIFO, soglia 14 byte */
    *uart_reg(UART_FCR_OFF) = 0xC7;

    /* 6. MCR: autoflow come da uart.c ufficiale Cheshire */
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

// Small local stack, since we are not using the C runtime startup files.
// NOTE: If you change stack size, you must also update the cva6-crt0.S file.

//momentaneamente tolto
//uint8_t _boot_stack[8*4096] __attribute__((aligned(16)));  //32KB

//SPM CONFIGURATION
//uint8_t _boot_stack[32*1024] __attribute__((aligned(16)));

uint8_t _boot_stack[64*1024] __attribute__((aligned(16)));

//CAMBIATO MOMENTANEAMENTE
//uint8_t _boot_stack[256*1024] __attribute__((aligned(16)));  // 256KB
// cambiato stack per anagram
//uint8_t _boot_stack[512*1024] __attribute__((aligned(16)));  // 512KB

// In libtarg.c — massimizza lo stack
//uint8_t _boot_stack[8*1024*1024] __attribute__((aligned(16)));  // 4MB

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
void libtarg_putc(char c)
{
    _cheshire_putchar(c);
}

// per DRAM
#define MAX_HEAP    (2*1024*1024)  // 2MB per DRAM

//per SPM
//#define MAX_HEAP (32*1024)  // 32KB heap

// Allinea anche il buffer di base, per pulizia
static uint8_t __heap[MAX_HEAP] __attribute__((aligned(16)));
static uint32_t __heap_ptr = 0;

void *
libtarg_sbrk(size_t inc)
{
  // Mantieni __heap_ptr sempre allineato a 8 byte
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