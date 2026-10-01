#include "board.h"
#include "timer.h"

static volatile uint32_t tick;
static volatile uint8_t div10;
volatile uint8_t btn_held, ext_held, btn_released, ext_released;

// No function calls in here: every call level used by the interrupt is
// missing for the main program on the 16 level hardware stack.
void __interrupt() isr(void) {
   if(PIR0bits.TMR0IF) {
      PIR0bits.TMR0IF = 0;
      tick++;
      if(++div10 >= 10) {
         div10 = 0;
         if(BUTTON_DOWN) {
            if(btn_held != 255) btn_held++;
         }
         else if(btn_held) {
            btn_released = btn_held;
            btn_held = 0;
         }
         if(EXT_START) {
            if(ext_held != 255) ext_held++;
         }
         else if(ext_held) {
            ext_released = ext_held;
            ext_held = 0;
         }
      }
   }
}

uint32_t tick_ms(void) {
   uint32_t t;
   INTCONbits.GIE = 0;
   t = tick;
   INTCONbits.GIE = 1;
   return t;
}

// 1 if at least ms have passed since the tick value 'since' (wrap-safe)
uint8_t elapsed(uint32_t since, uint32_t ms) {
   return tick_ms() - since >= ms;
}
