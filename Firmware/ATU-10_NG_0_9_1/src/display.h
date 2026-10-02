// Display: everything is drawn into a framebuffer in RAM; only the changed
// parts go to the display, a little at a time from the main loop, and never
// while relays switch or the detectors are measured.

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#define LINE1  0                     // big text lines (16 px high)
#define LINE2  1

void disp_power(uint8_t on);         // on: power up, initialize, redraw
uint8_t disp_is_on(void);
void disp_clear(void);
void disp_big(uint8_t line, uint8_t x, const char *s);     // 12 px per character
void disp_small(uint8_t row, uint8_t x, const char *s);    // 6 px per character, row 0..3
void disp_battery(uint16_t mv);
void disp_service(void);             // sends one changed page; call often
void disp_flush(void);               // sends all changes now
void disp_refresh(void);             // sends everything again (repairs a garbled picture)

#endif
