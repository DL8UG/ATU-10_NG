// Cells decoding and hash
#include "check.h"
#include <stdint.h>

volatile uint8_t Cells[16] = {
   0x05, 0x30, 0x10, 0x10, 0x15, 0x13, 0x01, 0x04, 0x14, 0x60, 0x05, 0x02 };

#include "../src/cells.c"

int main(void) {
   uint8_t i;
   uint16_t h;
   // defaults as BCD in the table give the default values
   cells_load();
   for(i = 0; i < CELL_COUNT; i++) CHECK_EQ(cfg[i], cell_def[i]);
   // invalid BCD and out of range values fall back to the default
   CHECK_EQ(cell_decode(CFG_RELAY_MS, 0x0A), 10);    // not BCD
   CHECK_EQ(cell_decode(CFG_RELAY_MS, 0xA1), 10);
   CHECK_EQ(cell_decode(CFG_RELAY_MS, 0x01), 10);    // below 2 ms
   CHECK_EQ(cell_decode(CFG_RELAY_MS, 0x31), 10);    // above 30 ms
   CHECK_EQ(cell_decode(CFG_RELAY_MS, 0x30), 30);
   CHECK_EQ(cell_decode(CFG_MIN_PWR, 0x00), 10);    // 0 W would tune on noise
   CHECK_EQ(cell_decode(CFG_AUTO_DELTA, 0x10), 13);
   CHECK_EQ(cell_decode(CFG_AUTO, 0x02), 1);
   CHECK_EQ(cell_decode(CFG_SEARCH, 0x03), 3);
   CHECK_EQ(cell_decode(CFG_SEARCH, 0x00), 2);
   CHECK_EQ(cell_decode(CFG_DISP_OFF, 0x99), 99);
   for(i = 0; i < 100; i++) CHECK_EQ(cell_decode(CFG_DISP_OFF, dec2bcd(i)), i);
   // the hash changes with every Cell
   h = cells_hash(CELL_COUNT);
   for(i = 0; i < CELL_COUNT; i++) {
      uint8_t old = Cells[i];
      Cells[i] = (uint8_t)(old + 1);
      CHECK(cells_hash(CELL_COUNT) != h);
      Cells[i] = old;
   }
   CHECK_EQ(cells_hash(CELL_COUNT), h);
   // Cell 13: 0x00 (the spare byte of older hex files) = classic screen
   CHECK_EQ(cfg[CFG_LAYOUT], 0);
   CHECK_EQ(cell_decode(CFG_LAYOUT, 0x01), 1);
   CHECK_EQ(cell_decode(CFG_LAYOUT, 0x02), 0);
   return check_done("test_cells");
}
