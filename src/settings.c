#include "settings.h"
#include "cells.h"
#include "nvm.h"

// Two blocks: Cells 1..12 at 0x20 exactly as up to firmware 1.0.0 (an update
// keeps the menu settings), the Cells added later at 0xF0. Each block: magic,
// hash (2) over the hex Cells up to its last one, the values, crc.
#define SET_MAGIC  0x5E
#define SET_ADDR   0x20              // Cells 1..12: 16 bytes
#define SET_N      12
#define EXT_ADDR   0xF0              // Cells 13..: 3 + n + 1 bytes
#define EXT_N      (CELL_COUNT - SET_N)

static void load(uint8_t addr, uint8_t first, uint8_t n) {
   uint8_t b[3 + SET_N], i, len = (uint8_t)(3 + n);
   uint16_t h = cells_hash((uint8_t)(first + n));
   for(i = 0; i < len; i++) b[i] = nvm_read((uint8_t)(addr + i));
   if(b[0] != SET_MAGIC || b[1] != (uint8_t)(h >> 8) || b[2] != (uint8_t)h) return;
   if(crc8(b, len) != nvm_read((uint8_t)(addr + len))) return;
   for(i = 0; i < n; i++)
      if(b[3 + i] >= cell_min[first + i] && b[3 + i] <= cell_max[first + i]) cfg[first + i] = b[3 + i];
}

static void save(uint8_t addr, uint8_t first, uint8_t n) {
   uint8_t b[3 + SET_N], i, len = (uint8_t)(3 + n);
   uint16_t h = cells_hash((uint8_t)(first + n));
   b[0] = SET_MAGIC;
   b[1] = (uint8_t)(h >> 8);
   b[2] = (uint8_t)h;
   for(i = 0; i < n; i++) b[3 + i] = cfg[first + i];
   for(i = 0; i < len; i++) nvm_write((uint8_t)(addr + i), b[i]);
   nvm_write((uint8_t)(addr + len), crc8(b, len));
}

void settings_load(void) {
   cells_load();
   load(SET_ADDR, 0, SET_N);
   load(EXT_ADDR, SET_N, EXT_N);
}

void settings_save(void) {
   save(SET_ADDR, 0, SET_N);
   save(EXT_ADDR, SET_N, EXT_N);
}

void settings_defaults(void) {
   nvm_write(SET_ADDR, 0xFF);
   nvm_write(EXT_ADDR, 0xFF);
   cells_load();
}
