#include "board.h"
#include "display.h"
#include "oled.h"
#include "i2c_soft.h"
#include "font5x8.h"
#include "timer.h"
#include "battery.h"

#define W      128
#define PAGES  4

static uint8_t fb[PAGES][W];         // bit 0 = top pixel of a page
static uint8_t dirty_lo[PAGES], dirty_hi[PAGES];   // changed columns, lo > hi: none
static uint8_t on, faults;
// Display not answering: restart (power down and up, initialize). The
// first restart comes at once, the next ones after 2, 4, 8 .. 64 s, so a
// missing or broken display does not slow down the tuner. Meanwhile a
// short address ping once a second finds a display that answers again
// (plugged in, recovered) and restarts it right away.
static uint8_t restart;              // a restart is pending
static uint8_t gone;                 // the ping found no display since the last restart
static uint8_t backoff;              // restarts without success in a row
static uint32_t t_restart, t_probe;

static void mark(uint8_t page, uint8_t x0, uint8_t x1) {
   if(x0 < dirty_lo[page]) dirty_lo[page] = x0;
   if(x1 > dirty_hi[page]) dirty_hi[page] = x1;
}

static void mark_all(void) {
   uint8_t p;
   for(p = 0; p < PAGES; p++) {
      dirty_lo[p] = 0;
      dirty_hi[p] = W - 1;
   }
}

#ifndef __XC8
const uint8_t *disp_fb(void) {       // the picture, for the tests on the PC
   return &fb[0][0];
}
#endif

void disp_refresh(void) {
   mark_all();
}

uint8_t disp_is_on(void) {
   return on;
}

void disp_power(uint8_t pwr) {
   if(!pwr) {
      OLED_PWR = 0;
      I2C_SCL = 1;                   // released: driven low, they would draw
      I2C_SDA = 1;                   // current through the module's pull-ups
      ANSELA |= 0b00001100;          // no input buffer on the open lines
      on = 0;
      return;
   }
   ANSELA &= (uint8_t)~0b00001100;
   OLED_PWR = 1;
   delay_ms(200);                    // module supply settles
   faults = 0;
   restart = oled_init();            // no answer: restart pending at once
   t_restart = tick_ms();
   on = 1;
   mark_all();
   disp_flush();                     // the picture first, then the display on
   if(!restart && oled_on()) restart = 1;
}

void disp_clear(void) {
   uint8_t p, x;
   for(p = 0; p < PAGES; p++)
      for(x = 0; x < W; x++) fb[p][x] = 0;
   mark_all();
}

// sets the 8 pixel column (bit 0 at y) at x to the bits of b
// marks only bytes that change: drawing the same again sends nothing
static void put8(uint8_t x, uint8_t y, uint8_t b, uint8_t h) {
   uint8_t p = y >> 3, s = y & 7, mask = (uint8_t)((1u << h) - 1), n;
   uint16_t m = (uint16_t)mask << s, v = (uint16_t)(b & mask) << s;
   if(x >= W) return;
   if(p < PAGES) {
      n = (uint8_t)((fb[p][x] & ~m) | v);
      if(n != fb[p][x]) {
         fb[p][x] = n;
         mark(p, x, x);
      }
   }
   if(s && p + 1 < PAGES) {
      n = (uint8_t)((fb[p + 1][x] & ~(m >> 8)) | v >> 8);
      if(n != fb[p + 1][x]) {
         fb[p + 1][x] = n;
         mark((uint8_t)(p + 1), x, x);
      }
   }
}

static const uint8_t *glyph(char ch) {
   uint8_t i = (uint8_t)((uint8_t)ch - FONT_FIRST);
   if(i >= FONT_COUNT) i = 0;                 // unknown: blank
   return &font5x8[i * 5];
}

// doubles the low 4 bits of n: bit i -> bits 2i and 2i + 1
static uint8_t dbl(uint8_t n) {
   uint8_t r = 0, i;
   for(i = 0; i < 4; i++)
      if(n & (1 << i)) r |= (uint8_t)(3 << (2 * i));
   return r;
}

// Big text: the 5 x 8 font doubled to 10 x 16, 2 columns gap, from pixel
// row y. LINE2 starts 2 pixels lower than page 2 (a gap between the
// lines); its bottom font row (always empty) falls off the display.
void disp_big(uint8_t y, uint8_t x, const char *s) {
   uint8_t i, col, lo, hi;
   const uint8_t *g;
   for(; *s; s++, x += 12) {
      g = glyph(*s);
      for(i = 0; i < 12; i++) {
         col = i < 10 ? g[i / 2] : 0;
         lo = dbl(col & 0x0F);
         hi = dbl(col >> 4);
         put8((uint8_t)(x + i), y, lo, 8);
         put8((uint8_t)(x + i), (uint8_t)(y + 8), hi, 8);
      }
   }
}

