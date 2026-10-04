// ATU-10 NG firmware: main program.
//
// One main loop, no blocking waits except while tuning: it measures, keeps
// the peak power for the display, updates the display a little at a time,
// handles the button and the external interface, and runs the timers.

#include "board.h"
#include "timer.h"
#include "cells.h"
#include "meas.h"
#include "relays.h"
#include "tune.h"
#include "nvm.h"
#include "buttons.h"
#include "display.h"
#include "settings.h"
#include "setup.h"
#include "text.h"
#include "version.h"

#define BATT_MS     3000       // battery check and LED blink
#define WATCH_MS    50         // measurement for the display / auto tune
#define SHOW_MS     150        // display update of power and SWR
#define REFRESH_MS  30000      // send the whole picture again
#define BATT_N       3         // readings in a row below a threshold (6 s, one dip does not)
#define BATT_HYST    50        // a level ends only this far above its threshold (noise)
#define RECHARGE_N   3         // RECHARGE at every 3rd battery reading (9 s), the SWR stays readable
#define AUTO_STEADY 4          // auto tune after this many measurements in a row
#define AUTO_HOLD   3000       // ms after a tune without auto tune
#define RESUME_MS   60000      // a tune within this time continues an interrupted one
#define SWR_SHOW_UW 100000     // SWR shown from 0.1 W (the lowest Cell 4), also below Cell 4
#define HINT_MS     1000       // a tune without a suitable carrier: the display says why
#define CARRIER_MS  (WAIT_START * 10UL)   // ... and gives up, as tune_run would
#define CARRIER_ON_MS 200      // a carrier that comes while waiting: this long, then tune
#define SETUP_HOLD  100        // x 10 ms held at the end of the greeting: setup menu

static uint32_t t_batt, t_watch, t_show, t_refresh, t_active, t_led, t_msg, t_tuned;
static uint8_t led_on, msg_on, auto_cnt, auto_tune, go_off, low_cnt;
enum { B_OK, B_WARN, B_LOW, B_OFF };
static uint8_t batt_lvl, batt_blink;   // battery level, symbol hidden this time
static uint8_t row_lvl;        // the mildest level of the readings in the row so far
static uint8_t recharge_cnt;   // battery readings until RECHARGE shows again
static uint8_t msg_batt;       // the message shown is RECHARGE: OVERLOAD may replace it
static uint16_t swr_ref;       // SWR of the last tune, reference for auto tune
static uint16_t shown_pwr = 0xFFFF, shown_swr = 0xFFFF;
static meas_t peak;            // peak hold for the display
static uint32_t t_peak;
static uint16_t swr_last;      // last SWR measured with enough power
static uint16_t swr_disp;      // last SWR measured from SWR_SHOW_UW on, for the display
static uint8_t resume;         // the last tune can be continued (tune_resumable)

// ms since the tick value t. Always read the clock fresh: a timestamp set
// in between (wake, tune) may be later than a value read before, and the
// unsigned difference would then be huge.
static uint32_t since(uint32_t t) {
   return tick_ms() - t;
}

// ---------------------------------------------------------------- text

static char buf[8];

static const char *num(uint16_t v, uint8_t dec, uint8_t width) {
   fmt_num(buf, v, dec, width);
   return buf;
}

// ---------------------------------------------------------------- screen

// The label clears everything left of the value (x 0..59): whatever was
// there before (TUNE, a message) must not leave pixels in the gaps
static void show_swr_label(void) {
   disp_big(LINE2, 0, "     ");
   disp_big(LINE2, 0, st.bypass ? "BYP" : "SWR");
   disp_big(LINE2, 42, "=");
}

static void show_screen(void) {
   disp_clear();
   disp_big(LINE1, 0, "PWR");
   disp_big(LINE1, 42, "=");
   disp_big(LINE1, 96, "W");
   show_swr_label();
   disp_battery(vbat_mv);
   shown_pwr = shown_swr = 0xFFFF;
}

