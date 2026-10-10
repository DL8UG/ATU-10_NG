// Renders the display layout on the PC: ASCII on stdout and PBM images
// (build/screen_*.pbm). Checks that nothing is drawn outside its place.
#include "check.h"
#include <stdint.h>
#include <string.h>

// stand-ins for the hardware
#define BOARD_H
static uint8_t OLED_PWR, I2C_SCL, I2C_SDA, ANSELA;
static uint32_t now_ms;
static int display_ok = 1, acks_data = 1, inits, writes;
static void delay_ms(uint16_t ms) { now_ms += ms; }
uint32_t tick_ms(void) { return now_ms; }
uint8_t oled_init(void) { inits++; return !display_ok; }
uint8_t oled_present(void) { return display_ok; }
uint8_t oled_on(void) { return !display_ok; }
uint8_t oled_write(uint8_t page, uint8_t x, const uint8_t *d, uint8_t n) {
   (void)page; (void)x; (void)d; (void)n;
   writes++;
   return !(display_ok && acks_data);
}
void i2c_init(void) {}
#define I2C_SOFT_H
#define OLED_H

#include "../src/display.c"
#include "../src/version.h"

static int px(int x, int y) { return fb[y / 8][x] >> (y % 8) & 1; }

static void dump(const char *name) {
   char path[64];
   snprintf(path, sizeof path, "build/screen_%s.pbm", name);
   FILE *f = fopen(path, "w");
   fprintf(f, "P1\n128 32\n");
   printf("--- %s\n", name);
   for(int y = 0; y < 32; y++) {
      for(int x = 0; x < 128; x++) {
         fprintf(f, "%d ", px(x, y));
         putchar(px(x, y) ? '#' : '.');
      }
      fputc('\n', f);
      putchar('\n');
   }
   fclose(f);
}

