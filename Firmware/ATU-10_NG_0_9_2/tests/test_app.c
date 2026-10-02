// The main program on the PC: app.c with the real buttons, timer, display,
// settings, EEPROM and tuning code, against simulated hardware and time.
// A script presses the button and switches a carrier on and off over 90
// simulated minutes; the test checks what the tuner does.

#include "check.h"
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <xc.h>
#include "../src/board.h"
#include "../src/timer.h"
#include "../src/meas.h"
#include "../src/relays.h"
#include "../src/nvm.h"
#include "../src/tune.h"
#include "../src/cells.h"

// ---- registers (tests/host/xc.h)
struct LATAbits_t LATAbits; struct LATBbits_t LATBbits; struct LATCbits_t LATCbits;
struct LATDbits_t LATDbits; struct LATEbits_t LATEbits; struct PORTAbits_t PORTAbits;
struct PORTBbits_t PORTBbits; struct PORTDbits_t PORTDbits; struct INTCONbits_t INTCONbits;
struct IOCBFbits_t IOCBFbits; struct IOCBNbits_t IOCBNbits; struct PIE0bits_t PIE0bits;
struct PIR0bits_t PIR0bits; struct WDTCON0bits_t WDTCON0bits; struct PCON0bits_t PCON0bits;
uint8_t PCON0_reg;
volatile uint8_t Cells[16] = {
   0x05, 0x30, 0x07, 0x10, 0x15, 0x13, 0x01, 0x04, 0x14, 0x60, 0x05, 0x02 };

void isr(void);
void app_main(void);

// ---- simulated world
#define MIN 60000UL
static uint32_t wall;                // ms since power-up
static uint32_t end_at;
static jmp_buf done;
static int rf_on, sleeps, relay_calls, key_low_in_tune, in_tune;
static int mode;                     // 0: the main run, else a start-up / Cells variant
enum { M_MAIN, M_BOR, M_WDT, M_CELLS_MIN, M_CELLS_MAX, M_BLIP, M_UNMATCH, M_NOMATCH, M_EXTDARK, M_NOPOWER, M_LOWPWR, M_EXTTUNE };
static uint32_t last_clr, wdt_worst;
static int display_lit;              // the display shows something (switched on)
static int display_ok = 1, display_inited, oled_inits, oled_ok_writes, key_falls, key_prev = 1;
static uint32_t disp_us, io_frac;     // time the main program spends on display I/O
static double g2_floor;              // best possible reflection of the load

typedef struct { uint32_t from, to; } span_t;
static span_t press[40] = {         // button held (ms); the setup menu presses are added in main()
   {30 * MIN, 30 * MIN + 100},       // short, display missing: bypass on
   {31 * MIN, 31 * MIN + 100},       // short: bypass off
   {80 * MIN, 80 * MIN + 2000},      // wake from power off
   {82 * MIN, 82 * MIN + 100},       // short: bypass on
   {83 * MIN, 83 * MIN + 100},       // short: bypass off
   {84 * MIN, 84 * MIN + 500},       // long: tune
   {86 * MIN, 86 * MIN + 3000},      // extra long: power off
   {91 * MIN, 91 * MIN + 7000},      // wake and keep holding: setup menu
};
static int n_press = 8;
static const span_t carrier[] = {
   {5000, 45 * MIN},                 // 45 min of transmitting: auto tune, no power off
   {84 * MIN + 2000, 85 * MIN},      // the carrier for the tune at 84 min
   {97 * MIN + 1000, 98 * MIN},      // the carrier for the external tune
};
static const span_t ext_start[] = {  // start line of the external interface low
   {96 * MIN, 96 * MIN + 50},        // short: bypass on
   {97 * MIN, 97 * MIN + 400},       // long: tune
};

static int in_spans(const span_t *s, int n) {
   for(int i = 0; i < n; i++) if(wall >= s[i].from && wall < s[i].to) return 1;
   return 0;
}

static void checkpoint(uint32_t t);
static uint32_t check_line2_at;
const uint8_t *disp_fb(void);

