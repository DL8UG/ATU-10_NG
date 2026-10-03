// L network, bridge, detector and ADC model (see model.h)

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "model.h"

#define Q_L       100.0     // coil quality factor
#define C_STRAY   10.0      // pF, always in parallel to the capacitor bank

static const double L_NOM[7] = {0.1, 0.22, 0.45, 1.0, 2.2, 4.5, 10.0};
static const double C_NOM[7] = {22, 47, 100, 220, 470, 1000, 2200};
double l_uh[7] = {0.1, 0.22, 0.45, 1.0, 2.2, 4.5, 10.0};
double c_pf[7] = {22, 47, 100, 220, 470, 1000, 2200};

double freq;
double complex z_load;
int r_l, r_c, r_sw;
double noise_mv, jitter, r_src = -1, p_tx = 5.0, vdd_mv = 3900;
double true_a = 1.14, true_b = 0.4;
long relay_steps, measurements;
double time_s, relay_ms = 7;
int trace;
FILE *trace_file;

static double uniform(void) { return rand() / (RAND_MAX + 1.0); }

double gauss(void) {
   double u = (rand() + 1.0) / (RAND_MAX + 2.0), v = (rand() + 1.0) / (RAND_MAX + 2.0);
   return sqrt(-2 * log(u)) * cos(2 * M_PI * v);
}

void model_tolerances(double pct, unsigned seed) {
   unsigned s = (unsigned)rand();
   srand(seed * 7919 + 13);
   for(int i = 0; i < 7; i++) {
      l_uh[i] = L_NOM[i] * (1 + pct / 100 * (2 * uniform() - 1));
      c_pf[i] = C_NOM[i] * (1 + pct / 100 * (2 * uniform() - 1));
   }
   srand(s);
}

static double complex par(double complex a, double complex b) {
   return a * b / (a + b);
}

// sw = 1: capacitor at the input (transmitter) side, else at the output
double complex zin_of(int l, int c, int sw) {
   double w = 2 * M_PI * freq, L = 0, C = C_STRAY;
   for(int i = 0; i < 7; i++) {
      if(l & (1 << i)) L += l_uh[i];
      if(c & (1 << i)) C += c_pf[i];
   }
   double complex zl = w * L * 1e-6 * (1.0 / Q_L + I);
   double complex zc = 1.0 / (I * w * C * 1e-12);
   return sw ? par(zc, zl + z_load) : zl + par(z_load, zc);
}

double gamma_of(int l, int c, int sw) {
   double complex zin = zin_of(l, c, sw);
   return cabs((zin - 50) / (zin + 50));
}

double swr_of(double g) {
   return g >= 0.999 ? 999 : (1 + g) / (1 - g);
}

void model_relay_set(int l, int c, int sw) {
   r_l = l & 0x7F;
   r_c = c & 0x7F;
   r_sw = sw & 1;
   relay_steps++;
   time_s += (3 * relay_ms + 5) / 1000;      // pulses + settling
   if(trace) {
      fprintf(stderr, "%4ld  SW=%d L=%3d C=%3d  SWR %.3f\n", relay_steps, r_sw, r_l, r_c,
              swr_of(gamma_of(r_l, r_c, r_sw)));
      if(trace_file) fprintf(trace_file, "step\t%d\t%d\t%d\t%.4f\n", r_sw, r_l, r_c,
                             swr_of(gamma_of(r_l, r_c, r_sw)));
   }
}

void model_powers(double *pf, double *pr) {
   if(r_src < 0) {
      double g = gamma_of(r_l, r_c, r_sw);
      *pf = p_tx;
      *pr = p_tx * g * g;
   }
   else {   // the bridge sees (V +- 50 I) / 2 at the tuner input
      double complex zin = zin_of(r_l, r_c, r_sw);
      double vs = sqrt(p_tx * 50) * (50 + r_src) / 50;   // p_tx into 50 Ohm
      double complex i = vs / (zin + r_src), v = i * zin;
      *pf = pow(cabs(v + 50 * i), 2) / 200;
      *pr = pow(cabs(v - 50 * i), 2) / 200;
   }
}

// detector voltage for power p (real detector: true_a, true_b) plus noise
double model_sample_mv(double p, double scale) {
   double mv;
   p *= scale;
   if(p < 0) p = 0;
   mv = (-true_b + sqrt(true_b * true_b + 4 * true_a * p)) / (2 * true_a) * 1000;
   mv += noise_mv * gauss();
   return mv < 0 ? 0 : mv;
}
