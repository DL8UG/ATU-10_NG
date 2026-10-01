#include "meas_math.h"
#include "cells.h"

// P = a V^2 + b V with V in volts, a = 1 + Cell9 / 100, b = Cell8 / 10.
// With v in 1/8 mV: P[uW] = (100 + c9) v^2 / 6400 + 12.5 c8 v
// v^2 / 64 < 17.7e6 and (100 + c9) <= 199: the product fits 32 bits.
// (a macro, so meas_finish needs no extra hardware stack level for it)
#define POWER_UW(v) ((((uint32_t)(v) * (v)) >> 6) * (uint32_t)(100 + cfg[CFG_CAL_A]) / 100 \
                     + ((uint32_t)(v) * cfg[CFG_CAL_B] * 25) / 2)

uint32_t power_uw(uint16_t v) {
   return POWER_UW(v);
}

// Pr / Pf as a 24 bit binary fraction, by long division: exact without
// 64 bit arithmetic (Pf < 2^27, so the remainder never overflows)
uint32_t g2_calc(uint32_t pf, uint32_t pr) {
   uint32_t q = 0;
   uint8_t i;
   if(pf == 0 || pr >= pf) return G2_ONE;
   for(i = 0; i < G2_SHIFT; i++) {
      pr <<= 1;
      q <<= 1;
      if(pr >= pf) {
         pr -= pf;
         q |= 1;
      }
   }
   return q;
}

uint16_t isqrt32(uint32_t x) {
   uint32_t r = 0, bit = (uint32_t)1 << 30;
   while(bit > x) bit >>= 2;
   while(bit) {
      if(x >= r + bit) {
         x -= r + bit;
         r = (r >> 1) + bit;
      }
      else r >>= 1;
      bit >>= 2;
   }
   return (uint16_t)r;
}

// SWR = (1 + |G|) / (1 - |G|), |G| = sqrt(g2) / 4096
uint16_t swr_x100(uint32_t g2) {
   uint16_t g = isqrt32(g2);
   uint32_t s;
   if(g >= 4096) return SWR_MAX;
   s = (100UL * (4096 + g) + (4096 - g) / 2) / (4096 - g);
   return s > SWR_MAX ? SWR_MAX : (uint16_t)s;
}

uint16_t pwr_x10(uint32_t uw) {
   return (uint16_t)((uw + 50000) / 100000);
}

uint32_t pnet_uw(const meas_t *m) {
   return m->pr < m->pf ? m->pf - m->pr : 0;
}

// The halves agree if both voltages differ by at most 1/8 plus a little
// absolute margin for the ADC resolution near zero
static uint8_t close(uint16_t a, uint16_t b, uint16_t margin) {
   uint16_t d = a > b ? a - b : b - a;
   uint16_t m = (a > b ? a : b) / 8 + margin;
   return d <= m;
}

// |a - b| / max(a, b) in 1/256 (a macro: no extra hardware stack level)
#define REL_DIFF(a, b) ((a) > (b) ? (uint16_t)(((uint32_t)((a) - (b)) << 8) / (a)) \
                      : (b) ? (uint16_t)(((uint32_t)((b) - (a)) << 8) / (b)) : 0)

void meas_finish(meas_t *m, uint16_t f1, uint16_t r1, uint16_t r2, uint16_t f2) {
   uint16_t sp;
   m->fwd = (uint16_t)(((uint32_t)f1 + f2 + 1) / 2);
   m->rev = (uint16_t)(((uint32_t)r1 + r2 + 1) / 2);
   m->pf = POWER_UW(m->fwd);
   m->pr = POWER_UW(m->rev);
   m->g2 = g2_calc(m->pf, m->pr);
   m->stable = close(f1, f2, 3 * Q_PER_MV) && close(r1, r2, 3 * Q_PER_MV);
   // Pr / Pf follows the voltage ratio to the power 1 .. 2: the relative
   // difference of the halves in g2 is about the sum of both
   sp = REL_DIFF(r1, r2) + REL_DIFF(f1, f2);
   m->spread = sp > 255 ? 255 : (uint8_t)sp;
}
