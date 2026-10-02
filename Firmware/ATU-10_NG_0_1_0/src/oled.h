// SSD1306 controller of the 128 x 32 display

#ifndef OLED_H
#define OLED_H

#include <stdint.h>

uint8_t oled_init(void);             // after power-up; 0 = display answered
uint8_t oled_present(void);          // 1 = the display answers its address
uint8_t oled_on(void);               // display on (after the first picture)
// writes n bytes at page (0..3), column x; 0 = acknowledged
uint8_t oled_write(uint8_t page, uint8_t x, const uint8_t *d, uint8_t n);

#endif
