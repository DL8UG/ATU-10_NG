// Interface of the FW 1.6 power and SWR calculation (swr.c) to the NG
// simulator (glue_ref.c).

#ifndef SWR_H
#define SWR_H

extern int PWR, SWR, min_for_start;
extern float Cal_a, Cal_b;

void swr_calc(float F, float R);   // forward and reverse detector voltage in mV
float sqrt_n(float);

#endif
