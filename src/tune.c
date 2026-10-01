// Tuning: finds the relay setting with the lowest reflection.
//
// The tuner can only measure |reflection|^2 = Pr / Pf ("g2") at the setting
// the relays hold. For each position of the capacitor (input or output
// side) the L and C values that match the load form a narrow, curved
// valley in the L x C plane; where few relays are on, one relay step moves
// the value a lot and the valley jumps by many steps. A search that changes
// L and C one after the other stops on the valley's slope. This one:
//
//  1. quick retune: after a small QSY the optimum is usually close to the
//     last result - a local search from there, done if good enough
//  2. coarse grid: g2 on a logarithmic L x C grid for both capacitor
//     positions finds the valleys anywhere, not only the nearest one
//  3. local search from the best grid points that lie in different places:
//     a) pattern search with axis and diagonal moves, steps shrinking down
//        to single relay steps
//     b) valley moves: one value changed by a big, then smaller steps, the
//        other searched again along its axis - follows the valley floor
//     c) the capacitor to the other side and a pattern search there
//  4. the two best results measured again with more averaging and compared
//     with bypass; the winner is set
//
// Noise: every measurement estimates its own noise from how much its two
// halves differ. A setting only counts as better if it is better by more
// than that noise, so the search does not wander after noise, but follows
// small real improvements when the readings are clean.
//
// A button press stops the search at any point; the relays are then set to
// the best setting found so far. All measurements go through a cache, so
// each setting is switched and measured only once per tune.

#include "tune.h"
#include "cells.h"

// ---- tuning constants (the simulator may override them with -D)
#ifndef MEAS_N_GRID
#define MEAS_N_GRID    16            // ADC samples per half measurement on the grid
#endif
#ifndef MEAS_N_FINE
#define MEAS_N_FINE    32            // in the local search
#endif
#define MEAS_N_VERIFY  64            // final comparison
#ifndef HYST_BASE
#define HYST_BASE      1             // better by at least 1/256 of g2 ...
#endif
#ifndef HYST_NF
#define HYST_NF        2             // ... plus the noise of both times HYST_NF / 8
#endif
#ifndef HYST_ABS
#define HYST_ABS       64            // ... plus this (resolution near a perfect match)
#endif
#ifndef VALLEY_DIV
#define VALLEY_DIV     4             // first valley move: 1/4 of the value
#endif
#ifndef QUICK_MARGIN
#define QUICK_MARGIN   5             // quick retune kept if at most 0.05 worse than last time
#endif
#define WAIT_START     1000          // x 10 ms: wait for a carrier at the start
#define WAIT_LOST      300           // x 10 ms: carrier lost during the search
#define UNSTABLE_MAX   3             // an unsteady measurement is accepted the 3rd time

// grid and candidates per search effort (Cell 12), relay step budget
#ifndef GRID1
#define GRID1 0, 4, 16, 64
#endif
#ifndef GRID2
#define GRID2 0, 3, 10, 30, 90
#endif
#ifndef GRID3
#define GRID3 0, 2, 6, 14, 30, 60, 110
#endif
#ifndef K1
#define K1 2
#endif
#ifndef K2
#define K2 3
#endif
#ifndef K3
#define K3 4
#endif
#ifndef BUDGET1
#define BUDGET1 150
#endif
#ifndef BUDGET2
#define BUDGET2 350
#endif
#ifndef BUDGET3
#define BUDGET3 500
#endif
static const uint8_t grid1[] = {GRID1};
static const uint8_t grid2[] = {GRID2};
static const uint8_t grid3[] = {GRID3};
#define CAND_MAX 5

relays_t tune_best;
uint32_t tune_g2;
uint16_t tune_swr;

typedef struct {
   uint32_t g;                       // g2, Pr / Pf
   uint8_t sp;                       // noise estimate, relative, 1/256
} val_t;

enum { M_OK, M_ABORT, M_NO_CARRIER, M_BUDGET };

#ifdef TUNE_STATS      // simulator: relay steps per phase
long tune_stats[6];
static uint8_t phase;
#define PHASE(x) (phase = (x))
#define COUNT() (tune_stats[phase]++)
#else
#define PHASE(x)
#define COUNT()
#endif

// ---- measurement cache, open addressing; when full, new settings are
// measured but not stored. g2 is kept with 16 bits (g2 / 256: resolution
// SWR 1.008, below the measurement noise) to fit 256 entries in 1.3 kB.
#define CACHE_SIZE 256
static uint16_t cache_key[CACHE_SIZE];       // 0 = empty, else 0x8000 | sw << 14 | l << 7 | c
static uint16_t cache_g[CACHE_SIZE];
static uint8_t cache_sp[CACHE_SIZE];
static uint16_t cache_used;

