// Search internals: memory slots, final comparison with bypass
#include "check.h"
#include <string.h>
#include <stdint.h>
#include <math.h>

volatile uint8_t Cells[16] = {
   0x05, 0x30, 0x07, 0x10, 0x15, 0x13, 0x01, 0x04, 0x14, 0x60, 0x05, 0x02 };
#include "../src/cells.c"
#include "../src/meas_math.c"
#include "../src/tune.c"

static relays_t now;                 // relays as set
static int bypass_verified, probes, model, grid_end_seen;
void hal_relay_set(uint8_t l, uint8_t c, uint8_t sw) {
   now.l = l; now.c = c; now.sw = sw; probes++;
   if(l == 64 && c == 64 && sw == 1) grid_end_seen = 1;   // last point of grid 1
}
static double g2_of(void) {
   double d2;
   switch(model) {
   case 1:   // a long valley on side 0 along C = 64 down to SWR 2.3 at L 0, and a
             // single good setting (SWR 1.2) at L 0, C 4 that only the grid finds
      if(now.sw == 0 && now.l == 0 && now.c == 4) return 0.01;
      if(now.sw == 0) return 0.15 + 0.002 * now.l + 0.01 * fabs(now.c - 64.0);
      return 0.9;
   case 2:   // the network changes nothing: SWR 3.0 everywhere
      return 0.25;
   case 3:   // the same at SWR 1.10
      return (0.1 / 2.1) * (0.1 / 2.1);
   default:  // best match at L 40, C 40, side 1
      d2 = (now.l - 40.0) * (now.l - 40.0) + (now.c - 40.0) * (now.c - 40.0) + (now.sw ? 0 : 900);
      return 0.01 + 0.99 * d2 / (d2 + 50);
   }
}
void hal_sample(meas_t *m, uint8_t n) {
   double g = g2_of();
   memset(m, 0, sizeof *m);
   m->pf = 5000000;
   m->g2 = (uint32_t)(G2_ONE * (g < 1 ? g : 1));
   m->pr = (uint32_t)((double)m->pf * m->g2 / G2_ONE);
   m->stable = 1;
   if(n == MEAS_N_VERIFY && !now.l && !now.c && !now.sw) bypass_verified = 1;
}
uint8_t hal_abort(void) { return 0; }
void hal_wait_ms(uint8_t ms) { (void)ms; }
void hal_progress(uint16_t swr) { (void)swr; }

int main(void) {
   relays_t from = {0, 0, 0};
   uint8_t r;
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

   // the budget ends in the grid after a quick retune that did not reach
   // its old SWR: the best grid point is compared too, not only the
   // quick retune's result
   model = 1;
   cfg[CFG_SEARCH] = 1;
   tune_mem_n = 1;
   tune_mem[0] = (relays_t){127, 64, 0};
   tune_mem_swr[0] = 20;                   // SWR 1.20 when found
   grid_end_seen = probes = 0;
   r = tune_run(&from, 0);
   printf("budget ends in the grid: %d relay steps, result %d/%d/%d SWR %d.%02d\n", probes,
          tune_best.l, tune_best.c, tune_best.sw, tune_swr / 100, tune_swr % 100);
   CHECK(!grid_end_seen);                  // the budget did end in the grid
   CHECK_EQ(r, TUNE_OK);
   CHECK(tune_best.l == 0 && tune_best.c == 4 && tune_best.sw == 0);

   // quick retune at a remembered setting that measures as well as when
   // it was found: done without the grid - but not with Cell 11 = 0
   // (always the full search)
   model = 0;
   tune_mem[0] = (relays_t){40, 40, 1};
   tune_mem_swr[0] = 22;                   // SWR 1.22, what the load gives there
   grid_end_seen = 0;
   r = tune_run(&from, 0);
   CHECK_EQ(r, TUNE_OK);
   CHECK(!grid_end_seen);                  // target 1.05: the quick retune is enough
   cfg[CFG_TARGET] = 0;
   r = tune_run(&from, 0);
   CHECK_EQ(r, TUNE_OK);
   CHECK(grid_end_seen);                   // target 0: the grid ran too
   CHECK(tune_best.l == 40 && tune_best.c == 40 && tune_best.sw == 1);
   cfg[CFG_TARGET] = 5;

   // nothing is better than bypass: NO MATCH if bypass is above SWR 1.20,
   // else it is simply good as it is
   model = 2;
   r = tune_run(&from, 0);
   CHECK_EQ(r, TUNE_NO_MATCH);
   CHECK(tune_best.l == 0 && tune_best.c == 0 && tune_best.sw == 0);
   CHECK(now.l == 0 && now.c == 0 && now.sw == 0);
   model = 3;
   r = tune_run(&from, 0);
   CHECK_EQ(r, TUNE_OK);
   CHECK(now.l == 0 && now.c == 0 && now.sw == 0);
   return check_done("test_tune");
}
