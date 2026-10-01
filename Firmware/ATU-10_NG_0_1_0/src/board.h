// ATU-10 board: pin map and low level hardware set-up.
// This is the only place that knows which pin does what.

#ifndef BOARD_H
#define BOARD_H

#include <xc.h>
#include <stdint.h>

#define _XTAL_FREQ 32000000UL

// Button and external interface (both active low)
#define BUTTON_DOWN   (!PORTBbits.RB5)
#define EXT_START     (!PORTDbits.RD1)      // tune request from the transceiver
#define EXT_KEY_IN    (PORTDbits.RD2)       // key line as seen on the pin
#define EXT_KEY_OUT   LATDbits.LATD2        // 0 = tuning in progress (open drain)

// LEDs (active low)
#define LED_RED       LATBbits.LATB4
#define LED_GREEN     LATBbits.LATB3

// Display
#define OLED_PWR      LATAbits.LATA4        // 1 = display powered
#define I2C_SCL       LATAbits.LATA3        // open drain
#define I2C_SDA       LATAbits.LATA2        // open drain
#define I2C_SCL_IN    PORTAbits.RA3
#define I2C_SDA_IN    PORTAbits.RA2

// Relay coils (one pin per relay) and the two drivers that pulse them
#define REL_L_010     LATDbits.LATD7
#define REL_L_022     LATDbits.LATD6
#define REL_L_045     LATDbits.LATD5
#define REL_L_100     LATDbits.LATD4
#define REL_L_220     LATCbits.LATC7
#define REL_L_450     LATCbits.LATC6
#define REL_L_1000    LATCbits.LATC5
#define REL_C_22      LATAbits.LATA5
#define REL_C_47      LATEbits.LATE1
#define REL_C_100     LATAbits.LATA7
#define REL_C_220     LATAbits.LATA6
#define REL_C_470     LATCbits.LATC0
#define REL_C_1000    LATCbits.LATC1
#define REL_C_2200    LATCbits.LATC2
#define REL_C_SW      LATEbits.LATE0        // capacitor on the input or output side
#define REL_TO_GND    LATDbits.LATD3        // 1 = pulse towards ground
#define REL_TO_PLUS_N LATCbits.LATC4        // 0 = pulse towards plus

// ADC channels
#define ADC_CH_FWD    8                     // forward detector, RB0
#define ADC_CH_BAT    9                     // battery voltage / 11, RB1
#define ADC_CH_REV    10                    // reverse detector, RB2

#define WDT_ON()      (WDTCON0bits.SEN = 1)
#define WDT_OFF()     (WDTCON0bits.SEN = 0)

void board_init(void);
void delay_ms(uint16_t ms);      // run-time value, clears the watchdog

#endif
