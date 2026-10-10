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
#include "../src/display.h"

// ---- registers (tests/host/xc.h)
struct LATAbits_t LATAbits; struct LATBbits_t LATBbits; struct LATCbits_t LATCbits;
struct LATDbits_t LATDbits; struct LATEbits_t LATEbits; struct PORTAbits_t PORTAbits;
struct PORTBbits_t PORTBbits; struct PORTDbits_t PORTDbits; struct INTCONbits_t INTCONbits;
struct IOCBFbits_t IOCBFbits; struct IOCBNbits_t IOCBNbits; struct PIE0bits_t PIE0bits;
struct PIR0bits_t PIR0bits; struct WDTCON0bits_t WDTCON0bits; struct PCON0bits_t PCON0bits;
uint8_t PCON0_reg, ANSELA, ANSELD;
volatile uint8_t Cells[16] = {
   0x05, 0x30, 0x10, 0x10, 0x15, 0x13, 0x01, 0x04, 0x14, 0x60, 0x05, 0x02 };

void isr(void);
void app_main(void);

// ---- simulated world
#define MIN 60000UL
static uint32_t wall;                // ms since power-up
static uint32_t end_at;
static jmp_buf done;
static int rf_on, sleeps, relay_calls, key_low_in_tune, in_tune;
static int mode;                     // 0: the main run, else a start-up / Cells variant
enum { M_MAIN, M_BOR, M_WDT, M_CELLS_MIN, M_CELLS_MAX, M_BLIP, M_UNMATCH, M_NOMATCH, M_EXTDARK, M_NOPOWER, M_LOWPWR, M_EXTTUNE, M_STOPAUTO, M_OVERLOAD, M_PULSE, M_PULSE_SLOW,
       M_RESUME_OFF, M_RESUME_QSY, M_RESUME_EMPTY, M_BATT_DIP, M_HINT, M_BATT_OFF, M_HINT_BLIP, M_HINT_END, M_HINT_CW, M_BATT_HOVER,
       M_BATT_LOW, M_BATT_MIX, M_BATT_OVL, M_BATT_AUTO, M_BATT_EMPTY, M_RELAYS };
#define M_RESUME(m) ((m) >= M_RESUME_OFF && (m) <= M_RESUME_EMPTY)
static uint32_t last_clr, wdt_worst;
static int display_lit;              // the display shows something (switched on)
static int display_ok = 1, display_inited, oled_inits, oled_ok_writes, key_falls, key_prev = 1;
static uint32_t disp_us, io_frac;     // time the main program spends on display I/O
static double g2_floor;              // best possible reflection of the load
static int adc_on = 1;               // ADC and reference (meas_init / meas_off)
static double opt_l = 20, opt_c = 30;   // best match of the load (capacitor at the output)
static uint8_t seen1[2][128][128];   // M_RESUME_OFF: settings switched by the 1st / 2nd tune
static int n_first, n_again;
static int batt_hidden, recharge_seen, recharge_late, ovl_seen;
static uint8_t switched[2][128][128];   // M_RELAYS: settings switched by the tune running
static int live_updates, relays_checks, drawn_prev = -1;

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
   if(mode == M_OVERLOAD || mode == M_BATT_OVL) return;   // OVERLOAD stays while the carrier is too strong
   for(int y = 16; y < 32; y++)                       // rows 16, 17: between the lines
      for(int x = 0; x < (mode == M_RELAYS ? 113 : 115); x++) {   // up to the battery
         int gap = y < 18 || (x >= 36 && x <= 41) || (x >= 54 && x <= 59) || (x >= 108);
         if(gap && (fb[(y / 8) * 128 + x] >> (y % 8) & 1)) dirty++;
      }
   CHECK_EQ(dirty, 0);
   line2_checks++;
}

