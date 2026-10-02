// Search internals: memory slots, final comparison with bypass
#include "check.h"
#include <string.h>
#include <stdint.h>

volatile uint8_t Cells[16] = {
   0x05, 0x30, 0x07, 0x10, 0x15, 0x13, 0x01, 0x04, 0x14, 0x60, 0x05, 0x02 };
#include "../src/cells.c"
#include "../src/meas_math.c"
#include "../src/tune.c"

static relays_t now;                 // relays as set
static int bypass_verified, probes;
void hal_relay_set(uint8_t l, uint8_t c, uint8_t sw) { now.l = l; now.c = c; now.sw = sw; probes++; }
void hal_sample(meas_t *m, uint8_t n) {   // best match at L 40, C 40, side 1
   double d2 = (now.l - 40.0) * (now.l - 40.0) + (now.c - 40.0) * (now.c - 40.0) + (now.sw ? 0 : 900);
   memset(m, 0, sizeof *m);
   m->pf = 5000000;
   m->g2 = (uint32_t)(G2_ONE * (0.01 + 0.99 * d2 / (d2 + 50)));
   m->pr = (uint32_t)((double)m->pf * m->g2 / G2_ONE);
   m->stable = 1;
   if(n == MEAS_N_VERIFY && !now.l && !now.c && !now.sw) bypass_verified = 1;
}
uint8_t hal_abort(void) { return 0; }
void hal_wait_ms(uint8_t ms) { (void)ms; }
void hal_progress(uint16_t swr) { (void)swr; }

int main(void) {
   relays_t from = {0, 0, 0};
   cells_load();

   // memory: a damaged (empty) slot between used ones is taken first, and
   // the sequence numbers continue from the newest used slot
   tune_mem_n = 3;
   tune_mem[0] = (relays_t){10, 10, 0}; tune_mem_seq[0] = 200;
   tune_mem[1] = (relays_t){0, 0, 0};   tune_mem_seq[1] = 0;     // damaged
   tune_mem[2] = (relays_t){90, 90, 1}; tune_mem_seq[2] = 210;
   tune_best = (relays_t){50, 60, 1};
   tune_swr = 120;
   tune_mem_dirty = 0;
   remember();
   CHECK(tune_mem[1].l == 50 && tune_mem[1].c == 60);
   CHECK_EQ(tune_mem_seq[1], 211);
   CHECK_EQ(tune_mem_n, 3);
   CHECK_EQ(tune_mem_dirty, 0x2);
   // full memory: the oldest goes (mod 256 across the wrap)
   tune_mem_n = MEM_SLOTS;
   for(int i = 0; i < MEM_SLOTS; i++) {
      tune_mem[i] = (relays_t){(uint8_t)(5 + 10 * i), 5, 0};
      tune_mem_seq[i] = (uint8_t)(250 + i);                       // 250 .. 255, 0 .. 5
   }
   tune_best = (relays_t){125, 125, 1};
   remember();
   CHECK(tune_mem[0].l == 125);                                   // seq 250 was the oldest
   CHECK_EQ(tune_mem_seq[0], 6);
   // a result near a remembered one replaces it
   tune_best = (relays_t){16, 6, 0};
   remember();
   CHECK(tune_mem[1].l == 16 && tune_mem_seq[1] == 7);

   // the final comparison measures bypass, also when the budget ends the
   // search before the grid reached it (quick search effort, memory full
   // of far away settings)
   cfg[CFG_SEARCH] = 1;
   bypass_verified = 0;
   tune_run(&from, 0);
   CHECK(bypass_verified);
   CHECK(tune_best.sw == 1 && tune_best.l >= 36 && tune_best.l <= 44);
   cfg[CFG_SEARCH] = 2;
   tune_mem_n = 0;
   bypass_verified = probes = 0;
   tune_run(&from, 0);
   CHECK(bypass_verified);
   CHECK(tune_best.l == 40 && tune_best.c == 40 && tune_best.sw == 1);
   printf("normal search: %d relay steps\n", probes);
   return check_done("test_tune");
}
