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

typedef struct { uint32_t from, to; } span_t;
static span_t press[40] = {         // button held (ms); the setup menu presses are added in main()
   {80 * MIN, 80 * MIN + 2000},      // wake from power off
   {82 * MIN, 82 * MIN + 100},       // short: bypass on
   {83 * MIN, 83 * MIN + 100},       // short: bypass off
   {84 * MIN, 84 * MIN + 500},       // long: tune
   {86 * MIN, 86 * MIN + 3000},      // extra long: power off
   {91 * MIN, 91 * MIN + 7000},      // wake and keep holding: setup menu
};
static int n_press = 6;
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

void fake_ms(uint32_t ms) {
   while(ms--) {
      wall++;
      PORTBbits.RB5 = !in_spans(press, n_press);                        // low = pressed
      PORTDbits.RD1 = !in_spans(ext_start, 2);                          // low = start
      PORTDbits.RD2 = LATDbits.LATD2;                                   // key line as driven
      rf_on = in_spans(carrier, sizeof carrier / sizeof *carrier);
      if(wall == 100 * MIN) vbat_mv = 3300;                             // battery empty
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
   while(PORTBbits.RB5) fake_ms(10);  // until the button goes down (wake-up interrupt)
}

// ---- hardware the application calls
relays_t rel;
uint16_t vbat_mv = 4000;
void board_init(void) { INTCONbits.GIE = 1; PIE0bits.TMR0IE = 1; }
void delay_ms(uint16_t ms) { fake_ms(ms); }
void meas_init(void) {}
uint16_t meas_battery(void) { return vbat_mv; }
void relays_set(uint8_t l, uint8_t c, uint8_t sw) {
   rel.l = l; rel.c = c; rel.sw = sw;
   relay_calls++;
   if(!LATDbits.LATD2) key_low_in_tune = 1;
   fake_ms(26);
}
// a load whose best match is L 20, C 30, capacitor at the output
void meas_take(meas_t *m, uint8_t n) {
   double d2 = (rel.l - 20.0) * (rel.l - 20.0) + (rel.c - 30.0) * (rel.c - 30.0) + (rel.sw ? 400 : 0);
   memset(m, 0, sizeof *m);
   m->pf = rf_on ? 5000000 : 0;
   m->g2 = (uint32_t)(G2_ONE * (d2 / (d2 + 60)));
   m->pr = (uint32_t)((double)m->pf * m->g2 / G2_ONE);
   m->stable = 1;
   m->spread = 2;
   fake_ms(n > 8 ? 4 : 1);
}
void hal_relay_set(uint8_t l, uint8_t c, uint8_t sw) { in_tune = 1; relays_set(l, c, sw); }
void hal_sample(meas_t *m, uint8_t n) { meas_take(m, n); }
void hal_wait_ms(uint8_t ms) { fake_ms(ms); }
// display hardware
uint8_t oled_init(void) { return 0; }
uint8_t oled_write(uint8_t page, uint8_t x, const uint8_t *d, uint8_t n) { (void)page; (void)x; (void)d; (void)n; return 0; }
void i2c_init(void) {}
// data EEPROM, erased
static uint8_t ee[256];
uint8_t nvm_read(uint8_t a) { return ee[a]; }
void nvm_write(uint8_t a, uint8_t v) { ee[a] = v; }

// ---- what the tuner should have done, checked at these points in time
static int tunes_seen(void) {        // a tune switched the relays since the last call
   int t = in_tune;
   in_tune = 0;
   return t;
}

static void checkpoint(uint32_t t) {
   if(getenv("TRACE") && t >= 91 * MIN && t <= 91 * MIN + 40000 && t % 500 == 0)
      printf("%6.1f s  btn %d held %3d  oled %d  sleeps %d  relay_ms %d  ticks %u\n", (t - 91 * MIN) / 1000.0,
             !PORTBbits.RB5, btn_held, OLED_PWR, sleeps, cfg[CFG_RELAY_MS], tick_ms());
   switch(t) {
   case 4000:                        // after the greeting
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

int main(void) {
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
   printf("relay steps %d over the run\n", relay_calls);
   return check_done("test_app");
}
