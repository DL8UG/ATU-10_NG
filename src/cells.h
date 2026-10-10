// Settings ("Cells"): 12 BCD coded bytes at a fixed place in program
// memory, word address 0x7770 (byte address 0xEEE0 in the hex file), so
// they can be changed in the hex file without rebuilding the firmware.
// Decoding is free of hardware access and also builds on the PC.

#ifndef CELLS_H
#define CELLS_H

#include <stdint.h>

#define CELLS_ADDR  0x7770
#define CELL_COUNT  13
// 16 words reserved, the last 3 are spare (0x00): the Cells fill exactly two
// hex records (:10EEE000 and :10EEF000) that hold nothing else
#define CELLS_SIZE  16

enum {
   CFG_DISP_OFF,     // 1  display off after minutes, 0 = never
   CFG_POWER_OFF,    // 2  power off after minutes, 0 = never
   CFG_RELAY_MS,     // 3  relay pulse time in ms
   CFG_MIN_PWR,      // 4  minimum power for tuning in 0.1 W
   CFG_MAX_PWR,      // 5  maximum power for tuning in W
   CFG_AUTO_DELTA,   // 6  auto tune if the SWR changed by more than (value - 10) / 10
                     //    (from 90 on practically never: the SWR cannot change that much)
   CFG_AUTO,         // 7  auto tune 1 = on, 0 = off
   CFG_CAL_B,        // 8  detector calibration b = value / 10
   CFG_CAL_A,        // 9  detector calibration a = 1 + value / 100
   CFG_PEAK,         // 10 peak hold time for the power display in 10 ms
   CFG_TARGET,       // 11 tuning target: SWR 1 + value / 100, 0 = always full search
   CFG_SEARCH,       // 12 search effort 1..3
   CFG_LAYOUT,       // 13 main screen: 0 = classic, 1 = relays (older hex files: spare 0x00)
};

extern uint8_t cfg[CELL_COUNT];                 // decoded, decimal
extern const uint8_t cell_min[CELL_COUNT], cell_max[CELL_COUNT], cell_def[CELL_COUNT];

uint8_t cell_decode(uint8_t i, uint8_t bcd);    // decimal value, default if invalid
uint8_t dec2bcd(uint8_t v);
void cells_load(void);                          // hex Cells -> cfg[]
uint16_t cells_hash(uint8_t n);                 // checksum of the first n hex Cells

#endif
