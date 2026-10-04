// The relay pulse sequence follows the ATU-10 firmware by David Fainitski,
// N7DDC (public domain).

#include "board.h"
#include "relays.h"
#include "cells.h"
#include "meas.h"

relays_t rel;

#define SETTLE_MS 5      // contacts bounce, the detectors follow

// Each relay has one coil pin. The coils of the relays to switch on are
// pulsed towards ground, then those to switch off towards plus: a coil pin
// at 0 during the first pulse, at 1 during the second. All relays get the
// pulse every time, latching relays already in place do not move.
void relays_set(uint8_t l, uint8_t c, uint8_t sw) {
   uint8_t t = cfg[CFG_RELAY_MS];
   if(vbat_mv <= BATT_SLOW_MV) t++;  // a weak battery drives the coils slower
   REL_L_010  = !(l & 0x01);
   REL_L_022  = !(l & 0x02);
   REL_L_045  = !(l & 0x04);
   REL_L_100  = !(l & 0x08);
   REL_L_220  = !(l & 0x10);
   REL_L_450  = !(l & 0x20);
   REL_L_1000 = !(l & 0x40);
   REL_C_22   = !(c & 0x01);
   REL_C_47   = !(c & 0x02);
   REL_C_100  = !(c & 0x04);
   REL_C_220  = !(c & 0x08);
   REL_C_470  = !(c & 0x10);
   REL_C_1000 = !(c & 0x20);
   REL_C_2200 = !(c & 0x40);
   REL_C_SW   = sw ? 1 : 0;
   REL_TO_GND = 1;
   delay_ms(t);
   REL_TO_GND = 0;
   __delay_us(10);
   REL_TO_PLUS_N = 0;
   delay_ms(t);
   REL_TO_PLUS_N = 1;
   delay_ms(t);
   LATD &= 0x0F;                     // all coil pins low again
   LATC &= 0x18;                     // (keeps RC3 and the driver RC4)
   REL_C_22 = 0;
   REL_C_47 = 0;
   REL_C_100 = 0;
   REL_C_220 = 0;
   REL_C_SW = 0;
   delay_ms(SETTLE_MS);
   rel.l = l & 0x7F;
   rel.c = c & 0x7F;
   rel.sw = sw ? 1 : 0;
}
