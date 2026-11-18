// offload_layout.h
//
// "Contratto" tra host e device: indirizzi fissi in L2 e codici dei messaggi.
// Va incluso sia dal lato host (RTEMS su QEMU) sia dal lato device (omptarget_magia.c).
//
// In HERO questo ruolo lo hanno:
//   - i registri LETTER_0/1 della mailbox Carfield  (HERO2/sw/libhero/src/host/carfield/snitch_cluster.h)
//   - i codici MBOX_*                               (HERO2/sw/libhero/include/libhero/host/hero_api.h)
//
// Mappa MAGIA v4 (magia-sdk/gvsoc/pulp/pulp/chips/magia_v4/arch.py, magia_v4/link.ld):
//   L2           0xC000_0000 - 0xFFFF_FFFF (L2_SIZE = 1 GB di default)
//   codice       0xCC00_0000 (instrram, 32 KB)    <- NON usare
//   dati/.bss    0xCC01_0000 (dataram)            <- NON usare
//   TEST_END     0xCCFF_0000                      <- NON usare
//   stdout       0xFFFF_0004                      <- NON usare
// Le strutture dell'offload stanno quindi all'inizio della L2.
//
// Riccardo: questo file e' il contratto da condividere con l'host RTEMS. Con la
// memoria UMA (QEMU + GVSoC) host e MAGIA vedono la L2 agli STESSI indirizzi,
// quindi valgono tali e quali anche sull'host. Da verificare che tutta la zona
// 0xC000_0000 - 0xC100_0000 sia condivisa

#pragma once
#include <stdint.h>

// ---- Cella "ready": sostituisce LETTER_0/1 --------------------------------
// Sequenza (la memoria condivisa puo' sopravvivere tra due esecuzioni):
//   host:   flag = 0, indirizzi = 0, fence w,w        <- invalida residui vecchi
//           inizializza le caselle
//           scrive OFL_H2A_ADDR / OFL_A2H_ADDR, fence w,w
//           flag = OFL_READY_MAGIC                    <- per ULTIMO
//   device: attende flag == OFL_READY_MAGIC, fence r,r
//           legge OFL_H2A_ADDR / OFL_A2H_ADDR
//           fence r,w, flag = 0                       <- "consuma" il flag
// Riccardo: la parte "host:" della sequenza e' quella da scrivere su RTEMS, PRIMA
// del fetch_enable di MAGIA: cosi' MAGIA parte solo a memoria gia' pronta e non
// puo' vedere un flag rimasto da una sessione precedente.

#define OFL_READY_FLAG   0xC0000000u   // l'host ci scrive OFL_READY_MAGIC per ULTIMO
#define OFL_H2A_ADDR     0xC0000004u   // indirizzo della casella host -> device
#define OFL_A2H_ADDR     0xC0000008u   // indirizzo della casella device -> host
#define OFL_READY_MAGIC  0xCAFEBABEu

// ---- Le due caselle (struct ring_buf di ringbuf.h, allineate a 64 B) -------
#define OFL_H2A_RING     0xC0000100u
#define OFL_A2H_RING     0xC0000200u
#define OFL_H2A_DATA     0xC0001000u   // spazio dati della casella h2a
#define OFL_A2H_DATA     0xC0001400u   // spazio dati della casella a2h
#define OFL_RING_SLOTS   64u           // posti per casella (HERO ne usa 16; qui di piu'
                                       // perche' il finto host accoda tutto in anticipo)

//Le 64 parole stanno da 0xC000_1000 a 0xC000_10FF (h2a) e da 0xC000_1400 a 0xC000_14FF (a2h).
// 64 posti per casella ciascuno da 1 parola (4 B) = 256 B di dati ciascuna. 64*4 =256 byte= 0x100
// ---- Heap per i buffer dei dati (map) e per i buffer argomenti -------------
#define OFL_HEAP_BASE    0xC0010000u
#define OFL_HEAP_END     0xC1000000u

// ---- Protocollo: identico a HERO (hero_api.h / sw_mailbox.h) ---------------
#define MBOX_DEVICE_START  (0x02U)   // host->dev: START, &funzione, &argbuf, 0
#define MBOX_DEVICE_DONE   (0x04U)   // dev->host: DONE, cicli, dma, tot, dma, issue, compute
#define MBOX_DEVICE_STOP   (0x0FU)   // host->dev: esci dal ciclo
#define MBOX_DEVICE_LOGLVL (0x10U)   // host->dev: LOGLVL, valore (ignorato)
#define MBOX_DONE_WORDS    7u        // parole che seguono una risposta DONE (DONE compreso)
