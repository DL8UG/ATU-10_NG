// Measuring the forward / reverse detectors and the battery with the ADC.

#ifndef MEAS_H
#define MEAS_H

#include <stdint.h>
#include "meas_math.h"

extern uint16_t vbat_mv;                      // last battery voltage

void meas_init(void);
uint16_t meas_battery(void);                  // measures and returns vbat_mv
// Full measurement: F1 R1 R2 F2, each the average of n samples (n <= 64)
void meas_take(meas_t *m, uint8_t n);
// Quick single sample of both detectors (display, peak hold)
void meas_quick(meas_t *m);

#endif
