// Plausibility of the simulator's antenna models
#include "check.h"
#include "../tools/sim/antennas.c"

static double swr50(double complex z) {
   double g = cabs((z - 50) / (z + 50));
   return (1 + g) / (1 - g);
}

int main(void) {
   double f, f_res = 0, best = 1e9;
   double complex z;
   // radiation resistance of a half-wave dipole: 73 Ohm
   CHECK(fabs(rr_dipole(M_PI) - 73.1) < 0.5);
   // 20 m half-wave dipole (10.1 m): resonance a little below c / 2l
   for(f = 13e6; f < 15.5e6; f += 5e3) {
      z = z_dipole(f, 10.1, 0);
      if(fabs(cimag(z)) < best) { best = fabs(cimag(z)); f_res = f; }
   }
   z = z_dipole(f_res, 10.1, 0);
   printf("20 m dipole: resonance %.3f MHz, R %.1f Ohm\n", f_res / 1e6, creal(z));
   CHECK(f_res > 14.0e6 && f_res < 14.9e6);
   CHECK(creal(z) > 60 && creal(z) < 85);
   // full-wave (twice the frequency): high impedance, kOhms
   z = z_dipole(2 * f_res, 10.1, 0);
   printf("20 m dipole on 10 m: %.0f%+.0fj Ohm\n", creal(z), cimag(z));
   CHECK(cabs(z) > 1000 && cabs(z) < 10000);
   // short dipole: small R, large negative X
   z = z_dipole(3.6e6, 10.1, 0);
   CHECK(creal(z) < 15 && cimag(z) < -1000);
   // quarter-wave 75 Ohm line transforms 100 Ohm to 56.25 Ohm
   z = z_line(10e6, 75, 1, 0, 299792458.0 / 10e6 / 4, 100);
   CHECK(cabs(z - 56.25) < 0.01);
   // EFHW with 49:1 on its harmonics: near 50 Ohm, off the harmonics: not
   for(int b = 0; b < 4; b++) {
      double fb = (double[]){7.1e6, 14.2e6, 21.2e6, 28.5e6}[b];
      z = antennas[7].z(fb, antennas[7].p);
      printf("EFHW 20.2 m at %.1f MHz: %.0f%+.0fj, SWR %.1f\n", fb / 1e6, creal(z), cimag(z), swr50(z));
      CHECK(swr50(z) < 4);
   }
   z = antennas[7].z(10.12e6, antennas[7].p);
   printf("EFHW 20.2 m at 10.1 MHz: %.0f%+.0fj\n", creal(z), cimag(z));
   CHECK(swr50(z) > 4);
   // resonant 40 m dipole on coax: good SWR in band
   z = antennas[11].z(7.1e6, antennas[11].p);
   printf("40 m dipole on coax at 7.1 MHz: %.0f%+.0fj, SWR %.2f\n", creal(z), cimag(z), swr50(z));
   CHECK(swr50(z) < 2);
   return check_done("test_antennas");
}
