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
#include "version.h"

#define BATT_MS     3000       // battery check and LED blink
#define WATCH_MS    50         // measurement for the display / auto tune
#define SHOW_MS     150        // display update of power and SWR
#define REFRESH_MS  30000      // send the whole picture again
#define LOW_BATT_MV 3400
#define AUTO_STEADY 4          // auto tune after this many measurements in a row
#define AUTO_HOLD   3000       // ms after a tune without auto tune
#define SETUP_HOLD  100        // x 10 ms held at the end of the greeting: setup menu

static uint32_t t_batt, t_watch, t_show, t_refresh, t_active, t_led, t_msg, t_tuned;
static uint8_t led_on, msg_on, auto_cnt, auto_tune, go_off;
static uint16_t swr_ref;       // SWR of the last tune, reference for auto tune
static uint16_t shown_pwr = 0xFFFF, shown_swr = 0xFFFF;
static meas_t peak;            // peak hold for the display
static uint32_t t_peak;
static uint16_t swr_last;      // last SWR measured with enough power

// ms since the tick value t. Always read the clock fresh: a timestamp set
// in between (wake, tune) may be later than a value read before, and the
// unsigned difference would then be huge.
static uint32_t since(uint32_t t) {
   return tick_ms() - t;
}

// ---------------------------------------------------------------- text

static char buf[8];

// v / 10^dec as text with dec decimals, at least 'width' characters
static const char *num(uint16_t v, uint8_t dec, uint8_t width) {
   char t[6];
   uint8_t n = 0, i = 0;
   do {
      t[n++] = (char)('0' + v % 10);
      v /= 10;
      if(n == dec) t[n++] = '.';
   } while(v || (dec && n <= dec + 1));
   while(n < width && i < sizeof buf - 1) { buf[i++] = ' '; width--; }
   while(n) buf[i++] = t[--n];
   buf[i] = 0;
   return buf;
}

// ---------------------------------------------------------------- screen

static void show_swr_label(void) {
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
   disp_big(LINE2, 60, num(swr, 2, 4));
}

// a message in place of the SWR line for 'ms'
static void message(const char *s, uint16_t ms) {
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

static void message_wait(const char *s, uint16_t ms) {   // blocking, for start-up / power off
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
   if(ev == EV_XLONG) {                        // power off after the stop
      buttons_unget(ev);
      return 1;
   }
   return ev == EV_SHORT || ev == EV_LONG;
}

static void do_tune(void) {
   uint8_t r;
   wake();
   LED_GREEN = 0;
   EXT_KEY_OUT = 0;                            // tells the transceiver: tuning
   msg_on = 0;
   disp_big(LINE2, 0, "TUNE");                 // the SWR so far follows at the right
   disp_flush();
   // in bypass the relays hold no tune result (the memory has it)
   r = tune_run(&rel, !st.bypass && st.last_swr ? (uint16_t)(100 + st.last_swr) : 0);
   // A result ends the bypass. A stopped tune (e.g. the long press that
   // goes on to power off) leaves the relays as they were if no carrier
   // was there; then the bypass stays.
   if(r == TUNE_OK || r == TUNE_NO_MATCH || rel.l || rel.c || rel.sw) st.bypass = 0;
   // last_swr: SWR - 1.00 in hundredths, 1..255 (0 = no tune result, so
   // a perfect 1.00 is kept as 1.01)
   if(!st.bypass)
      st.last_swr = (r == TUNE_OK && (rel.l || rel.c))
                    ? (uint8_t)(tune_swr - 100 > 255 ? 255 : tune_swr > 101 ? tune_swr - 100 : 1) : 0;
   save_state();
   swr_ref = tune_swr;
   swr_last = tune_swr;
   show_swr_label();
   shown_swr = 0xFFFF;
   show_swr(tune_swr);
   if(r == TUNE_NO_CARRIER) message("NO POWER", 2000);
   else if(r == TUNE_NO_MATCH) message("NO MATCH", 2000);
   else if(r == TUNE_ABORTED) message("STOP", 1000);
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

static void greeting(void) {
   disp_clear();
   disp_big(LINE1, 28, "ATU-10");
   disp_small(2, 31, "FW NG " FW_VERSION);
   disp_small(3, 13, "DESIGNED BY DL8UG");
   disp_flush();
   LED_GREEN = 0;
   delay_ms(3000);
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

// Sleeps until the button is held for 1.6 s. The relays keep their setting
// without power; the watchdog is off while sleeping. The caller starts the
// display again (start_screen) - one hardware stack level less.
static void power_off(void) {
   uint8_t n;
   disp_power(0);
   LED_RED = 1;
   LED_GREEN = 1;
   WDT_OFF();
   INTCONbits.GIE = 0;
   PIE0bits.TMR0IE = 0;
   IOCBNbits.IOCBN5 = 1;                       // wake on the button going low
   PIE0bits.IOCIE = 1;
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
   meas_battery();
}

static void battery_check(void) {
   meas_battery();
   disp_battery(vbat_mv);
   // blink: green above 3.7 V, yellow (both) above 3.59 V, else red
   if(vbat_mv > 3700) LED_GREEN = 0;
   else if(vbat_mv > 3590) { LED_GREEN = 0; LED_RED = 0; }
   else LED_RED = 0;
   led_on = 1;
   t_led = tick_ms();
   if(vbat_mv < LOW_BATT_MV) go_off = 2;         // main loop switches off
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
   meas_take(&m, 4);
   if(m.overflow && !msg_on) message("OVERLOAD", 2000);
   p10 = pwr_x10(m.pf);
   if(p10) wake();
   // peak hold for the power display (Cell 10)
   if(m.pf >= peak.pf || now - t_peak >= (uint32_t)cfg[CFG_PEAK] * 10) {
      peak = m;
      t_peak = now;
   }
   swr = swr_x100(m.g2);
   if(p10 >= cfg[CFG_MIN_PWR]) swr_last = swr;

   // auto tune: enough power, SWR above 1.20 and changed by more than
   // Cell 6 since the last tune (not again right after a tune, and not
   // again and again when the antenna cannot be matched better)
   delta = (uint16_t)(cfg[CFG_AUTO_DELTA] - 10) * 10;
   if(cfg[CFG_AUTO] && !st.bypass && p10 >= cfg[CFG_MIN_PWR] && pnet_uw(&m) <= (uint32_t)cfg[CFG_MAX_PWR] * 1000000
      && m.stable && swr > 120 && since(t_tuned) >= AUTO_HOLD
      && (swr > swr_ref + delta || swr + delta < swr_ref)) {
      if(++auto_cnt >= AUTO_STEADY) auto_tune = 1;   // main loop starts it
   }
   else auto_cnt = 0;
}

static void show(void) {
   show_power(pwr_x10(peak.pf));
   show_swr(swr_last);
}

// ---------------------------------------------------------------- main

void main(void) {
   uint8_t ev, bor_only;
   board_init();
   bor_only = PCON0bits.nPOR && !PCON0bits.nBOR;
   settings_load();
   meas_init();
   meas_battery();
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
      if(ev != EV_NONE && !disp_is_on() && ev != EV_EXT_LONG && ev != EV_XLONG) {
         wake();                               // a dark display: the press only wakes it
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
            message_wait(go_off == 1 ? "POWER OFF" : "LOW BATT", 1500);
         }
         go_off = 0;
         power_off();
         start_screen();
         t_batt = t_watch = t_show = t_refresh = tick_ms();
      }
      disp_service();
   }
}
