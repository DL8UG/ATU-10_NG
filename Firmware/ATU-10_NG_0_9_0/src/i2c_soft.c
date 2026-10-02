#include "board.h"
#include "i2c_soft.h"

#define HALF() __delay_us(10)        // ~45 kHz: robust with the module's pull-ups

void i2c_init(void) {
   uint8_t i;
   I2C_SDA = 1;
   I2C_SCL = 1;
   HALF();
   for(i = 0; i < 9 && !I2C_SDA_IN; i++) {   // clock until the slave lets go
      I2C_SCL = 0;
      HALF();
      I2C_SCL = 1;
      HALF();
   }
   i2c_stop();
}

void i2c_start(void) {
   I2C_SDA = 1;
   I2C_SCL = 1;
   HALF();
   I2C_SDA = 0;
   HALF();
   I2C_SCL = 0;
   HALF();
}

void i2c_stop(void) {
   I2C_SDA = 0;
   HALF();
   I2C_SCL = 1;
   HALF();
   I2C_SDA = 1;
   HALF();
}

uint8_t i2c_write(uint8_t d) {
   uint8_t i, nack;
   for(i = 0; i < 8; i++) {
      I2C_SDA = (d & 0x80) ? 1 : 0;
      HALF();
      I2C_SCL = 1;
      HALF();
      I2C_SCL = 0;
      d <<= 1;
   }
   I2C_SDA = 1;                      // release for the acknowledge
   HALF();
   I2C_SCL = 1;
   HALF();
   nack = I2C_SDA_IN;
   I2C_SCL = 0;
   HALF();
   return nack;
}
