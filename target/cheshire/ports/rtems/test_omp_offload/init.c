/* SPDX-License-Identifier: BSD-2-Clause */

/*
 *  COPYRIGHT (c) 1989-2012.
 *  On-Line Applications Research Corporation (OAR).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
 * RTEMS -> MAGIA (GVSoC) OpenMP offload test over the shared memory (UMA).
 *
 * RTEMS plays the HERO host (libomptarget + HERO plugin): it loads the MAGIA
 * ELF built with the HERO clang, resolves the "#pragma omp target" kernels
 * from its .symtab, prepares the mailboxes of offload_layout.h, enables the
 * MAGIA fetch and runs the target regions with the HERO mailbox protocol
 * (START / DONE / STOP), checking the map(tofrom) result.
 *
 * Build with the MAGIA test test_omp_offload embedded:
 *   make build BENCHMARK=rtems PLATFORM=cheshire \
 *       MAGIA_ELF=~/magia-sdk/build_1tile/bin/test_omp_offload
 */

#include <rtems.h>
#include <tmacros.h>

#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>
#include <rtems/counter.h>
#include <rtems/magia_elf_loader.h>
#include "ringbuf.h"
#include "offload_layout.h"


const char rtems_test_name[] = "OMP OFFLOAD";

/*
 * GVSoC maps the MAGIA L2 (0xC000_0000) on the part of the QEMU RAM seen at
 * 0xC000_0000 (UMA), so RTEMS and MAGIA use the same addresses
 * (offload_layout.h). Outside the RTEMS RAM region: requires a QEMU RAM
 * covering it (e.g. -m 2G).
 */

typedef void (*kernel_t)(uint64_t);      // firma unica degli involucri (OmpKernelWrapper)

/* MAGIA ELF embedded in the image by magia_elf.c (.incbin) */
extern const unsigned char magia_elf[];
extern const size_t magia_elf_size;

// ===========================================================================
// PARTE 1 - i kernel li ha generati il clang di HERO (kernels_omp.o, catena di
// omp_hero_kernels() in omp/hero_magia.cmake): uno per ogni #pragma omp target,
// con nome "__omp_offloading_<id>_regions_l<riga della #pragma>".
//
// Host e device sono due ELF separati: l'ELF di RTEMS non ha la tabella
// omp_offloading_entries, quindi gli indirizzi degli involucri si leggono dalla
// tabella dei simboli (.symtab) dell'ELF di MAGIA incorporato, come fa il plugin
// HERO in satisfy_requested_symbols (plugins/hero/rtl.cpp:106).
// Differenza: in HERO i nomi completi arrivano dalla tabella host generata da
// clang; qui l'host non e' compilato da clang HERO e cerchiamo per suffisso.
// ===========================================================================

// Le regioni si riconoscono dalla riga della #pragma in src/kernels_omp.c
// (il resto del nome dipende dal file e cambia se viene ricreato).
// ATTENZIONE: se si aggiungono o tolgono righe sopra le #pragma in kernels_omp.c,
// questi numeri vanno aggiornati (il test stampa i kernel trovati nell'ELF).
#define KERNEL_NOP_SUFFIX        "_l28"
#define KERNEL_MAP_TO_SUFFIX     "_l32"
#define KERNEL_MAP_TOFROM_SUFFIX "_l36"

static int print_omp_kernel(const magia_elf_sym_t *sym, void *arg)
{
    (void) arg;
    if (sym->type == STT_FUNC &&
        strncmp(sym->name, MAGIA_OMP_KERNEL_PREFIX,
                sizeof(MAGIA_OMP_KERNEL_PREFIX) - 1) == 0)
        printf("[host]   0x%08" PRIx32 " %s\n", (uint32_t)sym->value, sym->name);
    return 0;
}

