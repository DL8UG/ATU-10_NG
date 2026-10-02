#include "board.h"
#include "i2c_soft.h"
#include "oled.h"

#define ADDR       0x78              // write address
#define COL_SHIFT  2                 // the visible columns start at 2

// Sent once after each power-up, display off meanwhile. The analog
// settings (clock, charge pump, pre-charge, VCOMH) are never sent to a
// running display.
static const uint8_t init_seq[] = {
   0xAE,          // display off
   0xD5, 0x80,    // clock divider
   0xA8, 63,      // multiplex
   0xD3, 64,      // display offset
   0x40,          // start line 0
   0x8D, 0x14,    // charge pump on
   0x81, 255,     // contrast
   0xD9, 0xF1,    // pre-charge
   0x20, 0x02,    // page addressing
   0x21, 0, 127,  // column range
   0x2E,          // no scrolling
   0xA0, 0xC0,    // segment / COM scan direction
   0xDA, 0x02,    // COM pins
   0xDB, 0x40,    // VCOMH
   0xA4,          // show RAM
   0xA6,          // not inverted
};

static uint8_t commands(const uint8_t *c, uint8_t n) {
   uint8_t nack;
   i2c_start();
   nack = i2c_write(ADDR);
   i2c_write(0x00);                  // command stream
   while(n--) i2c_write(*c++);
   i2c_stop();
   return nack;
}

uint8_t oled_write(uint8_t page, uint8_t x, const uint8_t *d, uint8_t n) {
   uint8_t a[3], nack;
   x += COL_SHIFT;
   a[0] = (uint8_t)(0xB0 + page);
   a[1] = x & 0x0F;
   a[2] = (uint8_t)(0x10 | x >> 4);
   nack = commands(a, 3);
   i2c_start();
   nack |= i2c_write(ADDR);
   i2c_write(0x40);                  // data stream
   while(n--) i2c_write(*d++);
   i2c_stop();
   return nack;
}

// 1 if the display acknowledges its address (about 0.2 ms)
uint8_t oled_present(void) {
   uint8_t nack;
   i2c_start();
   nack = i2c_write(ADDR);
   i2c_stop();
   return !nack;
}

uint8_t oled_init(void) {
   static const uint8_t zero[16] = {0};
   static const uint8_t on = 0xAF;
   uint8_t i, p, nack = 1;
   i2c_init();
   for(i = 0; i < 10 && nack; i++) {   // the controller needs a moment
      nack = commands(init_seq, sizeof init_seq);
      if(nack) delay_ms(100);
   }
   if(nack) return nack;                // no display: do not send the rest
   for(p = 0; p < 8; p++)               // the controller has 8 pages
      for(i = 0; i < 128; i += 16) oled_write(p, (uint8_t)(i - COL_SHIFT), zero, 16);
   nack |= commands(&on, 1);
   return nack;
}
