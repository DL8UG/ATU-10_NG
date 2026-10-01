// PC simulator for the ATU-10 tuning algorithm.
//
// Links the firmware's tune.c and meas_math.c (glue_new.c) - or for
// comparison an older algorithm (glue_ref.c) - against a model of the L
// network, the bridge, the detectors and the ADC, runs a tune for each test
// case and prints one tab separated line per case:
//   suite  case  MHz  swr_reached  swr_best  relay_steps  measurements  time_s
// swr_reached is the true SWR of the relays as finally set, swr_best the
// best SWR of all 2 x 128 x 128 relay settings (brute force).
//
// Usage: sim [options]
//   --suite S      std (9 bands x 14 fixed loads), ant (all antennas), or one
//                  antenna suite: rw efhw dipole dip4 nrdip doublet
//   --case MHz R X single fixed load, traces every relay step to stderr
//   --ant N MHz    single antenna (index from --list) at one frequency, traced
//   --list         list the antennas
//   --noise mV     gaussian noise per ADC sample
//   --jitter f     carrier amplitude varies by this fraction (1 sigma)
//   --rs Ohm       transmitter source resistance (QRP rigs are no 50 Ohm source)
//   --tol %        component tolerances (random per seed)
//   --cal A B      real detector calibration (the firmware assumes 1.14 0.4)
//   --retune %     tune, move the frequency by this much, report the 2nd tune
//   --seed n       random seed
//   --search n     Cell 12 (search effort), --target n  Cell 11

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model.h"

extern const char *glue_name;
void glue_init(int search, int target);
void glue_cold(void);
void glue_tune(void);

static const double std_bands[] = {1.85, 3.6, 7.1, 10.12, 14.2, 18.1, 21.2, 24.9, 28.5};
static const double complex std_loads[] = {
   12.5, 25, 50, 100, 200, 450, 1000, 2000,
   10 - 100 * I, 30 + 80 * I, 300 + 300 * I, 2000 - 500 * I, 5 - 20 * I, 150 - 150 * I,
};

static double retune;
static const antenna_t *cur_ant;

static void set_freq(double f) {
   freq = f;
   if(cur_ant) z_load = ant_eval(cur_ant, f);
}

static double best_swr(void) {
   double best = 1;
   for(int sw = 0; sw < 2; sw++)
      for(int l = 0; l < 128; l++)
         for(int c = 0; c < 128; c++) {
            double g = gamma_of(l, c, sw);
            if(g < best) best = g;
         }
   return swr_of(best);
}

static void run(const char *suite, const char *label, double mhz) {
   double best;
   glue_cold();
   if(retune != 0) {   // first tune before the QSY
      set_freq(mhz * 1e6 / (1 + retune / 100));
      glue_tune();
   }
   set_freq(mhz * 1e6);
   best = best_swr();
   relay_steps = measurements = 0;
   time_s = 0;
   glue_tune();
   printf("%s\t%s\t%.3f\t%.3f\t%.3f\t%ld\t%ld\t%.2f\n", suite, label, mhz,
          swr_of(gamma_of(r_l, r_c, r_sw)), best, relay_steps, measurements, time_s);
}

static void run_std(void) {
   char label[32];
   cur_ant = NULL;
   for(size_t b = 0; b < sizeof std_bands / sizeof *std_bands; b++)
      for(size_t l = 0; l < sizeof std_loads / sizeof *std_loads; l++) {
         z_load = std_loads[l];
         snprintf(label, sizeof label, "%g%+gj", creal(z_load), cimag(z_load));
         run("std", label, std_bands[b]);
      }
}

static void run_ants(const char *suite) {
   for(int a = 0; a < n_antennas; a++) {
      if(strcmp(suite, "ant") && strcmp(suite, antennas[a].suite)) continue;
      cur_ant = &antennas[a];
      for(int b = 0; b < n_bands; b++)
         for(int k = 0; k < 3; k++)
            run(antennas[a].suite, antennas[a].name, band_freqs[b][k]);
   }
}

int main(int argc, char **argv) {
   unsigned seed = 1;
   double tol = 0;
   int search = 0, target = -1, ant = -1;
   double case_mhz = 0, case_r = 0, case_x = 0;
   const char *suite = "std";
   for(int i = 1; i < argc; i++) {
      const char *o = argv[i];
      int more = argc - i - 1;
      if(!strcmp(o, "--suite") && more >= 1) suite = argv[++i];
      else if(!strcmp(o, "--noise") && more >= 1) noise_mv = atof(argv[++i]);
      else if(!strcmp(o, "--jitter") && more >= 1) jitter = atof(argv[++i]);
      else if(!strcmp(o, "--rs") && more >= 1) r_src = atof(argv[++i]);
      else if(!strcmp(o, "--tol") && more >= 1) tol = atof(argv[++i]);
      else if(!strcmp(o, "--retune") && more >= 1) retune = atof(argv[++i]);
      else if(!strcmp(o, "--seed") && more >= 1) seed = (unsigned)atoi(argv[++i]);
      else if(!strcmp(o, "--search") && more >= 1) search = atoi(argv[++i]);
      else if(!strcmp(o, "--target") && more >= 1) target = atoi(argv[++i]);
      else if(!strcmp(o, "--cal") && more >= 2) { true_a = atof(argv[++i]); true_b = atof(argv[++i]); }
      else if(!strcmp(o, "--case") && more >= 3) {
         case_mhz = atof(argv[++i]); case_r = atof(argv[++i]); case_x = atof(argv[++i]);
      }
      else if(!strcmp(o, "--ant") && more >= 2) { ant = atoi(argv[++i]); case_mhz = atof(argv[++i]); }
      else if(!strcmp(o, "--list")) {
         for(int a = 0; a < n_antennas; a++) printf("%2d  %-8s %s\n", a, antennas[a].suite, antennas[a].name);
         return 0;
      }
      else {
         fprintf(stderr, "usage: see the comment at the top of tools/sim/sim.c\n");
         return 2;
      }
   }
   srand(seed);
   if(tol > 0) model_tolerances(tol, seed);
   glue_init(search, target);
   if(case_mhz > 0) {
      trace = 1;
      if(ant >= 0 && ant < n_antennas) {
         cur_ant = &antennas[ant];
         run(cur_ant->suite, cur_ant->name, case_mhz);
      }
      else {
         char label[32];
         z_load = case_r + case_x * I;
         snprintf(label, sizeof label, "%g%+gj", case_r, case_x);
         run("case", label, case_mhz);
      }
      return 0;
   }
   if(!strcmp(suite, "std")) run_std();
   else run_ants(suite);
   return 0;
}
