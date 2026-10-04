// Renders the display layout on the PC: ASCII on stdout and PBM images
// (build/screen_*.pbm). Checks that nothing is drawn outside its place.
#include "check.h"
#include <stdint.h>
#include <string.h>

// stand-ins for the hardware
#define BOARD_H
static uint8_t OLED_PWR, I2C_SCL, I2C_SDA;
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