static void show_power(uint16_t p10) {        // 0.1 W
   if(p10 == shown_pwr) return;
   shown_pwr = p10;
   if(p10 < 100) disp_big(LINE1, 60, num(p10, 1, 3));
   else disp_big(LINE1, 60, num((uint16_t)((p10 + 5) / 10), 0, 3));
}

static void show_swr(uint16_t swr) {          // SWR x 100, 0 = none
   if(swr == shown_swr || msg_on) return;
   shown_swr = swr;
   if(swr) disp_big(LINE2, 60, num(swr, 2, 4));
   else disp_big(LINE2, 60, "-.--");           // none measured yet
}

// a message in place of the SWR line for 'ms'
static void message(const char *s, uint16_t ms) {
   msg_batt = 0;
   disp_big(LINE2, 0, "         ");
   disp_big(LINE2, 0, s);
   msg_on = 1;
   t_msg = tick_ms() + ms;
}

static void message_end(void) {
   msg_on = 0;
   disp_big(LINE2, 0, "         ");
   show_swr_label();
   shown_swr = 0xFFFF;
}

static void message_wait(const char *s, uint16_t ms) {   // blocking, for start-up
   message(s, ms);
   disp_flush();
   delay_ms(ms);
   message_end();
}

// ---------------------------------------------------------------- activity

static void wake(void) {                       // something happened: timers restart
   t_active = tick_ms();
   if(!disp_is_on()) disp_power(1);            // the framebuffer still holds the picture
}

// ---------------------------------------------------------------- tuning

static void save_state(void) {
   st.r = rel;
   state_save();
   mem_save();
}

// called by tune.c
void hal_progress(uint16_t swr) {
   msg_on = 0;
   show_swr(swr);
   disp_flush();
}
uint8_t hal_abort(void) {
   uint8_t ev = buttons_event();
   // power off, or the transceiver's bypass pulse: after the stop (a tune
   // request from the transceiver while tuning changes nothing)
   if(ev == EV_XLONG || ev == EV_EXT_SHORT) {
      buttons_unget(ev);
      return 1;
   }
   return ev == EV_SHORT || ev == EV_LONG;
}

// Carrier power for tuning, the same limits as in tune_run: P_OK or why
// not (P_NONE below SWR_SHOW_UW, P_LOW below Cell 4, P_HIGH above Cell 5)
enum { P_OK, P_NONE, P_LOW, P_HIGH };
static uint8_t tune_power(const meas_t *m) {
   if(m->pf < SWR_SHOW_UW) return P_NONE;
   if(m->pf < TUNE_MIN_UW) return P_LOW;
   if(pnet_uw(m) > TUNE_MAX_UW) return P_HIGH;
   return P_OK;
}

// two small lines right of TUNE, where the SWR so far follows
static void hint(const char *a, const char *b) {
   disp_small(LINE2 - 1, 60, "        ");
   disp_small(LINE2 + 7, 60, "        ");
   disp_small(LINE2 - 1, 60, a);
   disp_small(LINE2 + 7, 60, b);
   disp_flush();
}

