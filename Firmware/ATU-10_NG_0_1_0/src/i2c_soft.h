// Bit-banged I2C master on two open-drain pins (display only)

#ifndef I2C_SOFT_H
#define I2C_SOFT_H

#include <stdint.h>

void i2c_init(void);                 // also frees a slave stuck in a byte
void i2c_start(void);
void i2c_stop(void);
uint8_t i2c_write(uint8_t d);        // 0 = acknowledged

#endif