// After a tune (and its message) the lower line shows only the label
// (x 0..35), "=" (x 42..53) and the value (x 60..107): the gaps must be
// empty - a left-over "E" of TUNE was seen on the device at x 36..41
static int line2_checks;
// only one big text, in the middle of an otherwise empty display
static int centred_checks;
static void check_centred(void) {
   const uint8_t *fb = disp_fb();
   int outside = 0, inside = 0, left = 128, right = -1;
   for(int y = 0; y < 32; y++)
      for(int x = 0; x < 128; x++) {
         if(!(fb[(y / 8) * 128 + x] >> (y % 8) & 1)) continue;
         if(y < 9 || y > 22) outside++;
         else inside++;
         if(x < left) left = x;
         if(x > right) right = x;
      }
   CHECK_EQ(outside, 0);
   CHECK(inside > 100);
   CHECK(abs(left - (127 - right)) <= 1);              // as far from both edges
}

static void check_line2(void) {
   const uint8_t *fb = disp_fb();
   int dirty = 0;
   if(!OLED_PWR) return;             // switched off meanwhile: nothing shown
   for(int y = 18; y < 32; y++)
      for(int x = 0; x < 115; x++) {
         int gap = (x >= 36 && x <= 41) || (x >= 54 && x <= 59) || (x >= 108);
         if(gap && (fb[(y / 8) * 128 + x] >> (y % 8) & 1)) dirty++;
      }
   CHECK_EQ(dirty, 0);
   line2_checks++;
}

void fake_clrwdt(void) {
   last_clr = wall;
   fake_ms(1);
}

void fake_ms(uint32_t ms) {
   while(ms--) {
      wall++;
      if(WDTCON0bits.SEN && wall - last_clr > wdt_worst) wdt_worst = wall - last_clr;
      if(!LATDbits.LATD2 && key_prev) key_falls++;                     // a tune started
      if(LATDbits.LATD2 && !key_prev) check_line2_at = wall + 2500;     // ended: check the picture
      key_prev = LATDbits.LATD2;
      if(wall == check_line2_at) check_line2();
      PORTBbits.RB5 = !in_spans(press, n_press);                        // low = pressed
      if(mode == M_EXTDARK) PORTDbits.RD1 = !(wall >= 3 * MIN && wall < 3 * MIN + 50);   // short pulse
      else if(mode == M_EXTTUNE) PORTDbits.RD1 = !(wall >= 11000 && wall < 11050);   // during a tune
      else PORTDbits.RD1 = mode ? 1 : !in_spans(ext_start, 2);          // low = start
      if(!mode) {                                                        // display unplugged
         display_ok = !(wall >= 20 * MIN && wall < 40 * MIN);
         if(!display_ok) display_inited = display_lit = 0;   // dark until initialized and on
      }
      PORTDbits.RD2 = LATDbits.LATD2;                                   // key line as driven
      if(mode == M_EXTDARK) rf_on = 0;
      else if(mode == M_NOPOWER) rf_on = wall >= 60000 && wall < 2 * MIN;   // after the NO POWER
      else rf_on = mode ? wall >= 10000 && wall < 5 * MIN : in_spans(carrier, sizeof carrier / sizeof *carrier);
      if(wall == 100 * MIN && !mode) vbat_mv = 3300;                    // battery empty
      if(INTCONbits.GIE && PIE0bits.TMR0IE) {
         PIR0bits.TMR0IF = 1;
         isr();
      }
      checkpoint(wall);
      if(wall >= end_at) longjmp(done, 1);
   }
}

void fake_sleep(void) {
   sleeps++;
   // the last picture before sleeping (the framebuffer keeps it): POWER OFF
   // (button held at 86 min) and LOW BATT (100 min) alone in the middle
   if(!mode && ((wall >= 86 * MIN && wall < 87 * MIN) || (wall >= 100 * MIN && wall < 101 * MIN))) {
      check_centred();
      centred_checks++;
   }
   while(PORTBbits.RB5) fake_ms(10);  // until the button goes down (wake-up interrupt)
}

