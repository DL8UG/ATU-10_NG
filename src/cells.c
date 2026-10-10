#include "cells.h"

uint8_t cfg[CELL_COUNT];

//                                      1   2   3   4   5   6   7   8   9  10  11  12  13
const uint8_t cell_min[CELL_COUNT] = {  0,  0,  2,  1,  1, 11,  0,  0,  0,  1,  0,  1,  0 };
const uint8_t cell_max[CELL_COUNT] = { 99, 99, 30, 99, 99, 99,  1, 99, 99, 99, 99,  3,  1 };
const uint8_t cell_def[CELL_COUNT] = {  5, 30, 10, 10, 15, 13,  1,  4, 14, 60,  5,  2,  0 };

#ifdef __XC8
// volatile: the compiler must read the values from program memory at run
// time and not build them into the code, or editing the hex would not work
const volatile uint8_t Cells[CELLS_SIZE] __at(CELLS_ADDR) = {
   0x05,   // 1  display off after 5 min
   0x30,   // 2  power off after 30 min
   0x10,   // 3  relay pulse 10 ms (the relay data sheets ask for 10 ms)
   0x10,   // 4  tune from 1.0 W
   0x15,   // 5  tune up to 15 W
   0x13,   // 6  auto tune if the SWR changed by more than 0.3
   0x01,   // 7  auto tune on
   0x04,   // 8  calibration b = 0.4 (BAT41 detector diodes)
   0x14,   // 9  calibration a = 1.14 (BAT41)
   0x60,   // 10 peak hold 600 ms
   0x05,   // 11 tuning target SWR 1.05
   0x02,   // 12 search effort 2
   0x00,   // 13 classic main screen
   0x00, 0x00, 0x00         // spare
};
#else
extern volatile uint8_t Cells[CELLS_SIZE];   // on the PC: defined by the test / simulator
#endif

uint8_t cell_decode(uint8_t i, uint8_t bcd) {
   uint8_t v;
   if((bcd >> 4) > 9 || (bcd & 0x0F) > 9) return cell_def[i];
   v = (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0F));
   if(v < cell_min[i] || v > cell_max[i]) return cell_def[i];
   return v;
}

uint8_t dec2bcd(uint8_t v) {
   return (uint8_t)((v / 10) << 4 | v % 10);
}

void cells_load(void) {
   uint8_t i;
   for(i = 0; i < CELL_COUNT; i++) cfg[i] = cell_decode(i, Cells[i]);
}

// Fletcher-16 over the raw Cells: a settings block saved by the setup menu
// only applies while the hex Cells are the ones it was made with
uint16_t cells_hash(uint8_t n) {
   uint8_t i, a = 0x5A, b = 0xA5;
   for(i = 0; i < n; i++) {
      a = (uint8_t)((a + Cells[i]) % 255);
      b = (uint8_t)((b + a) % 255);
   }
   return (uint16_t)b << 8 | a;
}
