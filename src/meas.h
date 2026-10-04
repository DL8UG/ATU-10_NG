// Measuring the forward / reverse detectors and the battery with the ADC.

#ifndef MEAS_H
#define MEAS_H

#include <stdint.h>
#include "meas_math.h"

extern uint16_t vbat_mv;                      // last battery voltage

// Battery levels (app.c); BATT_OFF_MV is also the empty battery symbol
#define BATT_WARN_MV 3400                     // below: the battery symbol blinks
#define BATT_LOW_MV  3200                     // below: RECHARGE (tuning still works)
#define BATT_OFF_MV  3000                     // below: LOW BATT, switched off
// The LED blink at each battery check (app.c): green, yellow, else red
#define BATT_GREEN_MV  3700                   // above: green
#define BATT_YELLOW_MV 3590                   // above: yellow (both)
// Relay pulse 1 ms longer (relays.c): a weak battery drives the coils slower
#define BATT_SLOW_MV 3800                     // at and below

void meas_init(void);
void meas_off(void);                          // ADC and reference off (sleep); meas_init again
uint16_t meas_battery(void);                  // measures and returns vbat_mv
// Full measurement: F1 R1 R2 F2, each the average of n samples (n <= 64)
void meas_take(meas_t *m, uint8_t n);

#endif