// M_RELAYS: the relay setting the cells show (centre pixels), -1 if a
// cell frame is missing; sw from the text TX / ANT
static int pix(int x, int y) { return disp_fb()[(y / 8) * 128 + x] >> (y % 8) & 1; }
static int text_shows(int big, uint8_t y, uint8_t x, const char *s);
static int drawn_setting(void) {
   int l = 0, c = 0, sw;
   for(int i = 0; i < 7; i++) {
      int x = 7 + 7 * i;
      if(!pix(x, 1) || !pix(x + 4, 5) || !pix(x, 9) || !pix(x + 4, 13)) return -1;
      l |= pix(x + 2, 3) << i;
      c |= pix(x + 2, 11) << i;
   }
   if(text_shows(0, 8, 100, "  TX")) sw = 1;
   else if(text_shows(0, 8, 100, " ANT")) sw = 0;
   else return -1;
   return sw << 14 | c << 7 | l;
}
static int rel_key(void) { return rel.sw << 14 | rel.c << 7 | rel.l; }

// relay view: after a tune the cells show the relays; while tuning they
// change only to settings the tune has switched (the best so far)
static void check_relays(void) {
   int d;
   if(!OLED_PWR || !display_lit) return;
   d = drawn_setting();
   CHECK(d >= 0);
   if(d < 0) return;
   if(!LATDbits.LATD2) {
      if(d != drawn_prev) {
         if(getenv("TRACE")) printf("%u ms: cells %d/%d/%d\n", wall, d & 127, (d >> 7) & 127, d >> 14);
         live_updates++;
         CHECK(switched[d >> 14][d & 127][(d >> 7) & 127]);
      }
   }
   else if(wall == check_line2_at) {
      CHECK_EQ(d, rel_key());
      relays_checks++;
   }
   drawn_prev = d;
   // the small battery instead of the symbol; columns 126, 127 dark
   for(int y = 0; y < 32; y++) CHECK(!pix(126, y) && !pix(127, y));
   for(int y = 0; y < 16; y++) CHECK(!pix(123, y) && !pix(124, y) && !pix(125, y));
}

void fake_clrwdt(void) {
   last_clr = wall;
   fake_ms(1);
}

