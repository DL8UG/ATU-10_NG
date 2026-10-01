// Placeholder until the search is written: stays in bypass
#include "tune.h"

relays_t tune_best;
uint32_t tune_g2 = G2_ONE;
uint16_t tune_swr = SWR_MAX;

uint8_t tune_run(const relays_t *from, uint8_t quick) {
   (void)from; (void)quick;
   tune_best.l = tune_best.c = tune_best.sw = 0;
   hal_relay_set(0, 0, 0);
   return TUNE_NO_MATCH;
}