// Waits up to CARRIER_MS for a carrier the tune can use; after HINT_MS
// without one the display says why, and the power line shows the power
// meanwhile. A carrier that is there at once starts the tune at once
// (auto tune has checked it already); one that comes while waiting must
// last CARRIER_ON_MS, else a blip or a CW element would start a tune that
// waits again in tune_run, without a hint. TUNE_OK: there is one, else
// TUNE_NO_CARRIER (*why: the reason shown last) or TUNE_ABORTED (button,
// as during the tune).
static uint8_t wait_carrier(uint8_t *why) {
   meas_t m;
   uint8_t p, shown = P_OK, waited = 0, r = TUNE_OK;
   uint32_t t = tick_ms(), t_bad = t;
   for(;;) {
      meas_take(&m, 4);
      p = tune_power(&m);
      if(p != P_OK) {
         waited = 1;
         t_bad = tick_ms();
      }
      else if(!waited || since(t_bad) >= CARRIER_ON_MS) break;
      show_power(pwr_x10(m.pf));
      disp_flush();
      if(hal_abort()) { r = TUNE_ABORTED; break; }
      if(p != P_OK) {                          // a carrier on its way: the hint stays
         if(since(t) >= CARRIER_MS) {
            r = TUNE_NO_CARRIER;
            *why = shown;
            break;
         }
         // once a power was there, its hint stays when it goes away
         // (CW keying, a carrier near 0.1 W): no flicker with WAITING
         if(since(t) >= HINT_MS && p != shown && (p != P_NONE || shown == P_OK)) {
            shown = p;
            if(p == P_NONE) hint("WAITING", "FOR RF");
            else hint("POWER", p == P_LOW ? "TOO LOW" : "TOO HIGH");
         }
      }
      delay_ms(10);
   }
   if(shown != P_OK) hint("", "");
   return r;
}

static void do_tune(void) {
   uint8_t r, same, why = P_NONE;
   relays_t from = rel;
   wake();
   LED_GREEN = 0;
   EXT_KEY_OUT = 0;                            // tells the transceiver: tuning
   msg_on = 0;
   disp_big(LINE2, 0, "         ");            // a message may still be there
   disp_big(LINE2, 0, "TUNE");                 // the SWR so far follows at the right
   shown_swr = 0xFFFF;                         // blanked: draw it even if unchanged
   disp_flush();
   r = wait_carrier(&why);
   if(r == TUNE_OK) {
      tune_resume = resume && since(t_tuned) < RESUME_MS;
      // in bypass the relays hold no tune result (the memory has it)
      r = tune_run(&rel, !st.bypass && st.last_swr ? (uint16_t)(100 + st.last_swr) : 0);
      // Carrier gone while the search still measured new settings: the
      // next tune within RESUME_MS goes on with this search, and auto tune
      // starts it with the next carrier (SWR above 1.20). The step budget
      // counts on, so the chain ends.
      resume = tune_resumable;
   }
   else resume = 0;                            // nothing measured: as a tune without a carrier
   same = rel.l == from.l && rel.c == from.c && rel.sw == from.sw;
   if(r != TUNE_OK && r != TUNE_NO_MATCH && same) {
      // Stopped without a change (no carrier, or stopped before anything
      // better was found, e.g. by the long press that goes on to power
      // off): everything stays as it was, the bypass too, and nothing
      // is written to the EEPROM. Auto tune does not start the same tune
      // again at the present SWR: it would end the same way (stopped, too
      // much power, or too little carrier to find anything better)
      if(swr_last) swr_ref = swr_last;
   }
   else {
      // a result ends the bypass
      if(r == TUNE_OK || r == TUNE_NO_MATCH || rel.l || rel.c || rel.sw) st.bypass = 0;
      // last_swr: SWR - 1.00 in hundredths, 1..255 (0 = no tune result,
      // so a perfect 1.00 is kept as 1.01)
      if(!st.bypass)
         st.last_swr = (r == TUNE_OK && (rel.l || rel.c))
                       ? (uint8_t)(tune_swr - 100 > 255 ? 255 : tune_swr > 101 ? tune_swr - 100 : 1) : 0;
      swr_ref = tune_swr;
      swr_last = swr_disp = tune_swr;
      save_state();
   }
   if(resume) swr_ref = 0;                     // auto tune goes on with the search
   show_swr_label();
   shown_swr = 0xFFFF;
   show_swr(swr_disp);
   if(r == TUNE_NO_CARRIER)                    // as the hint said while waiting
      message(why == P_LOW ? "TOO LOW" : why == P_HIGH ? "TOO HIGH" : "NO POWER", 2000);
   else if(r == TUNE_NO_MATCH) message("NO MATCH", 2000);
   else if(r == TUNE_ABORTED) message("STOP", 1000);
   else if(r == TUNE_OVERLOAD) message("OVERLOAD", 2000);
   disp_refresh();                             // RF may have garbled the picture
   EXT_KEY_OUT = 1;
   LED_GREEN = 1;
   auto_cnt = 0;
   t_active = t_tuned = tick_ms();
}

