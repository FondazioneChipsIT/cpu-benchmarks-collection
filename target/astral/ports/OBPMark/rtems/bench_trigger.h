/*
 * bench_trigger.h - segnale GPIO di trigger per le misure di potenza (Otii GPI 1)
 *
 * Il pin va alto durante ogni run MISURATA di bench_run e resta basso altrove
 * (warm-up compreso): nel grafico di Otii ogni livello alto e' una run.
 *
 * Nessun include del software baremetal: i registri sono scritti a indirizzo fisso.
 *
 * Attivazione (Makefile):
 *   -DBENCH_TRIGGER                 abilita il trigger (senza, le funzioni sono vuote)
 *   -DBENCH_PAD_SETUP               imposta dal firmware il mux del pad -> GPIO
 *   -DBENCH_GPIO_BASE=0x03005000UL  base del GPIO (default)
 *   -DBENCH_GPIO_BIT=0              bit del GPIO collegato al pad (default 0)
 *
 * Con BENCH_PAD_SETUP servono 4 valori, presi da car_memory_map.h:
 *   -DBENCH_PADFRAME_BASE=...   = PADFRAME_BASE_ADDRESS
 *   -DBENCH_PAD_MUX_OFF=...     = PADFRAME_CONFIG_MUXED_V_00_MUX_SEL
 *   -DBENCH_PAD_MUX_GPIO=...    = PADFRAME_MUXED_V_00_SEL_GPIO_IO_V_0
 *   -DBENCH_PAD_CFG_OFF=...     = PADFRAME_CONFIG_MUXED_V_00_CFG
 * La sequenza replica padframe.h: scrive l'id GPIO nel registro di mux, poi
 * nel registro CFG mette a 1 il bit 3 (pen) e il bit 4 (psel), cioe' pull-up.
 *
 * Registri del GPIO (come nel test baremetal di toggle):
 *   DIRECT_OUT = base + 0x14  (valore del pin)
 *   DIRECT_OE  = base + 0x20  (1 = bit configurato come uscita)
 */
#ifndef BENCH_TRIGGER_H
#define BENCH_TRIGGER_H

#include <stdint.h>

#ifdef BENCH_TRIGGER

  #ifndef BENCH_GPIO_BASE
  #define BENCH_GPIO_BASE 0x03005000UL
  #endif
  #ifndef BENCH_GPIO_BIT
  #define BENCH_GPIO_BIT 0
  #endif

  #define BENCH_GPIO_DIRECT_OUT 0x14
  #define BENCH_GPIO_DIRECT_OE  0x20
  #define BENCH_GPIO_REG(off)   (*(volatile uint32_t *)((BENCH_GPIO_BASE) + (off)))

  #ifdef BENCH_PAD_SETUP
    #if !defined(BENCH_PADFRAME_BASE) || !defined(BENCH_PAD_MUX_OFF) || \
        !defined(BENCH_PAD_MUX_GPIO)  || !defined(BENCH_PAD_CFG_OFF)
      #error "BENCH_PAD_SETUP richiede BENCH_PADFRAME_BASE, BENCH_PAD_MUX_OFF, BENCH_PAD_MUX_GPIO, BENCH_PAD_CFG_OFF (vedi car_memory_map.h)"
    #endif
    #define BENCH_PAD_REG(off) (*(volatile uint32_t *)((BENCH_PADFRAME_BASE) + (off)))
  #endif

  /* Da chiamare una volta, prima della prima run. */
  static inline void bench_trigger_init(void)
  {
  #ifdef BENCH_PAD_SETUP
      BENCH_PAD_REG(BENCH_PAD_MUX_OFF) = (uint32_t)(BENCH_PAD_MUX_GPIO);
      BENCH_PAD_REG(BENCH_PAD_CFG_OFF) = (BENCH_PAD_REG(BENCH_PAD_CFG_OFF) & ~0x18u) | 0x18u;
  #endif
      BENCH_GPIO_REG(BENCH_GPIO_DIRECT_OUT) = 0u;                       /* parte basso */
      BENCH_GPIO_REG(BENCH_GPIO_DIRECT_OE)  = (1u << BENCH_GPIO_BIT);   /* uscita */
  }

  /* v != 0 -> pin alto, v == 0 -> pin basso. Il fence fa uscire la scrittura
   * prima di proseguire, cosi' il fronte non scivola dentro la zona cronometrata. */
  static inline void bench_trigger(int v)
  {
      BENCH_GPIO_REG(BENCH_GPIO_DIRECT_OUT) = v ? (1u << BENCH_GPIO_BIT) : 0u;
      __asm__ volatile ("fence" ::: "memory");
  }

#else /* trigger disattivato: nessun effetto sulle misure */

  static inline void bench_trigger_init(void) {}
  static inline void bench_trigger(int v) { (void)v; }

#endif /* BENCH_TRIGGER */

#endif /* BENCH_TRIGGER_H */