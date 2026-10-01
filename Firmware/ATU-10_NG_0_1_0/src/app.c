// ATU-10 NG firmware: main program
#include "board.h"
#include "timer.h"
#include "cells.h"

void main(void) {
   uint32_t t;
   board_init();
   cells_load();
   WDT_ON();
   t = tick_ms();
   while(1) {
      CLRWDT();
      if(elapsed(t, 1000)) {   // placeholder: blink the green LED
         t += 1000 + cfg[CFG_RELAY_MS];
         LED_GREEN = 0;
         delay_ms(30);
         LED_GREEN = 1;
      }
   }
}
