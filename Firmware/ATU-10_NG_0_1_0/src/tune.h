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

// Result of the last tune
extern relays_t tune_best;
extern uint32_t tune_g2;           // reflection reached, G2_ONE = none
extern uint16_t tune_swr;          // SWR x 100 reached

// Tunes. 'from' is the current relay setting; if 'quick' is set it is a
// good earlier result and a local search from there is tried first.
uint8_t tune_run(const relays_t *from, uint8_t quick);

// Provided by the firmware or the simulator
void hal_relay_set(uint8_t l, uint8_t c, uint8_t sw);
void hal_sample(meas_t *m, uint8_t n);       // one measurement (meas_take)
uint8_t hal_abort(void);                     // 1 = stop tuning now
void hal_wait_ms(uint8_t ms);
void hal_progress(uint16_t swr);             // best SWR so far, for the display

#endif