static void bypass_toggle(uint8_t on) {
   if(on && !st.bypass) {                      // keep the setting to come back to
      st.byp = rel;
      st.bypass = 1;
      relays_set(0, 0, 0);
   }
   else if(!on && st.bypass) {
      st.bypass = 0;
      relays_set(st.byp.l, st.byp.c, st.byp.sw);
   }
   else return;
   save_state();
   swr_ref = st.bypass ? 0 : (uint16_t)(st.last_swr ? 100 + st.last_swr : 0);
   show_swr_label();
   message(st.bypass ? "BYPASS" : "TUNED", 800);
   auto_cnt = 0;
}

// ---------------------------------------------------------------- power

// centred on the 128 px: big text 12 px, small text 6 px per character,
// without the gap after the last one
#define CENTRE_BIG(s)   ((128 - 12 * (sizeof(s) - 1) + 2) / 2)
#define CENTRE_SMALL(s) ((128 - 6 * (sizeof(s) - 1) + 1) / 2)
#define GREET_FW "NG " FW_VERSION

// two pages of 2 s: the hardware, then the firmware and its version
static void greeting(void) {
   disp_clear();
   disp_big(LINE1, CENTRE_BIG("ATU-10"), "ATU-10");
   disp_small(24, CENTRE_SMALL("HARDWARE BY N7DDC"), "HARDWARE BY N7DDC");
   disp_flush();
   LED_GREEN = 0;
   delay_ms(2000);
   disp_clear();
   disp_big(LINE1, CENTRE_BIG(GREET_FW), GREET_FW);
   disp_small(24, CENTRE_SMALL("FIRMWARE BY DL8UG"), "FIRMWARE BY DL8UG");
   disp_flush();
   delay_ms(2000);
   LED_GREEN = 1;
}

static void start_screen(void) {
   disp_power(1);
   greeting();
   if(btn_held >= SETUP_HOLD) setup_run();     // held through the greeting
   show_screen();
   buttons_clear();                            // the press that woke us is no event
   t_active = tick_ms();
}

// Battery level of a reading (BATT_N readings in a row make it the level)
static uint8_t batt_level(uint16_t mv) {
   return mv < BATT_OFF_MV ? B_OFF : mv < BATT_LOW_MV ? B_LOW : mv < BATT_WARN_MV ? B_WARN : B_OK;
}

// At the start and after waking: the level at once (an almost empty
// battery shows RECHARGE at once), switched off only by BATT_N readings;
// low readings from before count no more
static void batt_start(void) {
   batt_lvl = batt_level(meas_battery());
   if(batt_lvl == B_OFF) batt_lvl = B_LOW;
   low_cnt = 0;
   batt_blink = 0;
   recharge_cnt = 0;
}

// Sleeps until the button is held for 1.6 s. The relays keep their setting
// without power; the watchdog is off while sleeping. The caller starts the
// display again (start_screen) - one hardware stack level less.
static void power_off(void) {
   uint8_t n;
   resume = 0;                                 // the clock stands still while sleeping:
                                               // RESUME_MS would go on after waking
   disp_power(0);
   LED_RED = 1;
   LED_GREEN = 1;
   WDT_OFF();
   INTCONbits.GIE = 0;
   PIE0bits.TMR0IE = 0;
   IOCBNbits.IOCBN5 = 1;                       // wake on the button going low
   PIE0bits.IOCIE = 1;
   meas_off();
   for(;;) {
      delay_ms(100);
      IOCBFbits.IOCBF5 = 0;
      SLEEP();
      NOP();
      for(n = 0; n < 16 && BUTTON_DOWN; n++) __delay_ms(100);
      if(n == 16) break;
   }
   PIE0bits.IOCIE = 0;
   IOCBNbits.IOCBN5 = 0;
   IOCBFbits.IOCBF5 = 0;
   PIR0bits.TMR0IF = 0;
   PIE0bits.TMR0IE = 1;
   INTCONbits.GIE = 1;
   WDT_ON();
   meas_init();
   batt_start();
}

