// Settings: hex Cells, EEPROM override from the setup menu, hash rule
#include "check.h"
#include <stdint.h>
#include <string.h>

static uint8_t ee[256];
uint8_t nvm_read(uint8_t a) { return ee[a]; }
void nvm_write(uint8_t a, uint8_t v) { ee[a] = v; }
uint8_t crc8(const uint8_t *p, uint8_t n);

volatile uint8_t Cells[16] = {
   0x05, 0x30, 0x10, 0x10, 0x15, 0x13, 0x01, 0x04, 0x14, 0x60, 0x05, 0x02 };

#include "../src/cells.c"
#include "../src/settings.c"

uint8_t crc8(const uint8_t *p, uint8_t n) {   // as in nvm.c
   uint8_t c = 0x5A, i;
   while(n--) {
      c ^= *p++;
      for(i = 0; i < 8; i++) c = (uint8_t)(c & 0x80 ? c << 1 ^ 0x07 : c << 1);
   }
   return c;
}

int main(void) {
   memset(ee, 0xFF, sizeof ee);
   settings_load();                              // nothing saved: hex values
   CHECK_EQ(cfg[CFG_RELAY_MS], 10);
   cfg[CFG_RELAY_MS] = 12;                       // the menu changes and saves
   cfg[CFG_SEARCH] = 3;
   settings_save();
   cfg[CFG_RELAY_MS] = 0;
   settings_load();
   CHECK_EQ(cfg[CFG_RELAY_MS], 12);
   CHECK_EQ(cfg[CFG_SEARCH], 3);
   CHECK_EQ(cfg[CFG_DISP_OFF], 5);
   // a damaged block is ignored
   ee[0x25] ^= 1;
   settings_load();
   CHECK_EQ(cfg[CFG_RELAY_MS], 10);
   ee[0x25] ^= 1;
   settings_load();
   CHECK_EQ(cfg[CFG_RELAY_MS], 12);
   // a hex file with other Cells: its values apply
   Cells[CFG_DISP_OFF] = 0x10;
   settings_load();
   CHECK_EQ(cfg[CFG_RELAY_MS], 10);
   CHECK_EQ(cfg[CFG_DISP_OFF], 10);
   // back to the old Cells: the saved block applies again
   Cells[CFG_DISP_OFF] = 0x05;
   settings_load();
   CHECK_EQ(cfg[CFG_RELAY_MS], 12);
   // HEX VALUES in the menu: block gone
   settings_defaults();
   CHECK_EQ(cfg[CFG_RELAY_MS], 10);
   settings_load();
   CHECK_EQ(cfg[CFG_RELAY_MS], 10);
   // a block saved by firmware 1.0.0 (12 Cells, hash over 12) still applies
   {
      static const uint8_t old[16] = {   // 1.0.0, relay pulse 12 ms, search 3
         0x5E, 0x00, 0x00, 5, 30, 12, 10, 15, 13, 1, 4, 14, 60, 5, 3, 0 };
      uint16_t h12 = cells_hash(12);
      memset(ee, 0xFF, sizeof ee);
      memcpy(&ee[0x20], old, 16);
      ee[0x21] = (uint8_t)(h12 >> 8);
      ee[0x22] = (uint8_t)h12;
      ee[0x2F] = crc8(&ee[0x20], 15);
      settings_load();
      CHECK_EQ(cfg[CFG_RELAY_MS], 12);
      CHECK_EQ(cfg[CFG_SEARCH], 3);
      CHECK_EQ(cfg[CFG_LAYOUT], 0);
   }
   // Cell 13 from the menu: saved in its own block
   cfg[CFG_LAYOUT] = 1;
   settings_save();
   cfg[CFG_LAYOUT] = 0;
   cfg[CFG_RELAY_MS] = 0;
   settings_load();
   CHECK_EQ(cfg[CFG_LAYOUT], 1);
   CHECK_EQ(cfg[CFG_RELAY_MS], 12);
   // a hex file with another Cell 13: its value applies, Cells 1..12 stay
   Cells[CFG_LAYOUT] = 0x01;
   cfg[CFG_LAYOUT] = 1;
   settings_save();
   Cells[CFG_LAYOUT] = 0x00;
   settings_load();
   CHECK_EQ(cfg[CFG_LAYOUT], 0);
   CHECK_EQ(cfg[CFG_RELAY_MS], 12);
   // a damaged Cell 13 block is ignored, the other one applies
   Cells[CFG_LAYOUT] = 0x01;
   cfg[CFG_LAYOUT] = 0;                          // the menu against the hex value
   settings_save();
   settings_load();
   CHECK_EQ(cfg[CFG_LAYOUT], 0);
   ee[0xF3] ^= 1;
   settings_load();
   CHECK_EQ(cfg[CFG_LAYOUT], 1);
   CHECK_EQ(cfg[CFG_RELAY_MS], 12);
   ee[0xF3] ^= 1;
   settings_load();
   CHECK_EQ(cfg[CFG_LAYOUT], 0);
   Cells[CFG_LAYOUT] = 0x00;
   settings_defaults();
   CHECK_EQ(ee[0xF0], 0xFF);
   // only 0x20..0x2F and 0xF0..0xF4 used
   for(int i = 0; i < 256; i++)
      if((i < 0x20 || i > 0x2F) && (i < 0xF0 || i > 0xF4)) CHECK_EQ(ee[i], 0xFF);
   return check_done("test_settings");
}
