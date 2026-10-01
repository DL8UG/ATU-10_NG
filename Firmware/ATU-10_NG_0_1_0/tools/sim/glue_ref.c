// Runs an older tuning algorithm (tune.c / swr.c of the previous firmware
// line, from REF_DIR, not part of this repository) against the same model,
// for comparison during development only.

#include <math.h>
#include <stdlib.h>
#include "model.h"
#include "tune.h"      // from REF_DIR
#include "swr.h"

const char *glue_name = "ref";
int glue_nomem;
char ind, cap, SW;
int PWR, SWR, PWR_fixed_old, min_for_start = 10, max_for_start = 150;
volatile __bit B_short, B_xlong;
float Cal_a = 1.14f, Cal_b = 0.4f;

// ADC reading as the old get_forward()/get_reverse() return it, in mV
static int adc_old(double p) {
   double mv = model_sample_mv(p, 1);
   long v = lround(mv);
   if(v > 1023) v = 1023;
   if(v == 1023) {
      v = lround(mv / 2); if(v > 1023) v = 1023;
      v *= 2;
   }
   if(v == 2046) {
      v = lround(mv * 1024 / vdd_mv); if(v > 1023) v = 1023;
      v = (long)(v * vdd_mv / 1024);
   }
   time_s += 300e-6;
   return (int)v;
}

void Relay_set(char l, char c, char sw) { model_relay_set(l, c, sw); }

void get_pwr(void) {
   double pf, pr;
   model_powers(&pf, &pr);
   swr_calc(adc_old(pf), adc_old(pr));
}

void get_pwr_avg(char n) {
   double pf, pr;
   long fs = 0, rs = 0;
   model_powers(&pf, &pr);
   measurements++;
   for(int i = 0; i < n; i++) {
      fs += adc_old(pf * (1 + jitter * 0));
      rs += adc_old(pr);
   }
   swr_calc((float)fs / n, (float)rs / n);
}

void draw_power(unsigned int p) { (void)p; }

void glue_init(int search, int target) { (void)search; (void)target; relay_ms = 7; }

void glue_cold(void) {
   atu_reset();
   tune_last = 0;
}

void glue_tune(void) {
   tune();
   tune_last = TUNE_RESULT;
}
