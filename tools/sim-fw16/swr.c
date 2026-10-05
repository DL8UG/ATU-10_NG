// FW 1.6 by N7DDC (David Fainitski), public domain: the power and SWR
// calculation of get_pwr() and sqrt_n(), copied unchanged from main.c of
// FW 1.6, for the NG simulator only. The ADC readings (get_forward,
// get_reverse) come from the simulator (glue_ref.c); the display handling
// of get_pwr() is left out.

#include "swr.h"

void swr_calc(float F, float R){   // detector voltages in mV
   volatile float gamma;
   F /= 1000;  // to Volts
   R /= 1000;  // to Volts
   F = Cal_a * F * F + Cal_b * F;
   R = Cal_a * R * R + Cal_b * R;
   PWR = (int)(F * 10 + 0.5);         // 0 - 150 (0 - 15.0 Watts)
   //
   if(PWR<min_for_start)  SWR = 0;      // < 1W
   else if(R >= F) SWR = 999;
   else {
      gamma = sqrt_n(R / F);
      if((1.0-gamma) == 0) gamma = 0.001;
      gamma = (1.0 + gamma) / (1.0 - gamma);
      if(gamma<1.0)
         gamma = 1.0;
      if(gamma>9.985) SWR = 999;
      else SWR = (int)(gamma * 100 + 0.5);
   }
   //
   return;
}

float sqrt_n(float x){   // Thanks, Newton !
   char i;
   const char n = 8;
   float a[n];
   a[0] = x/2;
   for(i=1; i<(n); i++)
      a[i] = (a[i-1] + x/a[i-1]) / 2;
   //
   return a[n-1];
}