// ---- hardware the application calls
relays_t rel;
uint16_t vbat_mv = 4000;
void board_init(void) { INTCONbits.GIE = 1; PIE0bits.TMR0IE = 1; }
void delay_ms(uint16_t ms) { while(ms--) fake_clrwdt(); }
void meas_init(void) {}
uint16_t meas_battery(void) { return vbat_mv; }
void relays_set(uint8_t l, uint8_t c, uint8_t sw) {
   rel.l = l; rel.c = c; rel.sw = sw;
   relay_calls++;
   if(!LATDbits.LATD2) key_low_in_tune = 1;
   delay_ms(26);
}
// a load whose best match is L 20, C 30, capacitor at the output
void meas_take(meas_t *m, uint8_t n) {
   double d2 = (rel.l - 20.0) * (rel.l - 20.0) + (rel.c - 30.0) * (rel.c - 30.0) + (rel.sw ? 400 : 0);
   double g;
   memset(m, 0, sizeof *m);
   last_clr = wall;                  // meas_take clears the watchdog
   m->pf = rf_on ? (mode == M_CELLS_MAX ? 12000000 : mode == M_LOWPWR ? 970000 : 5000000) : 0;
   g = d2 / (d2 + 60);
   m->g2 = (uint32_t)(G2_ONE * (g2_floor + (1 - g2_floor) * g));
   m->pr = (uint32_t)((double)m->pf * m->g2 / G2_ONE);
   m->stable = 1;
   m->spread = 2;
   fake_ms(n > 8 ? 4 : 1);
}
void hal_relay_set(uint8_t l, uint8_t c, uint8_t sw) { in_tune = 1; relays_set(l, c, sw); }
void hal_sample(meas_t *m, uint8_t n) { meas_take(m, n); }
void hal_wait_ms(uint8_t ms) { fake_ms(ms); }
// display hardware
// display I/O takes as long as on the tuner (I2C at about 45 kHz)
static void io_cost(uint32_t us) {
   disp_us += us;
   io_frac += us;
   if(io_frac >= 1000) { fake_ms(io_frac / 1000); io_frac %= 1000; }
}
uint8_t oled_present(void) { io_cost(200); return display_ok; }
uint8_t oled_on(void) { io_cost(500); if(display_ok && display_inited) display_lit = 1; return !display_ok; }
uint8_t oled_init(void) {
   oled_inits++;
   if(!display_ok) {                 // 10 tries with 100 ms in between
      for(int i = 0; i < 10; i++) { io_cost(6000); disp_us += 100000; delay_ms(100); }
      return 1;
   }
   io_cost(6000 + 64 * 24 * 180);    // init sequence, 8 pages cleared
   display_inited = 1;
   return 0;
}
uint8_t oled_write(uint8_t page, uint8_t x, const uint8_t *d, uint8_t n) {
   (void)page; (void)x; (void)d;
   io_cost((n + 8) * 180u);
   if(display_ok && display_inited) oled_ok_writes++;
   return !display_ok;
}
void i2c_init(void) {}
// data EEPROM, erased
static uint8_t ee[256];
uint8_t nvm_read(uint8_t a) { return ee[a]; }
static int ee_writes;                // bytes changed (nvm_write skips equal ones)
void nvm_write(uint8_t a, uint8_t v) { ee_writes += ee[a] != v; ee[a] = v; }

// ---- what the tuner should have done, checked at these points in time
static int tunes_seen(void) {        // a tune switched the relays since the last call
   int t = in_tune;
   in_tune = 0;
   return t;
}

static void checkpoint_variant(uint32_t t);

