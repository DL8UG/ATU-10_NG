// Interface of the FW 1.6 tuning functions (tune.c) to the NG simulator
// (glue_ref.c of a version folder, Firmware/ATU-10_NG_x_y_z/tools/sim).

#ifndef TUNE_H
#define TUNE_H

#define __bit unsigned char
#define Delay_us(x)

// provided by the simulator
extern char ind, cap, SW;
extern int PWR, SWR, PWR_fixed_old, min_for_start, max_for_start;
extern volatile __bit B_short, B_xlong;
void Relay_set(char, char, char);
void get_pwr(void);
void draw_power(unsigned int);

extern int tune_last;              // set by the simulator after a tune
// after tune(): SWR 0 = gave up (no power), 999 = no match
#define TUNE_RESULT (SWR>0 && SWR<999 ? SWR : 0)

void atu_reset(void);
void get_swr(void);
void tune(void);
void subtune(void);
void coarse_tune(void);
void coarse_ind_cap(void);
void coarse_cap(void);
void coarse_ind(void);
void sharp_tune(void);
void sharp_cap(void);
void sharp_ind(void);

#endif
