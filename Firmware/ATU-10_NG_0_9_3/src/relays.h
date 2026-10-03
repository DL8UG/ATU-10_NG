// Latching relays of the L network

#ifndef RELAYS_H
#define RELAYS_H

#include <stdint.h>
#include "tune.h"

extern relays_t rel;                 // setting the relays hold

void relays_set(uint8_t l, uint8_t c, uint8_t sw);

#endif