static uint8_t carrier_seen;
static uint16_t steps, budget;
static uint32_t min_uw, max_uw;
static relays_t best;                         // best setting measured so far
static uint32_t best_g;

static uint16_t key_of(uint8_t l, uint8_t c, uint8_t sw) {
   return (uint16_t)(0x8000 | (uint16_t)sw << 14 | (uint16_t)l << 7 | c);
}

// slot holding key, or the empty slot where it belongs; one slot always
// stays empty, so the search ends
static uint8_t slot_of(uint16_t key) {
   uint8_t s = (uint8_t)(key * 37u ^ key >> 7);
   while(cache_key[s] != key && cache_key[s] != 0) s++;   // wraps at 256
   return s;
}

// One valid measurement at the current relay setting: waits for a carrier
// in the allowed power range, repeats unsteady ones a few times
static uint8_t take(meas_t *m, uint8_t n) {
   uint16_t wait = 0;
   uint8_t unstable = 0;
   for(;;) {
      if(hal_abort()) return M_ABORT;
      hal_sample(m, n);
      if(m->overflow) {             // detector above the ADC range: a QRP rig delivers
         m->g2 = G2_ONE;            // more at a strong mismatch, count it as useless
         m->spread = 0;
         return M_OK;
      }
      if(m->pf < min_uw || pnet_uw(m) > max_uw) {
         if(++wait > (carrier_seen ? WAIT_LOST : WAIT_START)) return M_NO_CARRIER;
         hal_wait_ms(10);
         continue;
      }
      carrier_seen = 1;
      if(m->stable || ++unstable >= UNSTABLE_MAX) return M_OK;
   }
}

// g2 of a setting, switched and measured once per tune (cached)
static uint8_t probe(uint8_t l, uint8_t c, uint8_t sw, uint8_t n, val_t *v) {
   meas_t m;
   uint16_t key = key_of(l, c, sw);
   uint8_t s = slot_of(key), r;
   if(cache_key[s] == key) {
      v->g = cache_g[s] == 0xFFFF ? G2_ONE : (uint32_t)cache_g[s] << 8;
      v->sp = cache_sp[s];
      return M_OK;
   }
   if(steps >= budget) return M_BUDGET;
   steps++;
   COUNT();
   hal_relay_set(l, c, sw);
   r = take(&m, n);
   if(r != M_OK) return r;
   v->g = m.g2;
   v->sp = m.spread;
   if(cache_used < CACHE_SIZE - 1) {
      cache_key[s] = key;
      cache_g[s] = m.g2 >= G2_ONE ? 0xFFFF : (uint16_t)(m.g2 >> 8);
      cache_sp[s] = m.spread;
      cache_used++;
   }
   if(m.g2 < best_g) {
      best_g = m.g2;
      best.l = l;
      best.c = c;
      best.sw = sw;
      hal_progress(swr_x100(best_g));
   }
   return M_OK;
}

// a is better than b by more than the noise of both
static uint8_t better(const val_t *a, const val_t *b) {
   uint32_t rel = HYST_BASE + (uint32_t)(a->sp + b->sp) * HYST_NF / 8;
   return a->g + (b->g >> 8) * rel + HYST_ABS < b->g;
}

static uint8_t clamp_add(uint8_t v, int8_t d) {
   int16_t r = (int16_t)v + d;
   if(r < 0) return 0;
   if(r > 127) return 127;
   return (uint8_t)r;
}

static uint8_t probe_at(const relays_t *p, uint8_t n, val_t *v) {
   return probe(p->l, p->c, p->sw, n, v);
}

// Minimum along one axis (0 = L, 1 = C) from p: steps growing while it
// improves, then shrinking around the best point
static uint8_t line_search(relays_t *p, val_t *v, uint8_t axis) {
   uint8_t s, r, grow = 1;
   int8_t dir;
   relays_t q;
   val_t vn;
   // downhill direction
   for(dir = 1; dir >= -1; dir -= 2) {
      q = *p;
      if(axis) q.c = clamp_add(q.c, dir); else q.l = clamp_add(q.l, dir);
      if(q.l == p->l && q.c == p->c) continue;
      r = probe_at(&q, MEAS_N_FINE, &vn);
      if(r != M_OK) return r;
      if(better(&vn, v)) break;
   }
   if(dir < -1) return M_OK;                 // neither way is better
   *p = q;
   *v = vn;
   for(s = 2; s; ) {
      q = *p;
      if(axis) q.c = clamp_add(q.c, (int8_t)(dir * (int8_t)s));
      else q.l = clamp_add(q.l, (int8_t)(dir * (int8_t)s));
      if(q.l != p->l || q.c != p->c) {
         r = probe_at(&q, MEAS_N_FINE, &vn);
         if(r != M_OK) return r;
         if(better(&vn, v)) {
            *p = q;
            *v = vn;
            if(grow && s < 32) s = (uint8_t)(s * 2);
            continue;
         }
      }
      grow = 0;                              // overshot: smaller steps
      s = (uint8_t)(s / 2);
   }
   return M_OK;
}