static void checkpoint(uint32_t t) {
   if(mode) { checkpoint_variant(t); return; }
   if(getenv("TRACE") && t >= 91 * MIN && t <= 91 * MIN + 40000 && t % 500 == 0)
      printf("%6.1f s  btn %d held %3d  oled %d  sleeps %d  relay_ms %d  ticks %u\n", (t - 91 * MIN) / 1000.0,
             !PORTBbits.RB5, btn_held, OLED_PWR, sleeps, cfg[CFG_RELAY_MS], tick_ms());
   switch(t) {
   case 5000:                        // after the greeting (4.2 s)
      CHECK(OLED_PWR);
      CHECK_EQ(sleeps, 0);
      CHECK(rel.l == 0 && rel.c == 0);
      CHECK(WDTCON0bits.SEN);
      tunes_seen();
      break;
   case 30000:                       // carrier with SWR > 1.2 since 5 s: auto tune done
      CHECK(tunes_seen());
      CHECK(key_low_in_tune);
      CHECK(EXT_KEY_OUT);            // released after the tune
      CHECK(rel.l == 20 && rel.c == 30 && rel.sw == 0);
      CHECK(st.last_swr > 0);         // the tune result is known (also at SWR 1.00)
      break;
   case 20 * MIN:                    // the display is unplugged now
      oled_inits = 0;
      disp_us = 0;
      break;
   case 30 * MIN + 2000:             // the button still works without a display
      CHECK(st.bypass);
      break;
   case 31 * MIN + 2000:
      CHECK(!st.bypass);
      CHECK(rel.l == 20 && rel.c == 30);
      break;
   case 40 * MIN:                    // few restarts of the missing display (back-off),
      {                              // little time lost to it
         double busy = (disp_us / 1000.0 + oled_inits * 500.0) / (20.0 * MIN);
         printf("20 min without a display: %d restarts, %.1f %% of the time on display I/O\n",
                oled_inits, busy * 100);
         CHECK(oled_inits <= 30);
         CHECK(busy < 0.05);
      }
      oled_ok_writes = 0;
      oled_inits = 0;
      break;
   case 40 * MIN + 3000:             // plugged in again: initialized and drawn within 3 s
      CHECK(oled_inits >= 1);
      CHECK(display_inited && display_lit);
      CHECK(oled_ok_writes > 0);
      break;
   case 44 * MIN:                    // still transmitting: no power off, display on
      CHECK_EQ(sleeps, 0);
      CHECK(OLED_PWR);
      CHECK(!tunes_seen());          // no second auto tune at a good SWR
      break;
   case 49 * MIN:                    // 4 min after the carrier: display still on
      CHECK(OLED_PWR);
      break;
   case 51 * MIN:                    // 6 min: display off
      CHECK(!OLED_PWR);
      CHECK_EQ(sleeps, 0);
      break;
   case 74 * MIN:                    // 29 min without activity: not yet off
      CHECK_EQ(sleeps, 0);
      break;
   case 76 * MIN:                    // 31 min: switched off
      CHECK_EQ(sleeps, 1);
      CHECK(!OLED_PWR);
      break;
   case 81 * MIN:                    // woken by the button: display on, relays kept
      CHECK(OLED_PWR);
      CHECK(rel.l == 20 && rel.c == 30);
      CHECK(!st.bypass);
      break;
   case 82 * MIN + 2000:             // short press: bypass
      CHECK(st.bypass);
      CHECK(rel.l == 0 && rel.c == 0 && rel.sw == 0);
      break;
   case 83 * MIN + 2000:             // short press: back to the tuned setting
      CHECK(!st.bypass);
      CHECK(rel.l == 20 && rel.c == 30);
      tunes_seen();
      break;
   case 85 * MIN + 1000:             // long press, carrier 2 s later: tuned
      CHECK(tunes_seen());
      CHECK(rel.l == 20 && rel.c == 30 && rel.sw == 0);
      CHECK(EXT_KEY_OUT);
      CHECK_EQ(sleeps, 1);
      break;
   case 89 * MIN:                    // extra long press: off, setting kept
      CHECK_EQ(sleeps, 2);
      CHECK(rel.l == 20 && rel.c == 30 && rel.sw == 0);
      CHECK(!st.bypass);
      break;
   case 95 * MIN:                    // setup menu: relay pulse 7 -> 8 ms, saved
      CHECK_EQ(cfg[CFG_RELAY_MS], 8);
      CHECK_EQ(ee[0x20], 0x5E);
      CHECK_EQ(ee[0x20 + 3 + CFG_RELAY_MS], 8);
      CHECK(OLED_PWR);
      tunes_seen();
      break;
   case 96 * MIN + 2000:             // external short: bypass on
      CHECK(st.bypass);
      CHECK(rel.l == 0 && rel.c == 0);
      break;
   case 98 * MIN:                    // external long: tuned, bypass ended
      CHECK(tunes_seen());
      CHECK(!st.bypass);
      CHECK(rel.l == 20 && rel.c == 30);
      CHECK(EXT_KEY_OUT);
      CHECK_EQ(sleeps, 2);
      break;
   case 104 * MIN:                   // battery empty: switched off
      CHECK_EQ(sleeps, 3);
      break;
   }
}

