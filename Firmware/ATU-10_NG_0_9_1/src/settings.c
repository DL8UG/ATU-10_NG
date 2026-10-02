#include "settings.h"
#include "cells.h"
#include "nvm.h"

#define SET_ADDR   0x20              // magic, hash (2), 12 values, crc: 16 bytes
#define SET_MAGIC  0x5E
#define SET_LEN    (3 + CELL_COUNT)

void settings_load(void) {
   uint8_t b[SET_LEN], i;
   uint16_t h = cells_hash();
   cells_load();
   for(i = 0; i < SET_LEN; i++) b[i] = nvm_read((uint8_t)(SET_ADDR + i));
   if(b[0] != SET_MAGIC || b[1] != (uint8_t)(h >> 8) || b[2] != (uint8_t)h) return;
   if(crc8(b, SET_LEN) != nvm_read(SET_ADDR + SET_LEN)) return;
   for(i = 0; i < CELL_COUNT; i++)
      if(b[3 + i] >= cell_min[i] && b[3 + i] <= cell_max[i]) cfg[i] = b[3 + i];
}

void settings_save(void) {
   uint8_t b[SET_LEN], i;
   uint16_t h = cells_hash();
   b[0] = SET_MAGIC;
   b[1] = (uint8_t)(h >> 8);
   b[2] = (uint8_t)h;
   for(i = 0; i < CELL_COUNT; i++) b[3 + i] = cfg[i];
   for(i = 0; i < SET_LEN; i++) nvm_write((uint8_t)(SET_ADDR + i), b[i]);
   nvm_write(SET_ADDR + SET_LEN, crc8(b, SET_LEN));
}

void settings_defaults(void) {
   nvm_write(SET_ADDR, 0xFF);
   cells_load();
}
