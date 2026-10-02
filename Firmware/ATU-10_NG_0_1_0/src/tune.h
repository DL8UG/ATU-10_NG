// Tuning: searches the relay setting with the lowest reflection.
// Free of hardware access: it only talks to the hal_* functions, which the
// firmware (app.c) or the PC simulator (tools/sim) provide.

#ifndef TUNE_H
#define TUNE_H

#include <stdint.h>
#include "meas_math.h"

#define L_MAX 127                  // 7 inductor relays
#define C_MAX 127                  // 7 capacitor relays

typedef struct {
   uint8_t l, c, sw;               // sw: 1 = capacitor on the input side
} relays_t;

enum {
   TUNE_OK,                        // tuned (or already good)
   TUNE_NO_CARRIER,                // no or too much power: relays unchanged or best so far
   TUNE_ABORTED,                   // button: relays on the best setting so far
   TUNE_NO_MATCH,                  // nothing better than bypass: relays in bypass
};

// Memory of the last good tunes. The tuner does not know the frequency; it
// measures the remembered settings at the start of a tune instead, and the
// best one starts a short local search. Each result goes into its own
// slot (one near the same place is replaced, else the oldest), so the
// firmware writes only that slot to the data EEPROM (tune_mem_dirty).
#define MEM_SLOTS    12
#define MEM_MAX_SWR  200           // results up to SWR 2.00 are remembered
extern relays_t tune_mem[MEM_SLOTS];
extern uint8_t tune_mem_swr[MEM_SLOTS];   // SWR x 100 - 100 reached then (max 255)
extern uint8_t tune_mem_seq[MEM_SLOTS];   // age: higher (mod 256) = newer
extern uint8_t tune_mem_n;                // slots in use (0..n-1)
extern uint16_t tune_mem_dirty;           // bit i: slot i changed

// Result of the last tune
extern relays_t tune_best;
extern uint32_t tune_g2;           // reflection reached, G2_ONE = none
extern uint16_t tune_swr;          // SWR x 100 reached

// Tunes. 'from' is the current relay setting. If it is the result of an
// earlier tune, last_swr is the SWR x 100 reached then (0 = no earlier
// result). The current setting and the memory are measured first; from
// the best of them a local search follows, kept if it gets at most
// QUICK_MARGIN worse than when that setting was found.
uint8_t tune_run(const relays_t *from, uint16_t last_swr);

// Provided by the firmware or the simulator
#ifdef __XC8   // firmware: called directly, one hardware stack level less
#define hal_relay_set relays_set
#define hal_sample    meas_take
#define hal_wait_ms   delay_ms
void relays_set(uint8_t l, uint8_t c, uint8_t sw);
void meas_take(meas_t *m, uint8_t n);
void delay_ms(uint16_t ms);
#else
void hal_relay_set(uint8_t l, uint8_t c, uint8_t sw);
void hal_sample(meas_t *m, uint8_t n);       // one measurement (meas_take)
void hal_wait_ms(uint8_t ms);
#endif
uint8_t hal_abort(void);                     // 1 = stop tuning now
void hal_progress(uint16_t swr);             // best SWR so far, for the display (between phases)

#endif