// ---- start-up and Cells variants: 6 simulated minutes each
static void checkpoint_variant(uint32_t t) {
   if(getenv("TRACE") && t % 1000 == 0 && t <= 46000)
      printf("%5u ms  last_swr %d  rel %d/%d/%d  key %d  falls %d  held %d\n", t, st.last_swr,
             rel.l, rel.c, rel.sw, LATDbits.LATD2, key_falls, btn_held);
   if(t == 5000) {                   // after the start
      CHECK(OLED_PWR);
      if(mode < M_BLIP) CHECK(rel.l == 20 && rel.c == 30 && rel.sw == 0);   // restored from the EEPROM
      if(mode == M_BOR) CHECK_EQ(relay_calls, 0);           // no pulses after a brown-out
      if(mode == M_WDT) CHECK_EQ(relay_calls, 1);           // pulsed once to be sure
   }
   if(mode == M_EXTDARK && t == 3 * MIN - 1000)             // display off after 1 min (Cell 1)
      CHECK(!OLED_PWR);
   if(mode == M_EXTDARK && t == 3 * MIN + 2000) {           // the transceiver's short pulse
      CHECK(st.bypass);                                      // switched the bypass on although
      CHECK(rel.l == 0 && rel.c == 0);                       // the display was dark
   }
   static int ee_before;
   if(mode == M_NOPOWER && t == 20000) ee_before = ee_writes;
   if(mode == M_NOPOWER && t == 45000) {                    // long press without a carrier: NO
      CHECK_EQ(key_falls, 1);                                // POWER, everything as before
      CHECK_EQ(ee_writes - ee_before, 0);                    // and nothing written
      CHECK(rel.l == 20 && rel.c == 30);
      CHECK_EQ(st.last_swr, 5);
   }
   if(mode == M_EXTTUNE && t == 10999)                      // the auto tune is running when
      CHECK(!LATDbits.LATD2);                                // the transceiver asks for bypass
   if(mode == M_EXTTUNE && t == 20000) {                    // the tune stopped, bypass is on
      CHECK(st.bypass);
      CHECK(rel.l == 0 && rel.c == 0 && rel.sw == 0);
      CHECK(LATDbits.LATD2);
   }
   if(t == 40000 && mode == M_BLIP)                         // carrier since 10 s: tuned, so
      CHECK(key_falls >= 1);                                 // not stuck in the setup menu
   if(t == 6 * MIN - 1) {
      CHECK_EQ(sleeps, 0);
      switch(mode) {
      case M_CELLS_MIN:                                      // auto tune off: no tune
         CHECK_EQ(key_falls, 0);
         CHECK(OLED_PWR);                                    // display off never
         break;
      case M_CELLS_MAX:                                      // Cell 6 = 99 (threshold 8.9):
         printf("Cells max: %d tunes in 5 min\n", key_falls);
         CHECK_EQ(key_falls, 0);                             // auto tune practically off
         break;
      case M_BLIP:                                           // short press at the end of the
         CHECK(key_falls >= 1);                              // greeting: no setup menu, auto
         break;                                              // tune works
      case M_NOPOWER:                                        // carrier at the tuned setting
         CHECK_EQ(key_falls, 1);                             // later: no needless auto tune
         break;
      case M_EXTDARK:
         break;
      case M_EXTTUNE:
         break;
      case M_LOWPWR:                                         // 0.97 W, Cell 4 = 1.0 W: the
         CHECK_EQ(key_falls, 0);                             // tune would not see a carrier,
         break;                                              // so no auto tune either
      case M_UNMATCH:                                        // best possible SWR 2.0, default
      case M_NOMATCH:                                        // threshold / nothing matches:
         printf("%s: %d tunes in 5 min\n", mode == M_UNMATCH ? "SWR 2 load" : "no match",
                key_falls);
         CHECK_EQ(key_falls, 1);                             // tuned once, then left alone
         break;
      }
   }
}