void fake_ms(uint32_t ms) {
   while(ms--) {
      wall++;
      if(WDTCON0bits.SEN && wall - last_clr > wdt_worst) wdt_worst = wall - last_clr;
      if(!LATDbits.LATD2 && key_prev) {                                 // a tune started
         key_falls++;
         if(mode == M_RELAYS) memset(switched, 0, sizeof switched);
      }
      if(LATDbits.LATD2 && !key_prev) check_line2_at = wall + 2500;     // ended: check the picture
      key_prev = LATDbits.LATD2;
      if(OLED_PWR && ((ANSELA & 0x0C) || (ANSELD & 0x06))) {        // awake: inputs read
         CHECK_EQ(ANSELA & 0x0C, 0);
         CHECK_EQ(ANSELD & 0x06, 0);
         ANSELA = ANSELD = 0;                                       // report once
      }
      if(mode == M_RELAYS && wall >= 5000) check_relays();
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
      if(mode == M_EXTDARK || mode == M_BATT_OFF || mode == M_HINT_BLIP || mode == M_HINT_END
         || mode == M_HINT_CW || mode == M_BATT_AUTO || mode == M_BATT_EMPTY) rf_on = 0;
      else if(mode == M_NOPOWER) rf_on = wall >= 60000 && wall < 2 * MIN;   // after the NO POWER
      else if(mode == M_PULSE || mode == M_PULSE_SLOW)                  // CW key: 1 s down, 7 s
         rf_on = wall >= 10000 && (wall - 10000) % (mode == M_PULSE ? 8000 : 71000) < 1000;   // (70 s) up
      else if(M_RESUME(mode)) {                                   // one 1 s carrier, then
         rf_on = wall >= 10000 && wall < 11000;                         // (OFF) a tune after power
         if(mode == M_RESUME_OFF) rf_on |= wall >= 52000;               // off, (QSY) a carrier on
         if(mode == M_RESUME_QSY) rf_on |= wall >= 20000;               // another band, (EMPTY)
         if(mode == M_RESUME_EMPTY)                                     // 0.25 s carriers every 15 s
            rf_on |= wall >= 25000 && (wall - 25000) % 15000 < 250;
      }
      else if(mode == M_BATT_OVL) rf_on = wall >= 26000 && wall < 26300;   // a short overload
      else rf_on = mode ? wall >= 10000 && wall < 5 * MIN : in_spans(carrier, sizeof carrier / sizeof *carrier);
      if(wall == 100 * MIN && !mode) vbat_mv = 2900;                    // battery empty
      if(mode == M_RESUME_OFF) vbat_mv = wall >= 15000 && wall < 25000 ? 2900 : 4000;   // LOW BATT
      if(mode == M_RESUME_QSY && wall == 15000) { opt_l = 3; opt_c = 90; }            // other band
      if(mode == M_BATT_DIP)       // low for one reading (every 3 s), for two, then for good
         vbat_mv = (wall >= 20000 && wall < 22000) || (wall >= 40000 && wall < 46000) || wall >= 60000
                   ? 2900 : 4000;
      if(mode == M_BATT_OFF)       // low for two readings, switched off by the button, woken
         vbat_mv = (wall >= 41000 && wall < 50000) || (wall >= 68000 && wall < 71000)   // (60 s),
                   ? 2900 : 4000;                               // then low for one reading
      if(mode == M_BATT_HOVER && wall >= 18000)   // readings around 3.4 V: 3.42, 3.39 V, ...,
         vbat_mv = wall >= 70000 ? ((wall / 3000) % 2 ? 2990 : 3020)    // 3.1 V (RECHARGE),
                 : wall >= 50000 ? 3100 : (wall / 3000) % 2 ? 3390 : 3420;   // around 3.0 V
      if(mode == M_BATT_LOW)       // almost empty at the start, 3.3 V from 60 s on
         vbat_mv = wall < 60000 ? 3100 : 3300;
      if(mode == M_BATT_MIX && wall >= 18000)    // 3.38 V (readings at 19.8 and 22.8 s), one
         vbat_mv = wall >= 25000 && wall < 26000 ? 2990 : 3380;   // dip to 2.99 V at 25.8 s
      if(mode == M_BATT_EMPTY)     // empty at the start and at the first wake, charged later
         vbat_mv = wall < 30000 ? 2900 : 4000;
      if(mode == M_BATT_AUTO && wall >= 58000)   // the third reading below 3.0 V (64.8 s) comes
         vbat_mv = 2900;                         // in the same pass as the power off (Cell 2)
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
   if(mode == M_BATT_EMPTY && wall < 30000) {   // LOW BATT at once, no relay pulses
      check_centred();
      centred_checks++;
      CHECK_EQ(relay_calls, 0);
   }
   if(mode == M_BATT_AUTO) {         // switched off by the battery, not by the time: says so
      check_centred();               // LOW BATT alone in the middle, not the dark main screen
      centred_checks++;
   }
   CHECK(!adc_on);                   // no reference current while switched off
   // no current through the pins: display lines and external interface
   // released, without input buffer (the lines may be open)
   CHECK(!OLED_PWR && LATAbits.LATA2 && LATAbits.LATA3 && (ANSELA & 0x0C) == 0x0C);
   CHECK(LATDbits.LATD2 && (ANSELD & 0x06) == 0x06);
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
void meas_init(void) { adc_on = 1; }
void meas_off(void) { adc_on = 0; }
uint16_t meas_battery(void) { return vbat_mv; }
void relays_set(uint8_t l, uint8_t c, uint8_t sw) {
   rel.l = l; rel.c = c; rel.sw = sw;
   relay_calls++;
   if(!LATDbits.LATD2) key_low_in_tune = 1;
   if(mode == M_RELAYS && !LATDbits.LATD2) switched[sw & 1][l & 127][c & 127] = 1;
   if(mode == M_RESUME_OFF && !LATDbits.LATD2) {
      uint8_t *m = &seen1[sw & 1][l & 127][c & 127];
      if(key_falls == 1 && !*m) { *m = 1; n_first++; }
      if(key_falls == 2 && *m == 1) { *m = 2; n_again++; }
   }
   delay_ms(26);
}
// a load whose best match is L 20, C 30, capacitor at the output
void meas_take(meas_t *m, uint8_t n) {
   double d2 = (rel.l - opt_l) * (rel.l - opt_l) + (rel.c - opt_c) * (rel.c - opt_c) + (rel.sw ? 400 : 0);
   double g;
   memset(m, 0, sizeof *m);
   last_clr = wall;                  // meas_take clears the watchdog
   m->pf = rf_on ? (mode == M_CELLS_MAX ? 12000000 : mode == M_LOWPWR ? 970000 : 5000000) : 0;
   if(mode == M_HINT)                // none, 0.5 W, 20 W (above Cell 5), then 5 W
      m->pf = wall < 9000 ? 0 : wall < 10000 ? 500000 : wall < 11000 ? 20000000 : 5000000;
   if(mode == M_HINT_BLIP)           // 30 ms of 5 W while waiting, nothing else
      m->pf = wall >= 11000 && wall < 11030 ? 5000000 : 0;
   if(mode == M_HINT_END)            // 20 W (above Cell 5), later 0.5 W (below Cell 4)
      m->pf = wall >= 6000 && wall < 20000 ? 20000000 : wall >= 25000 ? 500000 : 0;
   if(mode == M_HINT_CW)             // CW at 0.5 W: 60 ms key down, 60 ms up
      m->pf = wall >= 7000 && (wall - 7000) % 120 < 60 ? 500000 : 0;
   g = d2 / (d2 + 60);
   if(mode == M_STOPAUTO) g = 0.25;                       // SWR 3.00 at every setting
   m->g2 = (uint32_t)(G2_ONE * (g2_floor + (1 - g2_floor) * g));
   if((mode == M_OVERLOAD || mode == M_BATT_OVL) && rf_on) {   // 30 W: the forward detector
      m->pf = 19000000;                                   // clips at every setting
      m->overflow = 1;
      m->g2 = (uint32_t)(G2_ONE * g);
   }
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

// text s (big or small) at pixel row y, x: drawing it there changes nothing
static int text_shows(int big, uint8_t y, uint8_t x, const char *s) {
   uint8_t before[512];
   int same;
   memcpy(before, disp_fb(), sizeof before);
   if(big) disp_big(y, x, s); else disp_small(y, x, s);
   same = !memcmp(before, disp_fb(), sizeof before);
   memcpy((uint8_t *)disp_fb(), before, sizeof before);
   return same;
}

#define small_shows(y, x, s) text_shows(0, y, x, s)

// the SWR value on the display reads s: drawing s there changes nothing
static int swr_shows(const char *s) {
   uint8_t before[512];
   int same;
   memcpy(before, disp_fb(), sizeof before);
   disp_big(LINE2, 60, s);
   same = !memcmp(before, disp_fb(), sizeof before);
   memcpy((uint8_t *)disp_fb(), before, sizeof before);
   return same;
}

// lit pixels of the battery symbol (x 115..125)
static int battery_px(void) {
   const uint8_t *fb = disp_fb();
   int n = 0;
   for(int p = 0; p < 4; p++) for(int x = 115; x <= 125; x++) n += fb[p * 128 + x] != 0;
   return n;
}

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
   case 95 * MIN:                    // setup menu: relay pulse 10 -> 12 ms, saved
      CHECK_EQ(cfg[CFG_RELAY_MS], 12);
      CHECK_EQ(ee[0x20], 0x5E);
      CHECK_EQ(ee[0x20 + 3 + CFG_RELAY_MS], 12);
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
      CHECK(OLED_PWR == (mode != M_BATT_EMPTY));             // empty: switched off
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
   if(mode == M_STOPAUTO && t == 5000)                      // no carrier yet: no SWR value
      CHECK(swr_shows("-.--"));
   if(mode == M_STOPAUTO && t == 12400) {                   // auto tune at SWR 3.00 everywhere:
      CHECK(!LATDbits.LATD2);                                // after the grid the progress shows
      CHECK(swr_shows("3.00"));                              // 3.00 again (blanked at the start)
   }
   if(mode == M_LOWPWR && t == 20000)                       // 0.97 W, below Cell 4: no tune,
      CHECK(!swr_shows("-.--"));                             // but the SWR is shown
   if(mode == M_HINT && t == 8500)                          // TUNE pressed, no carrier
      CHECK(small_shows(17, 60, "WAITING") && small_shows(25, 60, "FOR RF"));
   if(mode == M_HINT && t == 9500) {                        // 0.5 W, Cell 4 = 1.0 W
      CHECK(small_shows(17, 60, "POWER") && small_shows(25, 60, "TOO LOW"));
      CHECK(text_shows(1, LINE1, 60, "0.5"));                // the power meanwhile
   }
   if(mode == M_HINT && t == 10500) {                       // 20 W, Cell 5 = 15 W
      CHECK(small_shows(17, 60, "POWER") && small_shows(25, 60, "TOO HIGH"));
      CHECK(!LATDbits.LATD2);                                // still tuning
   }
   if(mode == M_BATT_DIP && t == 59000)                     // single low readings: still on
      CHECK_EQ(sleeps, 0);
   if(mode == M_BATT_DIP && t == 75000)                     // low for 9 s: LOW BATT, off
      CHECK_EQ(sleeps, 1);
   if(mode == M_HINT_BLIP && t == 14000)                    // after the blip: still waiting, and
      CHECK(small_shows(17, 60, "WAITING") && small_shows(25, 60, "FOR RF"));   // it says so
   if(mode == M_HINT_BLIP && t == 17500) {                  // NO POWER 10 s after the press,
      CHECK(LATDbits.LATD2);                                 // not 10 s after the blip
      CHECK(text_shows(1, LINE2, 0, "NO POWER"));
   }
   if(mode == M_HINT_END && t == 17500)                     // no tune with too much power:
      CHECK(LATDbits.LATD2 && text_shows(1, LINE2, 0, "TOO HIGH"));   // the end says so too
   if(mode == M_HINT_END && t == 41500)                     // ... and with too little
      CHECK(LATDbits.LATD2 && text_shows(1, LINE2, 0, "TOO LOW"));
   if(mode == M_HINT_CW && t >= 8000 && t < 16000 && (t - 7000) % 120 == 110)   // key up: the
      CHECK(small_shows(17, 60, "POWER") && small_shows(25, 60, "TOO LOW"));    // hint stays
   if(mode == M_HINT_CW && t == 17500)
      CHECK(LATDbits.LATD2 && text_shows(1, LINE2, 0, "TOO LOW"));
   if((mode == M_BATT_HOVER || mode == M_BATT_LOW || mode == M_BATT_MIX) && t > 6000 && OLED_PWR && !battery_px())
      batt_hidden++;                                         // the battery symbol blinks (not
                                                             // there during the greeting)
   if(mode == M_BATT_HOVER && t == 33000)                   // two readings below 3.4 V so far:
      CHECK(sleeps == 0 && !batt_hidden);                    // no blinking yet
   if(mode == M_BATT_HOVER && t == 50000) {                 // the third: 3.42 V in between did
      CHECK_EQ(sleeps, 0);                                   // not end the row, it blinks
      CHECK(batt_hidden > 0);
   }
   if(mode == M_BATT_HOVER && t == 59500)                   // 3.1 V for 3 readings: RECHARGE
      CHECK(text_shows(1, LINE2, 0, "RECHARGE"));
   if(mode == M_BATT_HOVER && t == 80000)                   // 2.99, 3.02, 2.99, 3.02 V: still on
      CHECK_EQ(sleeps, 0);
   if(mode == M_BATT_HOVER && t == 85000)                   // the third below 3.0 V: LOW BATT, off
      CHECK_EQ(sleeps, 1);
   if(mode == M_BATT_LOW && t >= 8000 && t < 40000 && t % 100 == 0 && text_shows(1, LINE2, 0, "RECHARGE"))
      recharge_seen++;                                       // RECHARGE comes and goes
   if(mode == M_BATT_LOW && t >= 64000 && t < 90000 && t % 100 == 0 && text_shows(1, LINE2, 0, "RECHARGE"))
      recharge_late++;
   if(mode == M_BATT_LOW && t == 40000) {                   // 3.1 V: RECHARGE, the symbol blinks,
      CHECK(recharge_seen > 0);                              // but the carrier at 10 s (SWR high)
      CHECK(batt_hidden > 0);                                // was tuned all the same
      CHECK_EQ(key_falls, 1);
      CHECK(rel.l == 20 && rel.c == 30 && rel.sw == 0);
   }
   if(mode == M_BATT_LOW && t == 50000)                     // TUNE pressed: it tunes too
      CHECK_EQ(key_falls, 2);
   if(mode == M_BATT_OVL && t == 25900)                     // 3.1 V: RECHARGE from 25.8 s, then
      CHECK(text_shows(1, LINE2, 0, "RECHARGE"));            // 0.3 s too much power: OVERLOAD
   if(mode == M_BATT_OVL && t >= 26000 && t < 26400 && t % 10 == 0)   // replaces it at once
      ovl_seen += text_shows(1, LINE2, 0, "OVERLOAD");
   if(mode == M_BATT_OVL && t == 27000)
      CHECK(ovl_seen > 20);
   if(mode == M_BATT_LOW && t == 40000)                     // RECHARGE 1.5 s of 9 s: the SWR
      CHECK(recharge_seen > 20 && recharge_seen < 80);
   if(mode == M_BATT_MIX && t == 25500)                     // two readings below 3.4 V so far
      CHECK(!batt_hidden);
   if(mode == M_BATT_MIX && t == 40000)                     // then a dip below 3.0 V: not off,
      CHECK(sleeps == 0 && batt_hidden > 0);                 // the symbol blinks (below 3.4 V,
                                                             // the mildest level of the row)
   if(mode == M_BATT_LOW && t == 90000) {                   // 3.3 V: RECHARGE gone, not switched off
      CHECK_EQ(recharge_late, 0);
      CHECK_EQ(sleeps, 0);
   }
   if(mode == M_BATT_EMPTY && t == 3000)                    // no greeting: LOW BATT and off
      CHECK_EQ(sleeps, 1);
   if(mode == M_BATT_EMPTY && t == 50000) {                 // charged: woken as usual, the
      CHECK(OLED_PWR && swr_shows("-.--"));                  // main screen, no tune
      CHECK_EQ(centred_checks, 2);
      CHECK_EQ(key_falls, 0);
   }
   if(mode == M_BATT_OFF && t == 75000)                     // a single low reading after waking:
      CHECK_EQ(sleeps, 1);                                   // the count before the power off is gone
   if(mode == M_RELAYS && t == 2 * MIN) {                   // tuned: L 3, C 90 at the output,
      CHECK_EQ(rel_key(), 90 << 7 | 3);                      // 5 W small at the top right
      CHECK(text_shows(0, 0, 100, "5.0W"));
      CHECK(text_shows(0, 0, 58, "0.32uH") && text_shows(0, 8, 58, "2957pF"));
      CHECK(text_shows(0, 0, 0, "L") && text_shows(0, 8, 0, "C"));
      CHECK(pix(113, 22) && pix(119, 25) && !pix(120, 25));  // 4.0 V: 5 of 7 columns
   }
   if(mode == M_RELAYS && t == 3 * MIN + 2000) {            // bypass: all cells empty
      CHECK_EQ(drawn_setting(), 0);
      CHECK(text_shows(0, 0, 58, "0.00uH") && text_shows(0, 8, 58, "   0pF"));
   }
   if(mode == M_RELAYS && t == 4 * MIN + 2000)              // back: the tuned setting
      CHECK_EQ(drawn_setting(), 90 << 7 | 3);
   if(t == 40000 && mode == M_BLIP)                         // carrier since 10 s: tuned, so
      CHECK(key_falls >= 1);                                 // not stuck in the setup menu
   if(t == 6 * MIN - 1) {
      CHECK_EQ(sleeps, mode == M_BATT_EMPTY ? 2 : mode == M_RESUME_OFF || mode == M_BATT_DIP
               || mode == M_BATT_OFF || mode == M_BATT_HOVER || mode == M_BATT_AUTO);
      if(mode == M_BATT_AUTO) CHECK_EQ(centred_checks, 1);
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
      case M_RELAYS:
         printf("relay view: %d tunes, %d live updates of the cells\n", key_falls, live_updates);
         CHECK(key_falls >= 1 && live_updates >= 1 && relays_checks >= 1);
         break;
      case M_EXTTUNE:
         break;
      case M_STOPAUTO:                                       // stopped by a short press: not
         CHECK_EQ(key_falls, 1);                             // started again at the same SWR
         CHECK(rel.l == 0 && rel.c == 0 && rel.sw == 0);
         break;
      case M_OVERLOAD:                                       // too much power: the tune stops
         printf("overload: %d tunes, %d relay steps\n", key_falls, relay_calls);
         CHECK_EQ(key_falls, 1);                             // after 64 clipped settings and
         CHECK(relay_calls <= 70);                           // does not start again
         CHECK(rel.l == 0 && rel.c == 0 && rel.sw == 0);
         CHECK(!st.bypass);
         break;
      case M_PULSE:                                          // 1 s carriers: each tune goes on
         printf("1 s carriers, 7 s pauses: %d tunes\n", key_falls);   // with the search of the
         CHECK(key_falls >= 2 && key_falls <= 12);           // last one and gets to the best
         CHECK(rel.l == 20 && rel.c == 30 && rel.sw == 0);   // setting
         break;
      case M_PULSE_SLOW:                                     // pauses longer than 60 s: a new
         printf("1 s carriers, 70 s pauses: %d tunes\n", key_falls);  // search each time,
         CHECK(key_falls <= 3);                              // which ends the chain once it
         break;                                              // finds nothing better
      case M_RESUME_OFF:                                     // the 1 s carrier left a search to
         printf("tune after power off: %d of %d settings measured again\n", n_again, n_first);
         CHECK_EQ(key_falls, 2);                             // continue, but the clock stood still
         CHECK(n_again * 2 >= n_first);                      // while switched off: a new search
         CHECK(rel.l == 20 && rel.c == 30 && rel.sw == 0);
         break;
      case M_RESUME_QSY:                                     // the load changed within the 60 s:
         printf("other band within 60 s: %d tunes, L %d C %d sw %d\n", key_falls, rel.l, rel.c, rel.sw);
         CHECK(rel.l == 3 && rel.c == 90 && rel.sw == 0);    // a new search, not the old one's
         break;                                              // measurements
      case M_RESUME_EMPTY:                                   // carriers too short to measure a
         printf("0.25 s carriers: %d tunes\n", key_falls);   // setting: the chain ends, no tune
         CHECK(key_falls <= 3);                              // (key line low) on every carrier
         break;
      case M_HINT_CW:                                        // below Cell 4: no tune at all
         CHECK_EQ(key_falls, 1);
         CHECK(rel.l == 20 && rel.c == 30 && rel.sw == 0);
         break;
      case M_HINT_END:                                       // two TUNE presses, nothing
         CHECK_EQ(key_falls, 2);                             // tuned, nothing changed
         CHECK(rel.l == 20 && rel.c == 30 && rel.sw == 0);
         break;
      case M_HINT_BLIP:                                      // nothing tuned, nothing changed
         CHECK_EQ(key_falls, 1);
         CHECK(rel.l == 20 && rel.c == 30 && rel.sw == 0);
         break;
      case M_HINT:                                           // 5 W: tuned, the hint gone
         CHECK_EQ(key_falls, 1);                             // (check_line2)
         CHECK(line2_checks >= 1);
         CHECK(rel.l == 20 && rel.c == 30 && rel.sw == 0);
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
   if(mode == M_BLIP || mode == M_UNMATCH || mode == M_NOMATCH || mode == M_LOWPWR || mode == M_EXTTUNE
      || mode == M_STOPAUTO || mode == M_OVERLOAD || mode == M_PULSE || mode == M_PULSE_SLOW || M_RESUME(mode) || mode == M_BATT_LOW) {   // start in bypass, so that
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
   if(mode == M_BATT_AUTO) {                                 // power off after 1 min
      static const uint8_t c[12] = {0x05, 0x01, 0x10, 0x10, 0x15, 0x13, 0x01, 0x04, 0x14, 0x60, 0x05, 0x02};
      memcpy((void *)Cells, c, 12);
   }
   if(mode == M_RELAYS) {                                    // Cell 13: relay view; bypass
      Cells[CFG_LAYOUT] = 0x01;                              // on at 3 min, off at 4 min;
      opt_l = 3; opt_c = 90;                                 // another load than the saved tune
      press[0].from = 3 * MIN; press[0].to = 3 * MIN + 100;   // short presses
      press[1].from = 4 * MIN; press[1].to = 4 * MIN + 100;
   }
   if(mode == M_EXTDARK) {
      static const uint8_t c[12] = {0x01, 0x30, 0x10, 0x10, 0x15, 0x13, 0x01, 0x04, 0x14, 0x60, 0x05, 0x02};
      memcpy((void *)Cells, c, 12);                          // display off after 1 min
   }
   n_press = mode == M_RELAYS ? 2 : 0;
   if(mode == M_BLIP) {                                       // ends 0.4 s after the greeting
      press[0].from = 3800; press[0].to = 4600;
      n_press = 1;
   }
   if(mode == M_NOPOWER) {                                    // long press, no carrier
      press[0].from = 20000; press[0].to = 20600;
      n_press = 1;
   }
   if(mode == M_STOPAUTO) {                                   // short press during the auto tune
      press[0].from = 12500; press[0].to = 12600;
      n_press = 1;
   }
   if(mode == M_HINT || mode == M_HINT_BLIP || mode == M_HINT_CW) {   // long press: TUNE without a carrier
      press[0].from = 6000; press[0].to = 6600;
      n_press = 1;
   }
   if(mode == M_RESUME_OFF) {                                 // wake, then a long press: tune
      press[0].from = 40000; press[0].to = 42000;
      press[1].from = 50000; press[1].to = 50500;
      n_press = 2;
   }
   if(mode == M_HINT_END) {                                   // TUNE with 20 W, then with 0.5 W
      press[0].from = 6000; press[0].to = 6600;
      press[1].from = 30000; press[1].to = 30600;
      n_press = 2;
   }
   if(mode == M_BATT_OVL) vbat_mv = 3100;                     // RECHARGE from the start
   if(mode == M_BATT_LOW) {                                   // 3.1 V at the start already,
      vbat_mv = 3100;                                        // then TUNE (carrier from 10 s)
      press[0].from = 40000; press[0].to = 40600;
      n_press = 1;
   }
   if(mode == M_BATT_EMPTY) {                                 // wake with the battery empty, then
      vbat_mv = 2900;
      press[0].from = 20000; press[0].to = 22000;            // charged
      press[1].from = 40000; press[1].to = 42000;
      n_press = 2;
   }
   if(mode == M_BATT_OFF) {                                   // extra long press: power off, then
      press[0].from = 47000; press[0].to = 50000;            // wake
      press[1].from = 60000; press[1].to = 62000;
      n_press = 2;
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
                     : mode == M_EXTTUNE ? "test_app ext-tune" : mode == M_STOPAUTO ? "test_app stop-auto"
                     : mode == M_OVERLOAD ? "test_app overload" : mode == M_PULSE ? "test_app pulse"
                     : mode == M_PULSE_SLOW ? "test_app pulse-slow" : mode == M_RESUME_OFF ? "test_app resume-off"
                     : mode == M_RESUME_QSY ? "test_app resume-qsy" : mode == M_RESUME_EMPTY ? "test_app resume-empty"
                     : mode == M_BATT_DIP ? "test_app batt-dip" : mode == M_HINT ? "test_app hint"
                     : mode == M_BATT_OFF ? "test_app batt-off" : mode == M_HINT_BLIP ? "test_app hint-blip"
                     : mode == M_HINT_END ? "test_app hint-end" : mode == M_HINT_CW ? "test_app hint-cw"
                     : mode == M_BATT_HOVER ? "test_app batt-hover" : mode == M_BATT_LOW ? "test_app batt-low"
                     : mode == M_BATT_MIX ? "test_app batt-mix" : mode == M_BATT_OVL ? "test_app batt-ovl"
                     : mode == M_BATT_AUTO ? "test_app batt-auto" : mode == M_BATT_EMPTY ? "test_app batt-empty"
                     : "test_app relays");
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
   // setting 3, a short one (10 -> 12 ms), 11 long ones to the SAVE page, a
   // short one
   {
      uint32_t t = 91 * MIN + 9000;
      for(int i = 0; i < 15; i++, t += 1500) {
         uint32_t len = (i == 2 || i == 14) ? 100 : 500;
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
