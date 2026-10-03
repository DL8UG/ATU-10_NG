// Number to text, for the display (no printf on the PIC)

#ifndef TEXT_H
#define TEXT_H

#include <stdint.h>

// v / 10^dec with dec decimals into p, right aligned in at least 'width'
// characters; returns the end (the terminating 0). Needs up to 8 bytes.
char *fmt_num(char *p, uint16_t v, uint8_t dec, uint8_t width);

#endif
