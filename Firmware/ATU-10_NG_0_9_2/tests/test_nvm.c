// EEPROM ring and tune memory, against an emulated EEPROM
#include "check.h"
#include <string.h>
#include <stdint.h>

static uint8_t ee[256];
static long writes[256];
uint8_t nvm_read(uint8_t a) { return ee[a]; }
void nvm_write(uint8_t a, uint8_t v) { if(ee[a] != v) { ee[a] = v; writes[a]++; } }

#include "../src/tune.h"
relays_t tune_mem[MEM_SLOTS];
uint8_t tune_mem_swr[MEM_SLOTS], tune_mem_seq[MEM_SLOTS];
uint8_t tune_mem_n;
uint16_t tune_mem_dirty;

#include "../src/nvm.c"

int main(void) {
   long i, maxw = 0;
   memset(ee, 0xFF, sizeof ee);                 // erased
   CHECK(!state_load());
   mem_load();
   CHECK_EQ(tune_mem_n, 0);
   // many saves: always the last one comes back, also across the wrap
   for(i = 0; i < 1000; i++) {
      st.r.l = (uint8_t)(i % 128); st.r.c = (uint8_t)(i * 7 % 128); st.r.sw = (uint8_t)(i & 1);
      st.byp.l = 5; st.byp.c = 6; st.byp.sw = 1;
      st.bypass = (uint8_t)(i % 3 == 0);
      st.last_swr = (uint8_t)i;
      state_save();
      memset(&st, 0, sizeof st);
      CHECK(state_load());
      CHECK_EQ(st.r.l, i % 128);
      CHECK_EQ(st.r.c, i * 7 % 128);
      CHECK_EQ(st.r.sw, i & 1);
      CHECK_EQ(st.bypass, i % 3 == 0);
      CHECK_EQ(st.byp.sw, 1);
      CHECK_EQ(st.last_swr, (uint8_t)i);
   }
   for(i = 0x70; i < 0xF0; i++) if(writes[i] > maxw) maxw = writes[i];
   printf("1000 saves: at most %ld writes per byte\n", maxw);
   CHECK(maxw <= 1000 / 16 + 2);
   // a corrupted newest slot: the one before is used
   state_save();                                 // seq n, values of i = 999
   st.r.l = 42; state_save();
   ee[0x70 + ring_slot * 8 + 1] ^= 0x10;         // damage the newest
   CHECK(state_load());
   CHECK_EQ(st.r.l, 999 % 128);
   // nothing outside 0x30..0xEF is touched
   for(i = 0; i < 0x30; i++) CHECK_EQ(writes[i], 0);
   for(i = 0xF0; i < 256; i++) CHECK_EQ(writes[i], 0);
   // tune memory round trip
   tune_mem_n = 3;
   for(i = 0; i < 3; i++) {
      tune_mem[i].l = (uint8_t)(10 + i); tune_mem[i].c = (uint8_t)(100 + i); tune_mem[i].sw = (uint8_t)(i & 1);
      tune_mem_swr[i] = (uint8_t)(i * 3);
      tune_mem_seq[i] = (uint8_t)(250 + i * 4);    // across the wrap
   }
   tune_mem_dirty = 0x7;
   mem_save();
   CHECK_EQ(tune_mem_dirty, 0);
   memset(tune_mem, 0, sizeof tune_mem);
   mem_load();
   CHECK_EQ(tune_mem_n, 3);
   CHECK_EQ(tune_mem[2].l, 12);
   CHECK_EQ(tune_mem[2].c, 102);
   CHECK_EQ(tune_mem[1].sw, 1);
   CHECK_EQ(tune_mem_swr[2], 6);
   CHECK_EQ(tune_mem_seq[2], 2);
   // one slot changed: only its 5 bytes are written
   memset(writes, 0, sizeof writes);
   tune_mem[1].l = 77;
   tune_mem_dirty = 0x2;
   mem_save();
   for(i = 0; i < 256; i++) if(writes[i]) CHECK(i >= 0x35 && i < 0x3A);
   // a damaged slot in between stays empty, the others are kept
   ee[0x36] ^= 1;
   mem_load();
   CHECK_EQ(tune_mem_n, 3);
   CHECK(tune_mem[1].l == 0 && tune_mem[1].c == 0);
   CHECK_EQ(tune_mem[2].l, 12);
   return check_done("test_nvm");
}