static void battery_check(void) {
   uint8_t lvl;
   meas_battery();
   lvl = batt_level(vbat_mv);
   if(lvl > batt_lvl) {
      if(!low_cnt || lvl < row_lvl) row_lvl = lvl;
      if(++low_cnt >= BATT_N) {                // the mildest level of the row: a
         low_cnt = 0;                          // single dip at its end does not
         batt_lvl = row_lvl;                   // switch off
      }
   }
   else {                                      // lvl <= batt_lvl < B_OFF: no underflow
      lvl = batt_level(vbat_mv - BATT_HYST);
      if(lvl <= batt_lvl) {                    // clearly above the next threshold: the
         low_cnt = 0;                          // row ends, and a level ends BATT_HYST
         batt_lvl = lvl;                       // above its threshold
      }
   }
   if(batt_lvl == B_OFF) go_off = 2;           // main loop switches off
   batt_blink = batt_lvl >= B_WARN && !batt_blink;
   disp_battery(batt_blink ? 0 : vbat_mv);
   if(batt_lvl != B_LOW) recharge_cnt = 0;     // shown at once when the level comes
   else if(recharge_cnt) recharge_cnt--;
   else if(!msg_on) {
      message("RECHARGE", 1500);
      msg_batt = 1;
      recharge_cnt = RECHARGE_N - 1;
   }
   // blink: green above 3.7 V, yellow (both) above 3.59 V, else red
   if(vbat_mv > 3700) LED_GREEN = 0;
   else if(vbat_mv > 3590) { LED_GREEN = 0; LED_RED = 0; }
   else LED_RED = 0;
   led_on = 1;
   t_led = tick_ms();
}

// Why the PIC was reset, shown for 2 s; nothing after a normal power-up
static void reset_reason(void) {
   const char *s = 0;
   if(!PCON0bits.nPOR) s = 0;
   else if(!PCON0bits.nBOR) s = "LOW BATT";
   else if(!PCON0bits.nRWDT) s = "WDT RST";
   else if(PCON0bits.STKOVF || PCON0bits.STKUNF) s = "STACK RST";
   PCON0 = 0x3F;                               // arm the flags for the next reset
   if(s) message_wait(s, 2000);
}

// ---------------------------------------------------------------- watching

static void watch(void) {
   meas_t m;
   uint16_t p10, swr, delta;
   uint32_t now = tick_ms();
   uint8_t pw;
   meas_take(&m, 4);
   if(m.overflow && (!msg_on || msg_batt)) message("OVERLOAD", 2000);   // before RECHARGE
   p10 = pwr_x10(m.pf);
   if(p10) wake();
   // peak hold for the power display (Cell 10)
   if(m.pf >= peak.pf || now - t_peak >= (uint32_t)cfg[CFG_PEAK] * 10) {
      peak = m;
      t_peak = now;
   }
   swr = swr_x100(m.g2);
   // power compared as in tune_run (not the rounded p10), else auto tune
   // starts tunes that never see a carrier
   pw = tune_power(&m);
   if(pw == P_OK || pw == P_HIGH) swr_last = swr;
   if(pw != P_NONE) swr_disp = swr;

   // auto tune: power for tuning, SWR above 1.20 and changed by more than
   // Cell 6 since the last tune (not again right after a tune, and not
   // again and again when the antenna cannot be matched better)
   delta = (uint16_t)(cfg[CFG_AUTO_DELTA] - 10) * 10;
   if(cfg[CFG_AUTO] && !st.bypass && pw == P_OK && m.stable && swr > 120 && since(t_tuned) >= AUTO_HOLD
      && (swr > swr_ref + delta || swr + delta < swr_ref)) {
      if(++auto_cnt >= AUTO_STEADY) auto_tune = 1;   // main loop starts it
   }
   else auto_cnt = 0;
}

