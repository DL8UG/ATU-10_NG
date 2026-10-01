// 1 ms system tick and raw sampling of the button and the external start
// line. The interrupt only counts; all decisions are made in buttons.c.

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// press time in 10 ms units while held (saturates at 255), 0 = released
extern volatile uint8_t btn_held, ext_held;
// length of the last finished press in 10 ms units, 0 = none pending
extern volatile uint8_t btn_released, ext_released;

uint32_t tick_ms(void);                  // read atomically
uint8_t elapsed(uint32_t since, uint32_t ms);

#endif