static kernel_t find_kernel(const char *suffix)
{
    magia_elf_sym_t sym;
    int rv = magia_elf_find_omp_kernel(magia_elf, magia_elf_size, suffix, &sym);

    if (rv == -2) {
        printf("[host] kernel ambiguo: %s\n", suffix);
        return 0;
    }
    if (rv != 0) {
        printf("[host] kernel non trovato: %s\n", suffix);
        return 0;
    }
    // MAGIA e' a 32 bit e il kernel deve stare nella L2 caricata
    if (sym.value < MAGIA_L2_START || sym.value > MAGIA_L2_END) {
        printf("[host] kernel %s a 0x%" PRIx64 " fuori dalla L2 di MAGIA\n",
               sym.name, sym.value);
        return 0;
    }
    return (kernel_t)(uintptr_t)sym.value;
}

// In HERO: satisfy_requested_symbols (rtl.cpp). Trova gli indirizzi delle
// funzioni involucro, una per #pragma omp target.
static void resolve_kernels(kernel_t *nop, kernel_t *map_to, kernel_t *map_tofrom)
{
    printf("[host] kernel OpenMP nella .symtab di MAGIA:\n");
    if (magia_elf_foreach_symbol(magia_elf, magia_elf_size,
                                 print_omp_kernel, NULL) < 0)
        printf("[host]   nessuna .symtab (ELF MAGIA strippato?)\n");
    *nop        = find_kernel(KERNEL_NOP_SUFFIX);
    *map_to     = find_kernel(KERNEL_MAP_TO_SUFFIX);
    *map_tofrom = find_kernel(KERNEL_MAP_TOFROM_SUFFIX);
}

// ===========================================================================
// PARTE 2 - cosa fanno libomptarget + plugin HERO (lato host)
// ===========================================================================
// Riccardo: con UMA (QEMU + GVSoC) host e MAGIA vedono la L2 agli stessi indirizzi,
// quindi questi puntatori valgono anche su RTEMS (vedi offload_layout.h).
static volatile struct ring_buf *const h2a = (volatile struct ring_buf *)OFL_H2A_RING;
static volatile struct ring_buf *const a2h = (volatile struct ring_buf *)OFL_A2H_RING;

// Riccardo: ALLOCAZIONE MEMORIA. In HERO: __tgt_rtl_data_alloc (rtl.cpp) ->
// GOMP_OFFLOAD_alloc (plugin-hero-dev.cpp) -> hero_dev_l2_malloc (o1heap).
// Qui basta un allocatore che avanza e non libera.
static uintptr_t heap_next;
static uintptr_t data_alloc(uint32_t size)
{
    uintptr_t p = heap_next;
    heap_next += (size + 7u) & ~7u;
    return p;
}

// Riccardo: COPIA HOST -> DEVICE. In HERO: __tgt_rtl_data_submit -> GOMP_OFFLOAD_host2dev.
static void data_submit(uintptr_t dev, const void *host, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        ((volatile uint8_t *)dev)[i] = ((const uint8_t *)host)[i];
}

// Riccardo: COPIA DEVICE -> HOST. In HERO: __tgt_rtl_data_retrieve -> GOMP_OFFLOAD_dev2host.
static void data_retrieve(void *host, uintptr_t dev, uint32_t size)
{
    for (uint32_t i = 0; i < size; i++)
        ((uint8_t *)host)[i] = ((volatile uint8_t *)dev)[i];
}

// Attesa massima per ogni parola delle caselle. HERO aspetta senza limite
// (while (hero_dev_mbox_read(...)); in rtl.cpp); qui un timeout evita che il test
// resti appeso se MAGIA non parte o si blocca. Abbondante perche' GVSoC simula
// MAGIA molto piu' lentamente del tempo reale di QEMU. Si puo' cambiare da cflags.
#ifndef OFL_MBOX_TIMEOUT_MS
#define OFL_MBOX_TIMEOUT_MS 60000u
#endif

