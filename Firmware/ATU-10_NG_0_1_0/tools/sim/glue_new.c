// Runs the firmware's tune.c against the model: provides the hal_*
// functions and the Cells, and emulates meas.c (ADC ranges, F1 R1 R2 F2).

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "model.h"
#include "tune.h"
#include "cells.h"

volatile uint8_t Cells[16] = {
   0x05, 0x30, 0x07, 0x10, 0x15, 0x13, 0x01, 0x04, 0x14, 0x60, 0x08, 0x02 };

const char *glue_name = "new";
static relays_t cur;
static uint8_t have_result;

extern double gauss(void);

// as detector() in meas.c: start in the FVR 1.024 V range, move up when a
// sample does not fit, average n samples; result in 1/8 mV
static uint16_t detector(double p, uint8_t n, int *ovf) {
   double scale = 1 + jitter * gauss();
   for(int ref = 0; ; ref++) {
      long sum = 0;
      int i;
      for(i = 0; i < n; i++) {
         double mv = model_sample_mv(p, scale);
         long s = ref == 0 ? lround(mv) : ref == 1 ? lround(mv / 2) : lround(mv * 1024 / vdd_mv);
         if(s > 1023) s = 1023;
         if(s > 1000 && ref != 2) break;
         if(s >= 1023) *ovf = 1;
         sum += s;
      }
      time_s += n * 45e-6 + 100e-6;
      if(i < n) continue;
      double q = ref == 0 ? sum * 8.0 : ref == 1 ? sum * 16.0 : sum * vdd_mv / 1024 * 8;
      return (uint16_t)lround(q / n);
   }
}

void hal_relay_set(uint8_t l, uint8_t c, uint8_t sw) {
   model_relay_set(l, c, sw);
}

void hal_sample(meas_t *m, uint8_t n) {
   double pf, pr;
   int ovf = 0;
   uint16_t f1, r1, r2, f2;
   model_powers(&pf, &pr);
   measurements++;
   f1 = detector(pf, n, &ovf);
   r1 = detector(pr, n, &ovf);
   r2 = detector(pr, n, &ovf);
   f2 = detector(pf, n, &ovf);
   meas_finish(m, f1, r1, r2, f2);
   m->overflow = (uint8_t)ovf;
}

uint8_t hal_abort(void) { return 0; }
void hal_wait_ms(uint8_t ms) { time_s += ms / 1000.0; }
void hal_progress(uint16_t swr) {
   if(trace) fprintf(stderr, "      progress: best SWR %.2f\n", swr / 100.0);
}

void glue_init(int search, int target) {
   cells_load();
   if(search > 0) cfg[CFG_SEARCH] = (uint8_t)search;
   if(target >= 0) cfg[CFG_TARGET] = (uint8_t)target;
   relay_ms = cfg[CFG_RELAY_MS];
}

void glue_cold(void) {   // no earlier result, relays in bypass
   cur.l = cur.c = cur.sw = 0;
   have_result = 0;
   model_relay_set(0, 0, 0);
}

void glue_tune(void) {
   tune_run(&cur, have_result);
   cur = tune_best;
   have_result = tune_g2 < G2_ONE && (cur.l || cur.c);
}