static int run_variant(void) {
   memset(ee, 0xFF, sizeof ee);
   st.r.l = 20; st.r.c = 30; st.r.sw = 0;                    // a saved tune result
   st.last_swr = 5;
   if(mode == M_UNMATCH) g2_floor = 1.0 / 9;                 // best possible SWR 2.0
   if(mode == M_NOMATCH) g2_floor = 0.99995;                 // SWR 9.99 everywhere
   if(mode == M_BLIP || mode == M_UNMATCH || mode == M_NOMATCH || mode == M_LOWPWR || mode == M_EXTTUNE) {   // start in bypass, so that
      st.r.l = st.r.c = 0;                                   // the carrier causes an auto tune
      st.last_swr = 0;
   }
   state_save();
   PORTBbits.RB5 = 1;
   LATDbits.LATD2 = 1;
   PCON0bits.nPOR = mode != M_BOR && mode != M_WDT ? 0 : 1;
   PCON0bits.nBOR = mode == M_BOR ? 0 : 1;
   PCON0bits.nRWDT = mode == M_WDT ? 0 : 1;
   if(mode == M_CELLS_MIN) {
      static const uint8_t c[12] = {0x00, 0x00, 0x02, 0x01, 0x01, 0x11, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01};
      memcpy((void *)Cells, c, 12);
   }
   if(mode == M_CELLS_MAX) {
      static const uint8_t c[12] = {0x99, 0x99, 0x30, 0x99, 0x99, 0x99, 0x01, 0x99, 0x99, 0x99, 0x99, 0x03};
      memcpy((void *)Cells, c, 12);
      g2_floor = 1.0 / 9;                                    // best possible SWR 2.0
   }
   if(mode == M_EXTDARK) {
      static const uint8_t c[12] = {0x01, 0x30, 0x07, 0x10, 0x15, 0x13, 0x01, 0x04, 0x14, 0x60, 0x05, 0x02};
      memcpy((void *)Cells, c, 12);                          // display off after 1 min
   }
   n_press = 0;
   if(mode == M_BLIP) {                                       // ends 0.4 s after the greeting
      press[0].from = 3800; press[0].to = 4600;
      n_press = 1;
   }
   if(mode == M_NOPOWER) {                                    // long press, no carrier
      press[0].from = 20000; press[0].to = 20600;
      n_press = 1;
   }
   relay_calls = 0;
   end_at = 6 * MIN;
   if(!setjmp(done)) app_main();
   printf("watchdog: longest time without clearing %u ms\n", wdt_worst);
   CHECK(wdt_worst < 7000);
   return check_done(mode == M_BOR ? "test_app bor" : mode == M_WDT ? "test_app wdt" :
                     mode == M_CELLS_MIN ? "test_app cells-min" : mode == M_CELLS_MAX ? "test_app cells-max"
                     : mode == M_BLIP ? "test_app blip" : mode == M_UNMATCH ? "test_app swr2"
                     : mode == M_NOMATCH ? "test_app nomatch" : mode == M_EXTDARK ? "test_app ext-dark"
                     : mode == M_NOPOWER ? "test_app nopower" : mode == M_LOWPWR ? "test_app lowpwr"
                     : "test_app ext-tune");
}

int main(int argc, char **argv) {
   if(argc > 1) {
      mode = atoi(argv[1]);
      return run_variant();
   }
   memset(ee, 0xFF, sizeof ee);
   PORTBbits.RB5 = 1;
   PCON0bits.nPOR = 0;               // a normal power-up
   PCON0bits.nBOR = 1;
   LATDbits.LATD2 = 1;
   // setup menu, after the greeting (released at 91:07): 2 long presses to
   // setting 3, a short one (7 -> 8 ms), 10 long ones to the SAVE page, a
   // short one
   {
      uint32_t t = 91 * MIN + 9000;
      for(int i = 0; i < 14; i++, t += 1500) {
         uint32_t len = (i == 2 || i == 13) ? 100 : 500;
         press[n_press].from = t;
         press[n_press++].to = t + len;
      }
   }
   end_at = 105 * MIN;
   if(!setjmp(done)) app_main();     // runs until fake_ms jumps back at end_at
   // the EEPROM holds the tuned setting and the memory
   {
      uint8_t n = 0;
      CHECK(state_load());
      CHECK(st.r.l == 20 && st.r.c == 30);
      mem_load();
      for(int i = 0; i < tune_mem_n; i++) n += tune_mem[i].l == 20 && tune_mem[i].c == 30;
      CHECK_EQ(n, 1);
   }
   printf("relay steps %d over the run, lower line checked after %d tunes\n", relay_calls, line2_checks);
   CHECK(line2_checks >= 3);         // not the tune that the power off stops (display off)
   CHECK_EQ(centred_checks, 2);
   printf("watchdog: longest time without clearing %u ms\n", wdt_worst);
   CHECK(wdt_worst < 7000);
   return check_done("test_app");
}