static const int8_t dir_l[8] = {1, -1, 0, 0, 1, -1, 1, -1};
static const int8_t dir_c[8] = {0, 0, 1, -1, -1, 1, 1, -1};

// Pattern search from p until no move of single relay steps improves
static uint8_t pattern_search(relays_t *p, val_t *v) {
   uint8_t sl = p->l / 4, sc = p->c / 4, d, moved, r;
   relays_t q;
   val_t vn;
   if(sl < 1) sl = 1;
   if(sc < 1) sc = 1;
   for(;;) {
      moved = 0;
      for(d = 0; d < 8; d++) {
         for(;;) {                           // keep going while it improves
            q = *p;
            q.l = clamp_add(p->l, (int8_t)(dir_l[d] * (int8_t)sl));
            q.c = clamp_add(p->c, (int8_t)(dir_c[d] * (int8_t)sc));
            if(q.l == p->l && q.c == p->c) break;
            r = probe_at(&q, MEAS_N_FINE, &vn);
            if(r != M_OK) return r;
            if(!better(&vn, v)) break;
            *p = q;
            *v = vn;
            moved = 1;
         }
      }
      if(!moved) {
         if(sl == 1 && sc == 1) return M_OK;
         sl = (uint8_t)((sl + 1) / 2);
         sc = (uint8_t)((sc + 1) / 2);
      }
   }
}

// Pattern search, then valley moves until neither improves
static uint8_t local_search(relays_t *p, val_t *v) {
   uint8_t axis, r, st, x;
   int8_t d;
   relays_t q;
   val_t vq;
   for(;;) {
      PHASE(2);
      r = pattern_search(p, v);
      if(r != M_OK) return r;
      PHASE(3);
      for(axis = 0; axis < 2; axis++) {      // 0: move C, search L; 1: move L, search C
         x = axis ? p->l : p->c;
         for(st = x / VALLEY_DIV > 1 ? (uint8_t)(x / VALLEY_DIV) : 1; ; st = (uint8_t)(st / 2)) {
            for(d = 1; d >= -1; d -= 2) {
               q = *p;
               if(axis) q.l = clamp_add(q.l, (int8_t)(d * (int8_t)st));
               else q.c = clamp_add(q.c, (int8_t)(d * (int8_t)st));
               if(q.l == p->l && q.c == p->c) continue;
               r = probe_at(&q, MEAS_N_FINE, &vq);
               if(r != M_OK) return r;
               r = line_search(&q, &vq, axis);
               if(r != M_OK) return r;
               if(better(&vq, v)) {
                  *p = q;
                  *v = vq;
                  goto again;
               }
            }
            if(st <= 1) break;
         }
      }
      // capacitor to the other side: near L = 0 or C = 0 both sides are
      // almost the same network, and the better match may be on the other
      q = *p;
      q.sw ^= 1;
      r = probe_at(&q, MEAS_N_FINE, &vq);
      if(r != M_OK) return r;
      if(vq.g < 2 * v->g) {                  // promising: search there
         r = pattern_search(&q, &vq);
         if(r != M_OK) return r;
         if(better(&vq, v)) {
            *p = q;
            *v = vq;
            goto again;
         }
      }
      return M_OK;
again: ;
   }
}

static uint8_t target_reached(const val_t *v) {
   return cfg[CFG_TARGET] && swr_x100(v->g) <= 100 + cfg[CFG_TARGET];
}

static void finish(const relays_t *p, uint32_t g2) {
   tune_best = *p;
   tune_g2 = g2 > G2_ONE ? G2_ONE : g2;
   tune_swr = swr_x100(tune_g2);
   hal_relay_set(p->l, p->c, p->sw);
}

// Remeasures a setting with more averaging (not cached)
static uint8_t verify(const relays_t *p, val_t *v) {
   meas_t m;
   uint8_t r;
   hal_relay_set(p->l, p->c, p->sw);
   r = take(&m, MEAS_N_VERIFY);
   v->g = m.g2;
   v->sp = m.spread;
   return r;
}

// keeps the two best results of the local searches
static void keep(relays_t *res, val_t *rv, uint8_t *n, const relays_t *p, const val_t *v) {
   uint8_t w;
   if(*n < 2) w = (*n)++;
   else {
      w = rv[0].g > rv[1].g ? 0 : 1;          // replace the worse one
      if(v->g >= rv[w].g) return;
   }
   res[w] = *p;
   rv[w] = *v;
}

