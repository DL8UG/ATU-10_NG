// Display: everything is drawn into a framebuffer in RAM; only the changed
// parts go to the display, a little at a time from the main loop, and never
// while relays switch or the detectors are measured.

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#define LINE1  0                     // big text lines (16 px high): top pixel row
#define LINE2  18
#define LINE_MID 9                   // one line in the middle (visible rows 9..22)

void disp_power(uint8_t on);         // on: power up, initialize, redraw
uint8_t disp_is_on(void);
void disp_clear(void);
void disp_big(uint8_t y, uint8_t x, const char *s);        // 12 px per character
void disp_small(uint8_t y, uint8_t x, const char *s);      // 6 px per character, from pixel row y
void disp_battery(uint16_t mv);
void disp_bat_small(uint16_t mv);     // small battery behind the SWR value (relay view)
void disp_cells(uint8_t y, uint8_t x, uint8_t bits);   // 7 relay cells, bit 0 left
void disp_service(void);             // sends one changed page; call often
void disp_flush(void);               // sends all changes now
void disp_refresh(void);             // sends everything again (repairs a garbled picture)

#endif
