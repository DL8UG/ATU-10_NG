// PC model of the ATU-10: L network, bridge, detectors, ADC, transmitter,
// and the antennas behind it. Shared by the simulator front end (sim.c),
// the firmware glue (glue_new.c) and the reference glue (glue_ref.c).

#ifndef MODEL_H
#define MODEL_H

#include <complex.h>

// ---- network
extern double freq;                  // Hz
extern double complex z_load;        // load at the tuner output
extern int r_l, r_c, r_sw;           // relays as actually switched
extern double l_uh[7], c_pf[7];      // component values (may carry tolerances)

double complex zin_of(int l, int c, int sw);
double gamma_of(int l, int c, int sw);
double swr_of(double g);
void model_tolerances(double pct, unsigned seed);   // randomize component values

// ---- measurement
extern double noise_mv;              // gaussian noise per ADC sample
extern double jitter;                // carrier amplitude change between halves (fraction)
extern double r_src;                 // transmitter source resistance, < 0: constant Pf
extern double p_tx;                  // transmitter power into 50 Ohm, W
extern double vdd_mv;                // supply = ADC reference in the Vdd range
extern double true_a, true_b;        // real detector calibration (P = a V^2 + b V)
void model_powers(double *pf, double *pr);          // at the current relay setting
double model_sample_mv(double p, double scale);     // one detector reading as the ADC sees it, mV

// ---- accounting
extern long relay_steps, measurements;
extern double time_s;                // estimated tuning time
extern int trace;
extern double relay_ms;
void model_relay_set(int l, int c, int sw);

// ---- antennas (antennas.c)
typedef struct {
   const char *suite, *name;
   double complex (*z)(double f, const double *p);  // impedance at the tuner output
   double p[6];
} antenna_t;
extern const antenna_t antennas[];
extern const int n_antennas;
extern const double band_freqs[][3];
extern const char *band_names[];
extern const int n_bands;
double complex ant_eval(const antenna_t *a, double f);

#endif