static void show(void) {
   show_power(pwr_x10(peak.pf));
   show_swr(swr_disp);
}

// ---------------------------------------------------------------- main

void main(void) {
   uint8_t ev, bor_only;
   board_init();
   bor_only = PCON0bits.nPOR && !PCON0bits.nBOR;
   settings_load();
   meas_init();
   batt_start();
   mem_load();
   // The relays latch: they hold the setting saved before the reset. They
   // are pulsed again to be sure, except after a brown-out, where the
   // pulses could pull the weak battery down once more.
   if(state_load()) {
      rel = st.r;
      if(!bor_only) relays_set(st.r.l, st.r.c, st.r.sw);
   }
   else {
      st.bypass = 0;
      st.last_swr = 0;
      st.byp.l = st.byp.c = st.byp.sw = 0;
      relays_set(0, 0, 0);
   }
   swr_ref = st.last_swr ? (uint16_t)(100 + st.last_swr) : 0;
   EXT_KEY_OUT = 1;
   start_screen();
   reset_reason();
   WDT_ON();
   t_batt = t_watch = t_show = t_refresh = tick_ms();

   for(;;) {
      CLRWDT();

      ev = buttons_event();
      // a dark display: a button press only wakes it (the user could not
      // see what it would do); the transceiver's commands and power off act
      if((ev == EV_SHORT || ev == EV_LONG) && !disp_is_on()) {
         wake();
         ev = EV_NONE;
      }
      switch(ev) {
         case EV_SHORT:     wake(); bypass_toggle(!st.bypass); break;
         case EV_LONG:      do_tune(); break;
         case EV_XLONG:     go_off = 1; break;
         case EV_EXT_SHORT: wake(); bypass_toggle(1); break;
         case EV_EXT_LONG:  do_tune(); break;
      }

      if(since(t_watch) >= WATCH_MS) {
         t_watch = tick_ms();
         watch();
      }
      if(auto_tune) {
         auto_tune = 0;
         do_tune();
      }
      if(since(t_show) >= SHOW_MS) {
         t_show = tick_ms();
         show();
      }
      if(since(t_batt) >= BATT_MS) {
         t_batt = tick_ms();
         battery_check();
      }
      if(led_on && since(t_led) >= 30) {
         led_on = 0;
         LED_GREEN = 1;
         LED_RED = 1;
      }
      if(msg_on && (int32_t)(tick_ms() - t_msg) >= 0) message_end();
      if(since(t_refresh) >= REFRESH_MS) {
         t_refresh = tick_ms();
         disp_refresh();
      }
      if(disp_is_on() && cfg[CFG_DISP_OFF] && since(t_active) >= (uint32_t)cfg[CFG_DISP_OFF] * 60000)
         disp_power(0);
      if(cfg[CFG_POWER_OFF] && since(t_active) >= (uint32_t)cfg[CFG_POWER_OFF] * 60000)
         go_off = 3;
      if(go_off) {
         if(go_off < 3) {
            wake();
            // alone in the middle of the display
            disp_clear();
            if(go_off == 1) disp_big(LINE_MID, CENTRE_BIG("POWER OFF"), "POWER OFF");
            else disp_big(LINE_MID, CENTRE_BIG("LOW BATT"), "LOW BATT");
            disp_flush();
            delay_ms(1500);
         }
         go_off = 0;
         power_off();
         start_screen();
         t_batt = t_watch = t_show = t_refresh = tick_ms();
      }
      disp_service();
   }
}
