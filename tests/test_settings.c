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
   // only 0x20..0x2F used
   for(int i = 0; i < 256; i++) if(i < 0x20 || i > 0x2F) CHECK_EQ(ee[i], 0xFF);
   return check_done("test_settings");
}