uint8_t tune_run(const relays_t *from, uint16_t last_swr) {
   const uint8_t *grid;
   uint8_t ng, k, sw, i, j, a, b, r, n_cand = 0, n_res = 0;
   relays_t cand[CAND_MAX], res[2], p;
   val_t cv[CAND_MAX], rv[2], v, vb;
   uint8_t gi[CAND_MAX], gj[CAND_MAX];

   i = 0;
   do cache_key[i] = 0; while(++i);          // all 256
   cache_used = 0;
   carrier_seen = 0;
   steps = 0;
   min_uw = (uint32_t)cfg[CFG_MIN_PWR] * 100000;
   max_uw = (uint32_t)cfg[CFG_MAX_PWR] * 1000000;
   best_g = G2_ONE + 1;
   best = *from;
   vb.g = G2_ONE;
   vb.sp = 0;

   switch(cfg[CFG_SEARCH]) {
      case 1:  grid = grid1; ng = sizeof grid1; k = K1; budget = BUDGET1; break;
      case 3:  grid = grid3; ng = sizeof grid3; k = K3; budget = BUDGET3; break;
      default: grid = grid2; ng = sizeof grid2; k = K2; budget = BUDGET2; break;
   }

   // the setting the relays hold now
   PHASE(0);
   p = *from;
   r = probe_at(&p, MEAS_N_FINE, &v);
   if(r != M_OK) goto stop;
   if(target_reached(&v)) {
      finish(&p, v.g);
      return TUNE_OK;
   }

   // 1. quick retune from the last result
   if(last_swr) {
      r = local_search(&p, &v);
      if(r == M_BUDGET) {
         keep(res, rv, &n_res, &p, &v);
         goto results;
      }
      if(r != M_OK) goto stop;
      if(target_reached(&v) || swr_x100(v.g) <= last_swr + QUICK_MARGIN) {
         finish(&p, v.g);
         return TUNE_OK;
      }
      keep(res, rv, &n_res, &p, &v);
   }

   // 2. coarse grid for both capacitor positions; keep the best k points
   //    that are not grid neighbours of a better one
   PHASE(1);
   for(sw = 0; sw < 2; sw++)
      for(i = 0; i < ng; i++)
         for(j = 0; j < ng; j++) {
            p.l = grid[i];
            p.c = grid[j];
            p.sw = sw;
            r = probe_at(&p, MEAS_N_GRID, &v);
            if(r == M_BUDGET) goto results;
            if(r != M_OK) goto stop;
            if(p.l == 0 && p.c == 0 && sw == 0) vb = v;   // bypass
            for(a = 0; a < n_cand; a++)                    // a better neighbour?
               if(cand[a].sw == sw && (uint8_t)(gi[a] - i + 1) <= 2 && (uint8_t)(gj[a] - j + 1) <= 2)
                  break;
            if(a < n_cand) {
               if(v.g >= cv[a].g) continue;
               for(b = a; b + 1 < n_cand; b++) {           // drop the worse neighbour
                  cand[b] = cand[b + 1]; cv[b] = cv[b + 1]; gi[b] = gi[b + 1]; gj[b] = gj[b + 1];
               }
               n_cand--;
            }
            for(a = n_cand; a > 0 && cv[a - 1].g > v.g; a--) {   // insert sorted
               if(a < k) {
                  cand[a] = cand[a - 1]; cv[a] = cv[a - 1]; gi[a] = gi[a - 1]; gj[a] = gj[a - 1];
               }
            }
            if(a < k) {
               cand[a] = p; cv[a] = v; gi[a] = i; gj[a] = j;
               if(n_cand < k) n_cand++;
            }
         }

   // 3. local search from each candidate
   for(i = 0; i < n_cand; i++) {
      p = cand[i];
      v = cv[i];
      r = local_search(&p, &v);
      if(r != M_OK && r != M_BUDGET) goto stop;
      keep(res, rv, &n_res, &p, &v);         // with the budget used up: as far as it got
      if(r == M_BUDGET || target_reached(&v)) break;
   }
results:
   if(n_res == 0) {                           // budget used up before any result
      p = best;
      v.g = best_g;
      keep(res, rv, &n_res, &p, &v);
   }

   // 4. the best results again with more averaging; bypass if not better
   PHASE(4);
   for(i = 0; i < n_res; i++) {
      r = verify(&res[i], &rv[i]);
      if(r != M_OK) goto stop;
   }
   i = n_res > 1 && rv[1].g < rv[0].g ? 1 : 0;
   if(!better(&rv[i], &vb)) {
      p.l = p.c = p.sw = 0;
      finish(&p, vb.g);
      return vb.g >= G2_ONE ? TUNE_NO_MATCH : TUNE_OK;
   }
   finish(&res[i], rv[i].g);
   return TUNE_OK;

stop:   // aborted or carrier gone: best setting so far
   finish(&best, best_g);
   return r == M_ABORT ? TUNE_ABORTED : TUNE_NO_CARRIER;
}