// Ripete op (rb_host_get/rb_host_put) finche' riesce o scade il timeout.
// Il tempo si misura con il CPU counter (mtime della CLINT), che funziona anche
// senza clock driver. Il counter e' a 32 bit: si accumulano le differenze, cosi'
// il giro del contatore non da' problemi.
static int mbox_wait(int (*op)(volatile struct ring_buf *, void *),
                     volatile struct ring_buf *rb, uint32_t *w)
{
    uint64_t limit = (uint64_t)rtems_counter_frequency() * OFL_MBOX_TIMEOUT_MS / 1000u;
    uint64_t elapsed = 0;
    rtems_counter_ticks last = rtems_counter_read();

    while ((*op)(rb, w)) {
        rtems_counter_ticks now = rtems_counter_read();
        elapsed += rtems_counter_difference(now, last);
        last = now;
        if (elapsed >= limit)
            return -1;
    }
    return 0;
}

static void print_mbox_state(void)
{
    printf("[host]   h2a head=%" PRIu32 " tail=%" PRIu32
           ", a2h head=%" PRIu32 " tail=%" PRIu32 ", ready=0x%08" PRIx32 "\n",
           h2a->prod.head, h2a->cons.tail, a2h->prod.head, a2h->cons.tail,
           *(volatile uint32_t *)OFL_READY_FLAG);
    // ready ancora OFL_READY_MAGIC: MAGIA non ha mai preso le caselle (non e' partita?)
}

// Riccardo: SCRITTURA nella casella h2a. In HERO: hero_dev_mbox_write (mailbox.c).
// rb_host_put di ringbuf.h si puo' riusare tale e quale. 0 = scritto, -1 = timeout.
static int host_mbox_write(uint32_t w)
{
    if (mbox_wait(rb_host_put, h2a, &w)) {
        printf("[host] timeout: casella h2a piena\n");
        print_mbox_state();
        return -1;
    }
    return 0;
}

// Riccardo: LETTURA dalla casella a2h. In HERO: hero_dev_mbox_read (mailbox.c).
// Bloccante come in HERO, perche' il device gira in parallelo. 0 = letto, -1 = timeout.
static int host_mbox_read(uint32_t *w)
{
    if (mbox_wait(rb_host_get, a2h, w)) {
        printf("[host] timeout: nessuna risposta nella casella a2h dopo %u ms\n",
               OFL_MBOX_TIMEOUT_MS);
        print_mbox_state();
        return -1;
    }
    return 0;
}

// Riccardo: INVIO DI UNA REGIONE. In HERO: __tgt_rtl_run_target_region (rtl.cpp:457),
// solo la parte di invio. In HERO c'e' un solo dev_arg_buf riusato a ogni offload;
// qui, dato che accodiamo tutto in anticipo, ogni regione ha il suo argbuf.
// Le parole accodate prima del fetch enable (4 per regione + STOP) devono stare
// nella casella, altrimenti host_mbox_write aspetterebbe un device non ancora partito.
#define N_REGIONS 4u
_Static_assert(4u * N_REGIONS + 1u < OFL_RING_SLOTS, "casella h2a troppo piccola");

static void run_region(void (*kernel)(uint64_t), uint32_t nargs, const uintptr_t *dev_ptrs)
{
    uint64_t *argbuf = (uint64_t *)data_alloc(8 * (nargs ? nargs : 1));
    for (uint32_t i = 0; i < nargs; i++)
        argbuf[i] = (uint64_t)dev_ptrs[i];

    // Non possono fallire: c'e' posto per tutto (vedi _Static_assert sopra)
    (void)host_mbox_write(MBOX_DEVICE_START);
    (void)host_mbox_write((uint32_t)(uintptr_t)kernel);   // indirizzo dell'involucro
    (void)host_mbox_write((uint32_t)(uintptr_t)argbuf);   // indirizzo del buffer argomenti
    (void)host_mbox_write(0);                             // n. miss handler (non usato)
}

// ---------------------------------------------------------------------------
// Il "programma host": le stesse regioni di offload_benchmark, una volta sola
// ---------------------------------------------------------------------------
static uint32_t tmp_1, tmp_2;            // le variabili "dell'host"
static uintptr_t tofrom_p1, tofrom_p2;   // dove stanno in L2 per la regione tofrom
static kernel_t k_nop, k_map_to, k_map_tofrom;
static int kernels_missing;

