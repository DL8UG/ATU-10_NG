// ATU-10 NG firmware: main program
#include "board.h"
#include "timer.h"
#include "cells.h"
#include "meas.h"

void main(void) {
   uint32_t t;
   board_init();
   cells_load();
   meas_init();
   WDT_ON();
   t = tick_ms();
   while(1) {
      CLRWDT();
      if(elapsed(t, 1000)) {   // placeholder: blink green while RF is seen
         meas_t m;
         t += 1000;
         meas_battery();
         meas_take(&m, 16);
         if(pwr_x10(m.pf) > 0 && swr_x100(m.g2) < 200) {
            LED_GREEN = 0;
            delay_ms(30);
            LED_GREEN = 1;
         }
      }
   }
}
