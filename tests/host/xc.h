// Stand-in for <xc.h> in the host test of the main program (test_app.c):
// the registers the application code touches, as plain variables, and the
// built-ins as hooks into the test's simulated time.

#ifndef FAKE_XC_H
#define FAKE_XC_H

#include <stdint.h>

#define BITS8(name, ...) extern struct name##_t { unsigned __VA_ARGS__; } name
BITS8(LATAbits, LATA2:1, LATA3:1, LATA4:1, LATA5:1, LATA6:1, LATA7:1);
BITS8(LATBbits, LATB3:1, LATB4:1);
BITS8(LATCbits, LATC0:1, LATC1:1, LATC2:1, LATC4:1, LATC5:1, LATC6:1, LATC7:1);
BITS8(LATDbits, LATD2:1, LATD3:1, LATD4:1, LATD5:1, LATD6:1, LATD7:1);
BITS8(LATEbits, LATE0:1, LATE1:1);
BITS8(PORTAbits, RA2:1, RA3:1);
BITS8(PORTBbits, RB5:1);
BITS8(PORTDbits, RD1:1, RD2:1);
BITS8(INTCONbits, GIE:1);
BITS8(IOCBFbits, IOCBF5:1);
BITS8(IOCBNbits, IOCBN5:1);
BITS8(PIE0bits, IOCIE:1, TMR0IE:1);
BITS8(PIR0bits, TMR0IF:1);
BITS8(WDTCON0bits, SEN:1);
BITS8(PCON0bits, nBOR:1, nPOR:1, nRWDT:1, STKOVF:1, STKUNF:1);
extern uint8_t PCON0_reg;
extern uint8_t ANSELA, ANSELD;
#define PCON0 PCON0_reg

void fake_ms(uint32_t ms);           // let simulated time pass
void fake_sleep(void);
void fake_clrwdt(void);              // watchdog cleared, about 1 ms passes
#define CLRWDT()        fake_clrwdt()
#define NOP()
#define SLEEP()         fake_sleep()
#define __delay_ms(x)   fake_ms(x)
#define __delay_us(x)
#define __interrupt()

#endif