// Small text: the 5 x 8 font, 1 column gap, from pixel row y
void disp_small(uint8_t y, uint8_t x, const char *s) {
   uint8_t i;
   const uint8_t *g;
   for(; *s; s++, x += 6) {
      g = glyph(*s);
      for(i = 0; i < 6; i++) put8((uint8_t)(x + i), y, i < 5 ? g[i] : 0, 8);
   }
}

// Battery symbol at the right edge (x 115..125), filled from the bottom:
// empty where the tuner switches off (BATT_OFF_MV, 3.0 V) .. 4.2 V full
#define BAT_FULL_MV  4200
static uint8_t bat_px(uint8_t x, uint8_t y, uint8_t fill) {
   if(y <= 1) return x >= 118 && x <= 122;                 // cap
   if(y == 2 || y == 31 || x <= 116 || x >= 124) return 1; // frame, 2 px at the sides
   if(x == 117 || x == 123 || y == 3 || y == 30) return 0; // gap inside the frame
   return y > 29 - fill;                                   // charge, 0..26 rows
}

// mv 0: no symbol (it blinks while the battery runs low)
void disp_battery(uint16_t mv) {
   uint8_t x, p, k, b, fill = 0;
   if(mv) {
      if(mv < BATT_OFF_MV) mv = BATT_OFF_MV;
      if(mv > BAT_FULL_MV) mv = BAT_FULL_MV;
      fill = (uint8_t)((uint32_t)(mv - BATT_OFF_MV) * 26 / (BAT_FULL_MV - BATT_OFF_MV));
   }
   for(x = 115; x <= 125; x++)
      for(p = 0; p < PAGES; p++) {
         b = 0;
         for(k = 0; k < 8; k++)
            if(mv && bat_px(x, (uint8_t)(p * 8 + k), fill)) b |= (uint8_t)(1 << k);
         put8(x, (uint8_t)(p * 8), b, 8);
      }
}

// Small battery behind the SWR value (relay view): frame x 113..123,
// rows 22..28, cap x 124, filled from the left, 0..7 columns from
// BATT_OFF_MV to 4.2 V; mv 0: nothing (it blinks)
void disp_bat_small(uint16_t mv) {
   uint8_t x, col, fill = 0;
   if(mv) {
      if(mv < BATT_OFF_MV) mv = BATT_OFF_MV;
      if(mv > BAT_FULL_MV) mv = BAT_FULL_MV;
      fill = (uint8_t)((uint16_t)(mv - BATT_OFF_MV) * 7 / (BAT_FULL_MV - BATT_OFF_MV));
   }
   for(x = 113; x <= 124; x++) {                  // bit 0 = row 22
      if(!mv) col = 0;
      else if(x == 124) col = 0x1C;               // cap, rows 24..26
      else if(x == 113 || x == 123) col = 0x7F;   // frame
      else if(x == 114 || x == 122) col = 0x41;
      else col = x - 115 < fill ? 0x5D : 0x41;    // charge rows 24..26
      put8(x, 22, col, 7);
   }
}

// 7 relay cells of 5 x 5 pixels from pixel row y, 7 columns apart from x:
// filled for a set bit (bit 0 left), else only the frame
void disp_cells(uint8_t y, uint8_t x, uint8_t bits) {
   uint8_t i, k;
   for(i = 0; i < 7; i++, bits >>= 1)
      for(k = 0; k < 7; k++, x++)
         put8(x, y, k >= 5 ? 0 : (bits & 1) || k == 0 || k == 4 ? 0x1F : 0x11, 5);
}

static void send_page(uint8_t p) {
   uint8_t lo = dirty_lo[p], hi = dirty_hi[p];
   dirty_lo[p] = 0xFF;
   dirty_hi[p] = 0;
   if(lo > hi) return;
   if(oled_write(p, lo, &fb[p][lo], (uint8_t)(hi - lo + 1))) {
      // no answer: free the bus, send everything again; after several
      // faults in a row power the display down and up
      mark_all();
      if(++faults >= 5) restart = 1;       // done by disp_service (hardware stack)
      else i2c_init();
   }
   else faults = backoff = restart = 0;
}

void disp_service(void) {
   static uint8_t next;
   uint8_t n;
   if(!on) return;
   if(restart) {
      if(backoff && tick_ms() - t_restart < (uint32_t)2000 << (backoff - 1)) {
         if(tick_ms() - t_probe < 1000) return;
         t_probe = tick_ms();
         // only a display that was gone and is back again cuts the wait
         // short; one that answers the ping but not the data waits
         if(!oled_present()) { gone = 1; return; }
         if(!gone) return;
      }
      gone = 0;
      if(backoff < 6) backoff++;
      disp_power(0);
      delay_ms(300);
      disp_power(1);                       // sets restart again if it fails
      return;
   }
   for(n = 0; n < PAGES; n++) {       // the next page with changes
      next = (uint8_t)((next + 1) % PAGES);
      if(dirty_lo[next] <= dirty_hi[next]) {
         send_page(next);
         return;
      }
   }
}

void disp_flush(void) {
   uint8_t p;
   if(!on || restart) return;         // nothing to a display that does not answer
   for(p = 0; p < PAGES; p++) send_page(p);
}