// Riccardo: sull'host vero questa funzione non esiste come tale. L'inizializzazione
// (cella "ready" + caselle) e' hero_dev_init / hero_dev_alloc_mboxes; le regioni le
// genera clang dalle #pragma nella parte host di kernels_omp.c (regions()) e le
// esegue libomptarget. Inoltre l'host non accoda tutto prima: invia START e aspetta
// DONE regione per regione, perche' il device gira in parallelo.
static void fake_host_before(void)
{
    // Correzione A: prima di tutto invalida la cella "ready". Se la memoria
    // condivisa sopravvive tra due esecuzioni, un flag rimasto da una volta
    // precedente non deve mai essere visto valido mentre prepariamo le caselle.
    *(volatile uint32_t *)OFL_READY_FLAG = 0;
    *(volatile uint32_t *)OFL_H2A_ADDR   = 0;
    *(volatile uint32_t *)OFL_A2H_ADDR   = 0;
    asm volatile("fence w,w" ::: "memory");

    heap_next = OFL_HEAP_BASE;

    // hero_dev_init: crea le due caselle (mailbox.c: hero_dev_alloc_mboxes)
    rb_init(h2a, OFL_RING_SLOTS, sizeof(uint32_t));
    rb_init(a2h, OFL_RING_SLOTS, sizeof(uint32_t));
    h2a->prod.data_v = h2a->prod.data_p = OFL_H2A_DATA;   // virtuale = fisico
    a2h->prod.data_v = a2h->prod.data_p = OFL_A2H_DATA;
    // Riccardo: data_v (indirizzo visto dall'host) e data_p (visto dal device) con
    // UMA coincidono anche sull'host. Sono diversi solo con un host a memoria
    // virtuale (Linux in HERO).

    // Da ogni #pragma omp target clang genera una funzione involucro con firma unica.
    resolve_kernels(&k_nop, &k_map_to, &k_map_tofrom);
    kernels_missing = !k_nop || !k_map_to || !k_map_tofrom;

    if (!kernels_missing) {
        // 1) #pragma omp target { nop }      ("enter_omp_init")
        run_region(k_nop, 0, 0);

        // 2) #pragma omp target { nop }      ("enter_omp_nop")
        run_region(k_nop, 0, 0);

        tmp_1 = 5;
        tmp_2 = 10;

        // 3) #pragma omp target map(to: tmp_1)
        {
            uintptr_t p1 = data_alloc(sizeof tmp_1);
            data_submit(p1, &tmp_1, sizeof tmp_1);          // "to": copia in L2
            run_region(k_map_to, 1, &p1);
            // "to" non ricopia indietro
        }

        // 4) #pragma omp target map(tofrom: tmp_1, tmp_2)
        {
            tofrom_p1 = data_alloc(sizeof tmp_1);
            tofrom_p2 = data_alloc(sizeof tmp_2);
            data_submit(tofrom_p1, &tmp_1, sizeof tmp_1);
            data_submit(tofrom_p2, &tmp_2, sizeof tmp_2);
            uintptr_t ptrs[2] = { tofrom_p1, tofrom_p2 };
            run_region(k_map_tofrom, 2, ptrs);
            // la ricopia ("from") si fa dopo, in fake_host_after
        }
    }

    // Se manca un kernel si invia solo STOP: il device esce subito e il test fallisce.
    (void)host_mbox_write(MBOX_DEVICE_STOP);

    // Riccardo: cella "ready" (al posto di LETTER_0/1): prima gli indirizzi, poi il
    // flag, per ULTIMO. Ordine e fence vanno mantenuti identici sull'host.
    *(volatile uint32_t *)OFL_H2A_ADDR = OFL_H2A_RING;
    *(volatile uint32_t *)OFL_A2H_ADDR = OFL_A2H_RING;
    asm volatile("fence w,w" ::: "memory");
    *(volatile uint32_t *)OFL_READY_FLAG = OFL_READY_MAGIC;
}

