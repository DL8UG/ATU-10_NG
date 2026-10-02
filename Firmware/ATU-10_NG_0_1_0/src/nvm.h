// Data EEPROM: relay state and the memory of good tunes survive a reset or
// a battery change. Layout:
//   0x20..0x2F  settings block of the setup menu (settings.c)
//   0x30..0x6B  memory of good tunes, 12 slots of 5 bytes
//   0x70..0xEF  relay state, ring of 16 slots of 8 bytes
// The rest is not used (older firmware keeps its data at 0x00..0x08).

#ifndef NVM_H
#define NVM_H

#include <stdint.h>
#include "tune.h"

typedef struct {
   relays_t r;              // relays now
   relays_t byp;            // setting to go back to when bypass is switched off
   uint8_t bypass;          // 1 = bypass on
   uint8_t last_swr;        // SWR x 100 - 100 of the tune the relays hold, 0 = none
} state_t;

extern state_t st;

uint8_t nvm_read(uint8_t a);
void nvm_write(uint8_t a, uint8_t v);         // only if different (endurance)
uint8_t crc8(const uint8_t *p, uint8_t n);

void state_save(void);                        // st -> EEPROM (next ring slot)
uint8_t state_load(void);                     // 1 = st restored
void mem_save(void);                          // changed slots of the tune memory
void mem_load(void);

#endif
