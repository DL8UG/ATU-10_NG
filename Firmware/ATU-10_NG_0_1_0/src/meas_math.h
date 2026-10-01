// Power, reflection and SWR from the detector voltages, integer only.
// Free of hardware access: the firmware and the PC simulator use the same
// code.
//
// Units:
//   detector voltage  1/8 mV  (uint16, up to ~4.2 V = 33600)
//   power             uW      (uint32)
//   g2                |reflection coefficient|^2 = Pr / Pf, scaled by 2^24
//                     (G2_ONE = total reflection or no forward power)

#ifndef MEAS_MATH_H
#define MEAS_MATH_H

#include <stdint.h>

#define Q_PER_MV   8
#define G2_SHIFT   24
#define G2_ONE     ((uint32_t)1 << G2_SHIFT)
#define SWR_MAX    999                 // SWR x 100 shown for 9.99 and above

typedef struct {
   uint16_t fwd, rev;      // detector voltages, 1/8 mV
   uint32_t pf, pr;        // forward and reverse power, uW
   uint32_t g2;            // Pr / Pf, 2^24 = 1
   uint8_t stable;         // the two halves of the measurement agreed
   uint8_t overflow;       // a detector voltage was above the ADC range
} meas_t;

uint32_t power_uw(uint16_t v);                 // calibration from Cells 8, 9
uint32_t g2_calc(uint32_t pf, uint32_t pr);
uint16_t isqrt32(uint32_t x);
uint16_t swr_x100(uint32_t g2);                // 100 .. 999
uint16_t pwr_x10(uint32_t uw);                 // 0.1 W, rounded
uint32_t pnet_uw(const meas_t *m);             // delivered power Pf - Pr

// Combines two half measurements taken in the order F1 R1 R2 F2 (cancels
// a linear drift of the carrier) into m and judges if they agree
void meas_finish(meas_t *m, uint16_t f1, uint16_t r1, uint16_t r2, uint16_t f2);

#endif
