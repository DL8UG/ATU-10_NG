#include "board.h"
#include "timer.h"
#include "buttons.h"

#define SHORT_MIN   3
#define LONG_T      25
#define XLONG_T     250
#define EXT_SHORT_MIN 2
#define EXT_SHORT_MAX 10
#define EXT_LONG_T  20

static uint8_t long_sent, xlong_sent, ext_long_sent, pending;

void buttons_unget(uint8_t ev) {
   pending = ev;
}

void buttons_clear(void) {
   INTCONbits.GIE = 0;
   btn_released = 0;
   ext_released = 0;
   INTCONbits.GIE = 1;
   pending = EV_NONE;
   long_sent = xlong_sent = 1;      // a press still going on gives no event
   ext_long_sent = 1;
}

uint8_t buttons_event(void) {
   uint8_t h, rel, eh, erel, ev;
   if(pending) {
      ev = pending;
      pending = EV_NONE;
      return ev;
   }
   INTCONbits.GIE = 0;
   h = btn_held;
   rel = btn_released;
   btn_released = 0;
   eh = ext_held;
   erel = ext_released;
   ext_released = 0;
   INTCONbits.GIE = 1;

   ev = EV_NONE;
   if(rel) {                        // a press has ended
      // if the main loop was busy the whole press long, judge by its length
      if(!long_sent && rel >= XLONG_T) ev = EV_XLONG;
      else if(!long_sent && rel >= LONG_T) ev = EV_LONG;
      else if(!long_sent && rel >= SHORT_MIN) ev = EV_SHORT;
      long_sent = xlong_sent = 0;
   }
   if(h >= LONG_T && !long_sent) {
      long_sent = 1;
      if(ev) pending = EV_LONG; else ev = EV_LONG;
   }
   if(h >= XLONG_T && !xlong_sent) {
      xlong_sent = 1;
      if(ev) pending = EV_XLONG; else ev = EV_XLONG;
   }
   if(erel) {
      if(erel >= EXT_SHORT_MIN && erel < EXT_SHORT_MAX && !pending) {
         if(ev) pending = EV_EXT_SHORT; else ev = EV_EXT_SHORT;
      }
      ext_long_sent = 0;
   }
   if(eh >= EXT_LONG_T && !ext_long_sent && EXT_KEY_IN) {
      ext_long_sent = 1;
      if(!ev) ev = EV_EXT_LONG; else if(!pending) pending = EV_EXT_LONG;
   }
   if(h == 0 && !rel) long_sent = xlong_sent = 0;
   if(eh == 0 && !erel) ext_long_sent = 0;
   return ev;
}
