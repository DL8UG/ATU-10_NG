// Integer measurement math against the floating point formulas
#include "check.h"
#include <math.h>
#include <stdint.h>

volatile uint8_t Cells[16] = {
   0x05, 0x30, 0x07, 0x10, 0x15, 0x13, 0x01, 0x04, 0x14, 0x60, 0x05, 0x02 };

#include "../src/cells.c"
#include "../src/meas_math.c"

static double power_float(double v_mv) {   // P = a V^2 + b V, watts
   double a = 1 + cfg[CFG_CAL_A] / 100.0, b = cfg[CFG_CAL_B] / 10.0, v = v_mv / 1000;
   return a * v * v + b * v;
}

static double swr_float(double pf, double pr) {
   double g = sqrt(pr / pf);
   return (1 + g) / (1 - g);
}

int main(void) {
   uint32_t v, worst_p = 0;
   int cal;
   cells_load();
   // power over the whole range for several calibrations
   for(cal = 0; cal < 3; cal++) {
      cfg[CFG_CAL_A] = (uint8_t[]){14, 0, 99}[cal];
      cfg[CFG_CAL_B] = (uint8_t[]){4, 0, 99}[cal];
      for(v = 0; v <= 33600; v += 7) {
         double want = power_float(v / 8.0) * 1e6;
         double got = power_uw((uint16_t)v);
         double err = fabs(got - want);
         CHECK(err <= 4 + want * 1e-6);
         if(err > worst_p) worst_p = (uint32_t)err;
      }
   }
   cells_load();
   // g2: exact fraction
   CHECK_EQ(g2_calc(0, 0), G2_ONE);
   CHECK_EQ(g2_calc(1000, 1000), G2_ONE);
   CHECK_EQ(g2_calc(1000, 2000), G2_ONE);
   CHECK_EQ(g2_calc(1000, 0), 0);
   CHECK_EQ(g2_calc(1000, 500), G2_ONE / 2);
   CHECK_EQ(g2_calc(76700000, 76699999), G2_ONE - 1);
   for(v = 1; v < 1000; v++) {
      uint32_t pf = 5000000, pr = v * 4999;
      double want = (double)pr / pf * G2_ONE;
      CHECK(fabs(g2_calc(pf, pr) - want) < 1.0);
   }
   // SWR against the float formula, +-1 in the last digit
   CHECK_EQ(swr_x100(0), 100);
   CHECK_EQ(swr_x100(G2_ONE), SWR_MAX);
   for(v = 1; v < 1000; v++) {
      uint32_t pf = 5000000, pr = v * 1000;
      double s = swr_float(pf, pr) * 100;
      uint16_t got = swr_x100(g2_calc(pf, pr));
      if(s > 999) CHECK_EQ(got, SWR_MAX);
      else CHECK(fabs(got - s) <= 1.0);
   }
   for(v = 0; v < 70000; v += 3) CHECK_EQ(isqrt32(v), (uint32_t)sqrt((double)v));
   CHECK_EQ(isqrt32(G2_ONE - 1), 4095);
   // display power rounding
   CHECK_EQ(pwr_x10(49999), 0);
   CHECK_EQ(pwr_x10(50000), 1);
   CHECK_EQ(pwr_x10(15000000), 150);
   // stability: equal halves are stable, a 20 % jump is not
   {
      meas_t m;
      meas_finish(&m, 8000, 800, 800, 8000);
      CHECK(m.stable);
      CHECK_EQ(m.fwd, 8000);
      meas_finish(&m, 8000, 800, 820, 7900);
      CHECK(m.stable);
      meas_finish(&m, 8000, 800, 800, 6400);
      CHECK(!m.stable);
      meas_finish(&m, 8000, 10, 30, 8000);   // tiny reverse voltage: absolute margin
      CHECK(m.stable);
      meas_finish(&m, 8000, 100, 200, 8000);
      CHECK(!m.stable);
   }
   printf("test_meas: worst power error %u uW\n", worst_p);
   return check_done("test_meas");
}