// Riccardo: sull'host vero la lettura dei DONE sta dentro __tgt_rtl_run_target_region
// (rtl.cpp:494-519), subito dopo ogni START, e la ricopia "from" la fa libomptarget
// alla fine di ogni regione.
static int fake_host_after(void)
{
    int errors = 0;
    uint32_t w, cycles;

    if (kernels_missing) {
        *(volatile uint32_t *)OFL_READY_FLAG = 0;
        printf("[host] TEST FAIL: kernel mancanti\n");
        return 1;
    }

    // Una risposta DONE per ogni regione (rtl.cpp:494-519): DONE, cicli, dma
    // e 4 contatori, tutte letture bloccanti.
    for (uint32_t r = 0; r < N_REGIONS; r++) {
        if (host_mbox_read(&w)) {
            printf("[host] FAIL: manca DONE per la regione %" PRIu32 "\n", r);
            *(volatile uint32_t *)OFL_READY_FLAG = 0;
            return 1;
        }
        if (w != MBOX_DEVICE_DONE) {
            printf("[host] FAIL: regione %" PRIu32 ", atteso DONE (0x%02x), letto 0x%08" PRIx32 "\n",
                   r, MBOX_DEVICE_DONE, w);
            *(volatile uint32_t *)OFL_READY_FLAG = 0;
            return 1;
        }
        if (host_mbox_read(&cycles)) {
            printf("[host] FAIL: regione %" PRIu32 ", DONE incompleto\n", r);
            *(volatile uint32_t *)OFL_READY_FLAG = 0;
            return 1;
        }
        for (uint32_t i = 2; i < MBOX_DONE_WORDS; i++) {
            if (host_mbox_read(&w)) {                   // dma + 4 contatori
                printf("[host] FAIL: regione %" PRIu32 ", DONE incompleto\n", r);
                *(volatile uint32_t *)OFL_READY_FLAG = 0;
                return 1;
            }
        }
        printf("[host] regione %" PRIu32 " DONE, cicli kernel = %" PRIu32 "\n", r, cycles);
    }

    // "from" della regione tofrom: ricopia L2 -> variabili host
    data_retrieve(&tmp_1, tofrom_p1, sizeof tmp_1);
    data_retrieve(&tmp_2, tofrom_p2, sizeof tmp_2);
    if (tmp_1 != tmp_2 || tmp_1 != 10) {
        printf("[host] FAIL: map tofrom, tmp_1 = 0x%08x\n", tmp_1);
        errors++;
    }

    *(volatile uint32_t *)OFL_READY_FLAG = 0;          // pulizia per la prossima esecuzione

    printf(errors ? "[host] TEST FAIL\n" : "[host] TEST PASS\n");
    return errors;
}



static rtems_task Init( rtems_task_argument ignored )
{
  elf_info_t elf_info;
  int errors;

  (void) ignored;

  TEST_BEGIN();

  memset( &elf_info, 0, sizeof( elf_info ) );
  load_elf_from_memory( magia_elf, magia_elf_size, &elf_info );

  /*
   * Mailboxes and "ready" cell first, MAGIA fetch enable last (contract in
   * offload_layout.h): MAGIA starts only with the shared memory already set
   * up and cannot see a stale ready flag left by a previous run.
   */
  fake_host_before();
  set_entry_point_and_enable_fetch( &elf_info );

  errors = fake_host_after();

  if ( errors != 0 ) {
    /* no TEST_END(): "*** END OF TEST" means success for the RTEMS tester */
    rtems_test_exit( 1 );
  }

  TEST_END();
  rtems_test_exit( 0 );
}

/* NOTICE: the clock driver is explicitly disabled */
#define CONFIGURE_APPLICATION_DOES_NOT_NEED_CLOCK_DRIVER
#define CONFIGURE_APPLICATION_NEEDS_SIMPLE_CONSOLE_DRIVER

#define CONFIGURE_MAXIMUM_TASKS 1

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#define CONFIGURE_INIT_TASK_ATTRIBUTES RTEMS_FLOATING_POINT

#define CONFIGURE_INITIAL_EXTENSIONS RTEMS_TEST_INITIAL_EXTENSION

#define CONFIGURE_INIT
#include <rtems/confdefs.h>
