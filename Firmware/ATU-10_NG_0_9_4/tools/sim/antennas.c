// Frequency dependent antenna models for the simulator.
//
// A wire antenna is modelled as an open, lossy transmission line
// (Schelkunoff): Z = Za coth((alpha + j beta) h). The loss term alpha
// stands for the radiation: alpha h = Rr / Za, with Rr the radiation
// resistance at the current maximum from the induced EMF method. This
// gives ~73 Ohm at half-wave resonance, a few kOhm at full-wave
// (anti-)resonance and the usual capacitive / inductive reactance in
// between, on all harmonics. Feed lines are lossy lines, baluns and ununs
// ideal transformers with magnetizing inductance, leakage and core loss.

#include <math.h>
#include "model.h"

#define C0      299792458.0
#define ETA     376.73
#define EULER   0.5772156649
#define WIRE_R  0.00075          // wire radius, m (1.5 mm diameter)
#define END_K   1.03             // end effect: a wire looks a little longer

// ---- special functions, by Simpson integration (x up to ~100)
static double simpson(double (*f)(double), double x) {
   int n = 2 * (int)(x / 0.01 + 1);
   double h = x / n, s = f(0) + f(x);
   for(int i = 1; i < n; i++) s += f(i * h) * (i & 1 ? 4 : 2);
   return s * h / 3;
}
static double si_f(double t) { return t == 0 ? 1 : sin(t) / t; }
static double cin_f(double t) { return t == 0 ? 0 : (1 - cos(t)) / t; }
static double Si(double x) { return simpson(si_f, x); }
static double Cin(double x) { return simpson(cin_f, x); }

// radiation resistance of a dipole of total length l at its current
// maximum, kl = 2 pi l / lambda
static double rr_dipole(double kl) {
   return ETA / (2 * M_PI) * (Cin(kl) + 0.5 * sin(kl) * (Si(2 * kl) - 2 * Si(kl))
                              + 0.5 * cos(kl) * (2 * Cin(kl) - Cin(2 * kl)));
}

static double complex ccoth(double complex x) {
   return ccosh(x) / csinh(x);
}

// centre fed dipole, total length l, plus loss resistance
static double complex z_dipole(double f, double l, double r_loss) {
   double k = 2 * M_PI * f / C0, h = l / 2;
   double za = 120 * (log(2 * h / WIRE_R) - 1);
   double ah = rr_dipole(k * l) / za;
   return za * ccoth(ah + I * k * h * END_K) + r_loss;
}

// end fed wire of length l against a counterpoise with loss r_gnd
static double complex z_endfed(double f, double l, double r_gnd) {
   double k = 2 * M_PI * f / C0;
   double za = 60 * (log(2 * l / WIRE_R) - 1);
   double al = rr_dipole(2 * k * l) / 2 / za;
   return za * ccoth(al + I * k * l * END_K) + r_gnd;
}

// transmission line: impedance z0, velocity factor vf, loss in dB/100 m at
// 10 MHz (grows with sqrt(f)), length len, terminated with zl
static double complex z_line(double f, double z0, double vf, double db100, double len,
                             double complex zl) {
   double a = db100 / 100 / 8.686 * sqrt(f / 10e6), b = 2 * M_PI * f / (C0 * vf);
   double complex t = ctanh((a + I * b) * len);
   return z0 * (zl + z0 * t) / (z0 + zl * t);
}

// transformer with impedance ratio n (antenna side high), magnetizing
// inductance lm and core loss rp on the tuner side, leakage ls in series,
// optional compensation capacitor cp across the tuner side
static double complex z_xfmr(double f, double complex zant, double n, double lm_uh,
                             double rp, double ls_uh, double cp_pf) {
   double w = 2 * M_PI * f;
   double complex y = n / zant + 1 / (I * w * lm_uh * 1e-6) + 1 / rp + I * w * cp_pf * 1e-12;
   return 1 / y + I * w * ls_uh * 1e-6;
}

#define RG58_DB  4.6
#define LADDER_DB 0.5

// p: wire length, counterpoise loss
static double complex ant_rw(double f, const double *p) {
   return z_xfmr(f, z_endfed(f, p[0], p[1]), 9, 8, 3000, 0.15, 0);
}
// p: wire length
static double complex ant_efhw(double f, const double *p) {
   return z_xfmr(f, z_endfed(f, p[0], 5), 49, 4.3, 500, 0.1, 100);
}
// p: dipole length, coax length
static double complex ant_dipole(double f, const double *p) {
   return z_line(f, 50, 0.66, RG58_DB, p[1], z_dipole(f, p[0], 2));
}
// p: dipole length, coax length; 1:4 balun at the feed point
static double complex ant_dip4(double f, const double *p) {
   return z_line(f, 50, 0.66, RG58_DB, p[1], z_xfmr(f, z_dipole(f, p[0], 2), 4, 20, 4000, 0.1, 0));
}
// p: dipole length, ladder line length, ladder line impedance; 1:4 balun at the tuner
static double complex ant_doublet(double f, const double *p) {
   double complex z = z_line(f, p[2], 0.9, LADDER_DB, p[1], z_dipole(f, p[0], 2));
   return z_xfmr(f, z, 4, 20, 4000, 0.1, 0);
}

