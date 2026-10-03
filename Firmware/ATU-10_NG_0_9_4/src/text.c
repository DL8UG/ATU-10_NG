#include "text.h"

char *fmt_num(char *p, uint16_t v, uint8_t dec, uint8_t width) {
   char t[7];                       // "6553.5": 5 digits, the point, a leading 0
   uint8_t n = 0;
   do {
      t[n++] = (char)('0' + v % 10);
      v /= 10;
      if(n == dec) t[n++] = '.';
   } while(v || (dec && n <= dec + 1));
   while(width > n) { *p++ = ' '; width--; }
   while(n) *p++ = t[--n];
   *p = 0;
   return p;
}
