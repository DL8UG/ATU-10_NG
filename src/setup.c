// Setup menu on the device.
//
// Entered when the button is still held after the greeting (keep it
// pressed when switching on). Then:
//   short press  next value of the setting shown
//   long press   next setting; after the last: SAVE, HEX VALUES, EXIT
// On SAVE / HEX VALUES / EXIT a short press does it. Without a press for
// 60 s the menu ends without saving.

#include "board.h"
#include "timer.h"
#include "cells.h"
#include "settings.h"
#include "buttons.h"
#include "display.h"
#include "setup.h"
#include "text.h"

#define TIMEOUT_MS 60000

enum { U_MIN, U_MS, U_W10, U_W, U_DSWR, U_ONOFF, U_DIV10, U_CALA, U_X10MS, U_TARGET, U_EFFORT };

static const char *const names[CELL_COUNT] = {
   "DISPLAY OFF AFTER", "POWER OFF AFTER", "RELAY PULSE", "MIN. TUNE POWER",
   "MAX. TUNE POWER", "AUTO TUNE: SWR CHANGE", "AUTO TUNE", "CALIBRATION B (1 W)",
   "CALIBRATION A (10 W)", "POWER PEAK HOLD", "TUNE TARGET SWR", "SEARCH EFFORT",
};
static const uint8_t units[CELL_COUNT] = {
   U_MIN, U_MIN, U_MS, U_W10, U_W, U_DSWR, U_ONOFF, U_DIV10, U_CALA, U_X10MS, U_TARGET, U_EFFORT,
};

// values offered per setting, ascending (other values from the hex file
// are shown and left when stepping on)
static const uint8_t v_disp[]   = {0, 1, 2, 3, 5, 10, 15, 20, 30, 45, 60, 99};
static const uint8_t v_off[]    = {0, 5, 10, 15, 20, 30, 45, 60, 90, 99};
static const uint8_t v_relay[]  = {3, 4, 5, 6, 7, 8, 9, 10, 12, 15, 20, 25, 30};
static const uint8_t v_minp[]   = {1, 2, 3, 5, 7, 10, 15, 20, 30, 50};
static const uint8_t v_maxp[]   = {3, 5, 8, 10, 12, 15, 20};
static const uint8_t v_delta[]  = {11, 12, 13, 15, 20, 25, 30};
static const uint8_t v_onoff[]  = {0, 1};
static const uint8_t v_calb[]   = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 15, 20};
static const uint8_t v_cala[]   = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 25, 30, 40, 50};
static const uint8_t v_peak[]   = {10, 20, 30, 40, 60, 80, 99};
static const uint8_t v_target[] = {0, 2, 3, 5, 8, 10, 15, 20};
static const uint8_t v_effort[] = {1, 2, 3};
static const uint8_t *const lists[CELL_COUNT] = {
   v_disp, v_off, v_relay, v_minp, v_maxp, v_delta, v_onoff, v_calb, v_cala, v_peak, v_target, v_effort,
};
static const uint8_t list_len[CELL_COUNT] = {
   sizeof v_disp, sizeof v_off, sizeof v_relay, sizeof v_minp, sizeof v_maxp, sizeof v_delta,
   sizeof v_onoff, sizeof v_calb, sizeof v_cala, sizeof v_peak, sizeof v_target, sizeof v_effort,
};

static char txt[11];

static char *fmt(char *p, uint16_t v, uint8_t dec) {
   return fmt_num(p, v, dec, 0);
}

static void cat(char *p, const char *s) {
   while((*p++ = *s++)) continue;
}

static void value_text(uint8_t i, uint8_t v) {
   char *p = txt;
   switch(units[i]) {
      case U_MIN:    if(v == 0) cat(p, "NEVER"); else cat(fmt(p, v, 0), " MIN"); break;
      case U_MS:     cat(fmt(p, v, 0), " MS"); break;
      case U_W10:    cat(fmt(p, v, 1), " W"); break;
      case U_W:      cat(fmt(p, v, 0), " W"); break;
      case U_DSWR:   fmt(p, (uint16_t)(v - 10), 1); break;
      case U_ONOFF:  cat(p, v ? "ON" : "OFF"); break;
      case U_DIV10:  fmt(p, v, 1); break;
      case U_CALA:   fmt(p, (uint16_t)(100 + v), 2); break;
      case U_X10MS:  cat(fmt(p, (uint16_t)v * 10, 0), " MS"); break;
      case U_TARGET: if(v == 0) cat(p, "FULL"); else fmt(p, (uint16_t)(100 + v), 2); break;
      default:       cat(p, v == 1 ? "QUICK" : v == 3 ? "THOROUGH" : "NORMAL"); break;
   }
}

#define PAGE_SAVE  CELL_COUNT
#define PAGE_HEX   (CELL_COUNT + 1)
#define PAGE_EXIT  (CELL_COUNT + 2)
#define PAGES      (CELL_COUNT + 3)

static void show(uint8_t page) {
   char head[8];
   disp_clear();
   if(page < CELL_COUNT) {
      cat(fmt(head, (uint16_t)(page + 1), 0), "/12");
      disp_small(0, 0, head);
      disp_small(8, 0, names[page]);
      value_text(page, cfg[page]);
      disp_big(LINE2, 0, txt);
   }
   else {
      disp_small(0, 0, "SHORT PRESS:");
      disp_big(LINE2, 0, page == PAGE_SAVE ? "SAVE" : page == PAGE_HEX ? "HEX VALUES" : "EXIT");
   }
   disp_flush();
}

static uint8_t next_value(uint8_t i, uint8_t v) {
   const uint8_t *l = lists[i];
   uint8_t k;
   for(k = 0; k < list_len[i]; k++)
      if(l[k] > v) return l[k];
   return l[0];                       // wrap around
}

void setup_run(void) {
   uint8_t page = 0, ev;
   uint32_t t = tick_ms();
   disp_clear();
   disp_big(LINE1, 34, "SETUP");
   disp_flush();
   while(BUTTON_DOWN) CLRWDT();       // let go first
   buttons_clear();
   show(page);
   for(;;) {
      CLRWDT();
      ev = buttons_event();
      if(ev == EV_NONE) {
         if(tick_ms() - t >= TIMEOUT_MS) break;
         continue;
      }
      t = tick_ms();
      if(ev == EV_LONG) {
         page = (uint8_t)((page + 1) % PAGES);
         show(page);
         while(BUTTON_DOWN) CLRWDT();
         buttons_clear();
      }
      else if(ev == EV_SHORT) {
         if(page < CELL_COUNT) {
            cfg[page] = next_value(page, cfg[page]);
            show(page);
         }
         else {
            if(page == PAGE_SAVE) settings_save();
            if(page == PAGE_HEX) settings_defaults();
            break;
         }
      }
   }
   settings_load();                   // without SAVE: back to the values in effect
}