const antenna_t antennas[] = {
   // random wire with 9:1 unun, common lengths
   {"rw", "random wire 8.8 m, 9:1", ant_rw, {8.8, 20}},
   {"rw", "random wire 10.8 m, 9:1", ant_rw, {10.8, 20}},
   {"rw", "random wire 12.5 m, 9:1", ant_rw, {12.5, 20}},
   {"rw", "random wire 17.7 m, 9:1", ant_rw, {17.7, 20}},
   {"rw", "random wire 21.6 m, 9:1", ant_rw, {21.6, 20}},
   {"rw", "random wire 25.6 m, 9:1", ant_rw, {25.6, 20}},
   // 40 m EFHW with 49:1 transformer
   {"efhw", "EFHW 19.8 m, 49:1", ant_efhw, {19.8}},
   {"efhw", "EFHW 20.2 m, 49:1", ant_efhw, {20.2}},
   {"efhw", "EFHW 20.6 m, 49:1", ant_efhw, {20.6}},
   // resonant dipoles on 50 Ohm coax (used on all bands)
   {"dipole", "80 m dipole 39.2 m, coax 15 m", ant_dipole, {39.2, 15}},
   {"dipole", "80 m dipole 39.2 m, coax 25 m", ant_dipole, {39.2, 25}},
   {"dipole", "40 m dipole 20.2 m, coax 15 m", ant_dipole, {20.2, 15}},
   {"dipole", "40 m dipole 20.2 m, coax 25 m", ant_dipole, {20.2, 25}},
   {"dipole", "20 m dipole 10.1 m, coax 15 m", ant_dipole, {10.1, 15}},
   {"dipole", "20 m dipole 10.1 m, coax 25 m", ant_dipole, {10.1, 25}},
   // dipole with 1:4 balun at the feed point, coax to the tuner
   {"dip4", "dipole 2x10 m, 1:4 balun, coax 10 m", ant_dip4, {20, 10}},
   {"dip4", "dipole 2x10 m, 1:4 balun, coax 20 m", ant_dip4, {20, 20}},
   {"dip4", "dipole 2x15 m, 1:4 balun, coax 10 m", ant_dip4, {30, 10}},
   {"dip4", "dipole 2x15 m, 1:4 balun, coax 20 m", ant_dip4, {30, 20}},
   // non resonant dipoles directly on coax
   {"nrdip", "dipole 2x7 m, coax 15 m", ant_dipole, {14, 15}},
   {"nrdip", "dipole 2x7 m, coax 25 m", ant_dipole, {14, 25}},
   {"nrdip", "dipole 2x13 m, coax 15 m", ant_dipole, {26, 15}},
   {"nrdip", "dipole 2x13 m, coax 25 m", ant_dipole, {26, 25}},
   {"nrdip", "dipole 2x17 m, coax 15 m", ant_dipole, {34, 15}},
   {"nrdip", "dipole 2x17 m, coax 25 m", ant_dipole, {34, 25}},
   // doublets on ladder line, 1:4 balun at the tuner
   {"doublet", "doublet 2x20 m, 450 Ohm 15 m", ant_doublet, {40, 15, 450}},
   {"doublet", "doublet 2x20 m, 450 Ohm 20 m", ant_doublet, {40, 20, 450}},
   {"doublet", "doublet 2x15.5 m (G5RV), 450 Ohm 10.4 m", ant_doublet, {31, 10.4, 450}},
   {"doublet", "doublet 2x15.5 m, 450 Ohm 20 m", ant_doublet, {31, 20, 450}},
   {"doublet", "doublet 2x10 m, 600 Ohm 12 m", ant_doublet, {20, 12, 600}},
};
const int n_antennas = sizeof antennas / sizeof *antennas;

// three frequencies per band: lower edge, middle, upper edge
const double band_freqs[][3] = {
   {1.81, 1.85, 1.99}, {3.5, 3.65, 3.8}, {7.0, 7.1, 7.2}, {10.1, 10.125, 10.15},
   {14.0, 14.175, 14.35}, {18.068, 18.118, 18.168}, {21.0, 21.225, 21.45},
   {24.89, 24.94, 24.99}, {28.0, 28.85, 29.7},
};
const char *band_names[] = {"160", "80", "40", "30", "20", "17", "15", "12", "10"};
const int n_bands = sizeof band_freqs / sizeof *band_freqs;

double complex ant_eval(const antenna_t *a, double f) {
   return a->z(f, a->p);
}