int main(void) {
   disp_power(1);
   // switched off: the lines released (no current through the module's
   // pull-ups), no input buffer on them; back on: inputs again
   disp_power(0);
   CHECK(!OLED_PWR && I2C_SCL && I2C_SDA);
   CHECK_EQ(ANSELA, 0x0C);
   disp_power(1);
   CHECK(OLED_PWR);
   CHECK_EQ(ANSELA, 0);
   // greeting
   disp_clear();
   disp_big(LINE1, 29, "ATU-10");
   disp_small(24, 13, "HARDWARE BY N7DDC");
   dump("greeting");
   disp_clear();
   disp_big(LINE1, 17, "NG " FW_VERSION);
   disp_small(24, 13, "FIRMWARE BY DL8UG");
   dump("greeting2");
   // power off: alone in the middle
   disp_clear();
   disp_big(LINE_MID, 11, "POWER OFF");
   dump("poweroff");
   // main screen
   disp_clear();
   disp_big(LINE1, 0, "PWR");
   disp_big(LINE1, 42, "=");
   disp_big(LINE1, 60, "5.0");
   disp_big(LINE1, 96, "W");
   disp_big(LINE2, 0, "SWR");
   disp_big(LINE2, 42, "=");
   disp_big(LINE2, 60, "1.05");
   disp_battery(3900);
   dump("main");
   // the text ends before the battery symbol (x 115)
   for(int y = 0; y < 32; y++) for(int x = 108; x < 115; x++) CHECK(!px(x, y));
   // battery: empty and full differ only inside
   disp_battery(3000);
   int empty = 0, full = 0, half = 0, below = 0;
   for(int y = 0; y < 32; y++) for(int x = 115; x < 126; x++) empty += px(x, y);
   disp_battery(4200);
   for(int y = 0; y < 32; y++) for(int x = 115; x < 126; x++) full += px(x, y);
   CHECK_EQ(full - empty, 26 * 5);
   // 3.0 V empty (LOW BATT), 3.6 V half, below 3.0 V as empty
   disp_battery(3600);
   for(int y = 0; y < 32; y++) for(int x = 115; x < 126; x++) half += px(x, y);
   CHECK_EQ(half - empty, 13 * 5);
   disp_battery(2900);
   for(int y = 0; y < 32; y++) for(int x = 115; x < 126; x++) below += px(x, y);
   CHECK_EQ(below, empty);
   disp_battery(0);                              // no symbol (it blinks)
   for(int y = 0; y < 32; y++) for(int x = 115; x < 126; x++) CHECK(!px(x, y));
   disp_battery(4200);
   // line 2 overwrites cleanly: old text gone
   disp_big(LINE2, 0, "TUNE");
   disp_big(LINE2, 0, "SWR ");
   dump("tune");
   // TUNE without a suitable carrier: two small lines where the SWR so far
   // follows, ending before the battery symbol
   disp_big(LINE2, 0, "         ");
   disp_big(LINE2, 0, "TUNE");
   disp_small(17, 60, "POWER");
   disp_small(25, 60, "TOO HIGH");
   dump("hint");
   for(int y = 0; y < 32; y++) for(int x = 108; x < 115; x++) CHECK(!px(x, y));
   for(int x = 60; x < 108; x++) CHECK(!px(x, 16));   // the gap row under line 1 stays free
   disp_small(17, 60, "        ");                   // back to the main screen
   disp_small(25, 60, "        ");
   disp_big(LINE2, 0, "SWR ");
   disp_big(LINE2, 42, "=");
   disp_big(LINE2, 60, "1.05");

   // drawing what is already there sends nothing (the battery symbol is
   // drawn again every 3 s)
   disp_power(1);
   disp_flush();
   writes = 0;
   disp_battery(4200);
   disp_big(LINE2, 60, "1.05");
   disp_flush();
   CHECK_EQ(writes, 0);
   disp_battery(3000);                           // a change is sent
   disp_flush();
   CHECK(writes > 0);

   // ---- relay view (Cell 13), drawn as app.c does it
   disp_clear();
   disp_small(0, 0, "L");
   disp_small(8, 0, "C");
   disp_cells(1, 7, 0x0D);                       // 0.1 + 0.45 + 1.0 uH
   disp_cells(9, 7, 0x2C);                       // 100 + 220 + 1000 pF
   disp_small(0, 58, "1.55uH");
   disp_small(8, 58, "1320pF");
   disp_small(0, 100, "5.0W");
   disp_small(8, 100, " ANT");
   disp_big(LINE2, 0, "SWR");
   disp_big(LINE2, 42, "=");
   disp_big(LINE2, 60, "1.05");
   disp_bat_small(3900);
   dump("relays");
   // cells: filled for a set bit, frame only else; 7 columns apart
   for(int i = 0; i < 7; i++) {
      CHECK_EQ(px(7 + 7 * i + 2, 3), (0x0D >> i) & 1);    // centre
      CHECK(px(7 + 7 * i, 1) && px(7 + 7 * i + 4, 5));     // frame
      CHECK(!px(7 + 7 * i + 5, 3) && !px(7 + 7 * i + 6, 3));   // gap
      CHECK_EQ(px(7 + 7 * i + 2, 11), (0x2C >> i) & 1);
   }
   for(int x = 0; x < 128; x++) CHECK(!px(x, 0) || x < 7 || x >= 56);   // cells start at row 1
   // columns 126, 127 stay dark: the display shows x 0..125 only (more
   // wraps around to the left edge); the power and TX / ANT end at x 122,
   // one column before the cap of the small battery
   for(int y = 0; y < 32; y++) CHECK(!px(126, y) && !px(127, y));
   for(int y = 0; y < 16; y++) CHECK(!px(123, y) && !px(124, y) && !px(125, y));
   // small battery: frame x 113..123 rows 22..28, cap x 124, behind the
   // SWR value (x 60..107), filled from the left
   for(int y = 16; y < 32; y++) for(int x = 108; x < 113; x++) CHECK(!px(x, y));
   for(int x = 113; x <= 125; x++) CHECK(!px(x, 21) && !px(x, 29));
   CHECK(px(113, 22) && px(123, 28) && px(124, 24) && px(124, 26) && !px(124, 23) && !px(125, 25));
   {
      int cols[4];
      const uint16_t mv[4] = {3000, 3600, 4200, 0};
      for(int k = 0; k < 4; k++) {
         disp_bat_small(mv[k]);
         cols[k] = 0;
         for(int x = 115; x <= 121; x++) cols[k] += px(x, 25);
         for(int x = 115; x <= 121; x++)                 // from the left
            CHECK_EQ(px(x, 25), x - 115 < cols[k]);
      }
      CHECK_EQ(cols[0], 0);
      CHECK_EQ(cols[1], 3);
      CHECK_EQ(cols[2], 7);
      CHECK_EQ(cols[3], 0);                      // blinking: nothing
      for(int y = 16; y < 32; y++) for(int x = 110; x < 128; x++) CHECK(!px(x, y));
      disp_bat_small(3400);
      dump("relays_low");
   }
   // 18.5 uH, 4059 pF, C on the transmitter side, 12 W; tuning
   disp_cells(1, 7, 0x7F);
   disp_cells(9, 7, 0x7F);
   disp_small(0, 58, "18.5uH");
   disp_small(8, 58, "4059pF");
   disp_small(0, 100, " 12W");
   disp_small(8, 100, "  TX");
   disp_big(LINE2, 0, "         ");
   disp_big(LINE2, 0, "TUNE");
   disp_big(LINE2, 60, "1.62");
   disp_bat_small(4100);
   dump("relays_tune");
   // all cells off again: only the frames left
   disp_cells(1, 7, 0);
   for(int i = 0; i < 7; i++) CHECK(!px(7 + 7 * i + 2, 3));

   // ---- restart of a display that does not answer
   {
      uint32_t t_init[20];
      int n = 0, last;
      disp_power(1);
      disp_flush();
      CHECK(!restart);
      display_ok = 0;                            // unplugged
      inits = 0;
      for(int k = 0; k < 400000 && n < 9; k += 10) {   // 10 ms per main loop pass
         disp_refresh();
         last = inits;
         disp_service();
         if(inits != last) t_init[n++] = now_ms;
         now_ms += 10;
      }
      // first restart at once, then after 2, 4, 8, 16, 32, 64, 64 s (+ 0.5 s power cycle)
      for(int i = 1; i < n; i++) {
         uint32_t want = 2000u << (i - 1 < 5 ? i - 1 : 5);
         uint32_t got = t_init[i] - t_init[i - 1];
         CHECK(got >= want && got <= want + 600);
      }
      CHECK_EQ(n, 9);
      // while a restart is pending nothing is sent, also not by disp_flush
      writes = 0;
      disp_flush();
      for(int k = 0; k < 50; k++) { disp_service(); now_ms += 10; }
      CHECK_EQ(writes, 0);
      // plugged in again: found by the ping within about a second
      display_ok = 1;
      inits = 0;
      writes = 0;
      for(int k = 0; k < 200 && !inits; k++) { disp_service(); now_ms += 10; }
      CHECK_EQ(inits, 1);                        // and the picture sent with it
      for(int k = 0; k < 10; k++) disp_service();
      CHECK(writes > 0 && !restart && !backoff);
      // a display that answers the ping but not the data: the back-off holds
      acks_data = 0;
      inits = 0;
      for(int k = 0; k < 3000; k++) { disp_refresh(); disp_service(); now_ms += 10; }   // 30 s
      printf("restarts in 30 s, ping answered, data not: %d\n", inits);
      CHECK(inits <= 5);                         // at once, +2, +4, +8, +16 s
      // switching on again clears a pending restart
      acks_data = 1;
      disp_power(1);
      CHECK(!restart);
   }
   return check_done("test_display");
}
