// Renders the display layout on the PC: ASCII on stdout and PBM images
// (build/screen_*.pbm). Checks that nothing is drawn outside its place.
#include "check.h"
#include <stdint.h>
#include <string.h>

// stand-ins for the hardware
#define BOARD_H
static uint8_t OLED_PWR, I2C_SCL, I2C_SDA;
static void delay_ms(uint16_t ms) { (void)ms; }
uint8_t oled_init(void) { return 0; }
uint8_t oled_write(uint8_t page, uint8_t x, const uint8_t *d, uint8_t n) { (void)page; (void)x; (void)d; (void)n; return 0; }
void i2c_init(void) {}
#define I2C_SOFT_H
#define OLED_H

#include "../src/display.c"

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
   disp_big(LINE1, 28, "ATU-10");
   disp_small(2, 31, "FW NG 0.1.0");
   disp_small(3, 13, "DESIGNED BY DL8UG");
   dump("greeting");
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
   int empty = 0, full = 0;
   for(int y = 0; y < 32; y++) for(int x = 115; x < 126; x++) empty += px(x, y);
   disp_battery(4200);
   for(int y = 0; y < 32; y++) for(int x = 115; x < 126; x++) full += px(x, y);
   CHECK_EQ(full - empty, 26 * 5);
   // line 2 overwrites cleanly: old text gone
   disp_big(LINE2, 0, "TUNE");
   disp_big(LINE2, 0, "SWR ");
   dump("tune");
   return check_done("test_display");
}
